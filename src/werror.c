/* werror.c
 *
 *
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998 Niels M�ller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "werror.h"

#include "charset.h"
#include "format.h"  /* For format_size_in_decimal() */
#include "gc.h"
#include "io.h"
#include "parse.h"
#include "xalloc.h"

#include <assert.h>
/* #include <stdio.h> */
#include <stdarg.h>
#include <ctype.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif

int debug_flag = 0;
int quiet_flag = 0;
int verbose_flag = 0;
int syslog_flag = 0;

int error_fd = STDERR_FILENO;

#define BUF_SIZE 500
static UINT8 error_buffer[BUF_SIZE];
static UINT32 error_pos = 0;

static int (*error_write)(int fd, UINT32 length, UINT8 *data) = write_raw;

#ifdef HAVE_SYSLOG
static int write_syslog(int fd UNUSED, UINT32 length, UINT8 *data)
{
  UINT8 string_buffer[BUF_SIZE];
  
  /* Make sure the message is properly terminated with \0. */
  snprintf(string_buffer, (BUF_SIZE > length) ? BUF_SIZE : length, "%s", data);

  /* FIXME: Should we use different log levels for werror, verbose and
   * debug? */
  
  syslog(LOG_NOTICE, "%s", string_buffer);

  return 0; /* FIXME */
}

void set_error_syslog(void)
{
  error_write = write_syslog;
  error_fd = -1;
}
#endif /* HAVE_SYSLOG */

static int write_ignore(int fd UNUSED,
			UINT32 length UNUSED, UINT8 *data UNUSED)
{
  return 1;
}
void set_error_stream(int fd, int with_poll)
{
  error_fd = fd;

  error_write = with_poll ? write_raw_with_poll : write_raw;
}

void set_error_ignore(void)
{
  error_write = write_ignore;
}

#define WERROR(l, d) (error_write(error_fd, (l), (d)))

static void werror_flush(void)
{
  if (error_pos)
    {
      WERROR(error_pos, error_buffer);
      error_pos = 0;
    }
}

static void werror_putc(UINT8 c)
{
  if (error_pos == BUF_SIZE)
    werror_flush();

  error_buffer[error_pos++] = c;
}

static void werror_write(UINT32 length, UINT8 *msg)
{
  if (error_pos + length <= BUF_SIZE)
    {
      memcpy(error_buffer + error_pos, msg, length);
      error_pos += length;
#if 0
      if (length && (msg[length-1] == '\n'))
	werror_flush();
#endif
    }
  else
    {
      werror_flush();
      WERROR(length, msg);
    }
}

static void werror_cstring(char *s) { werror_write(strlen(s), s); }

static void werror_bignum(mpz_t n, int base)
{
  UINT8 *s = alloca(mpz_sizeinbase(n, base) + 2);
  mpz_get_str(s, 16, n);

  werror_cstring(s);
}

static void werror_decimal(UINT32 n)
{
  unsigned length = format_size_in_decimal(n);
  char *buffer = alloca(length);
  unsigned i;
  
  for (i = 0; i<length; i++)
    buffer[length - i - 1] = '0' + n % 10;
  
  werror_write(length, buffer);
}

static unsigned format_size_in_hex(UINT32 n);

static void werror_hex_digit(unsigned digit)
{
  werror_putc("0123456789abcdef"[digit]);
}

static void werror_hex_putc(UINT8 c)
{
  werror_hex_digit(c / 16);
  werror_hex_digit(c % 16);
}

static void werror_hex(UINT32 n)
{
  unsigned left = 8;
  
  while ( (left > 1)
	  && (n & 0xf0000000UL))
    {
      left --;
      n <<= 4;
    }
		    
  while (left--)
    {
      werror_hex_digit((n >> 28) & 0xf);
      n <<= 4;
    }
}

static void werror_hexdump(UINT32 length, UINT8 *data)
{
  UINT32 i;
  werror("(size %i = 0x%xi)", length, length);

  for(i=0; i<length; i++)
  {
    if (! (i%16))
      {
	unsigned j = format_size_in_hex(i);

	werror_cstring("\n0x");

	for ( ; j < 8; j++)
	  werror_putc('0');
	
	werror_hex(i);
	werror_cstring(": ");
      }

    werror_hex_putc(data[i]);
  }
  werror_putc('\n');
}

static void werror_paranoia_putc(UINT8 c)
{
  switch (c)
    {
    case '\\':
      werror_cstring("\\\\");
      break;
    case '\r':
      /* Ignore */
      break;
    default:
      if (!isprint(c))
	{
	  werror_putc('\\');
	  werror_hex_putc(c);
	  break;
	}
      /* Fall through */
    case '\n':
      werror_putc(c);
      break;
    }
}

void werror_vformat(const char *f, va_list args)
{
  while (*f)
    {
      if (*f == '%')
	{
	  int do_hex = 0;
	  int do_free = 0;
	  int do_paranoia = 0;
	  int do_utf8 = 0;

	  while (*++f)
	    switch (*f)
	      {
	      case 'x':
		do_hex = 1;
		break;
	      case 'f':
		do_free = 1;
		break;
	      case 'p':
		do_paranoia = 1;
		break;
	      case 'u':
		do_utf8 = 1;
		break;
	      case 'h':
		do_hex = 1;
		break;
	      default:
		goto end_options;
	      }
	end_options:
	  switch(*f++)
	    {
	    case '%':
	      werror_putc(*f);
	      break;
	    case 'i':
	      (do_hex ? werror_hex : werror_decimal)(va_arg(args, UINT32));
	      break;
	    case 'c':
	      werror_putc(va_arg(args, int));
	      break;
	    case 'n':
	      werror_bignum(va_arg(args, MP_INT *), do_hex ? 16 : 10);
	      break;
	    case 'z':
	      {
		char *s = va_arg(args, char *);

		if (do_hex)
		  werror_hexdump(strlen(s), s);

		while (*s)
		  (do_paranoia ? werror_paranoia_putc : werror_putc)(*s++);
	      }
	      break;
	    case 's':
	      {
		UINT32 length = va_arg(args, UINT32);
		UINT8 *s = va_arg(args, UINT8 *);

		struct lsh_string *u = NULL; 

		if (do_utf8 && !local_is_utf8())
		  {
		    u = low_utf8_to_local(length, s, 0);
		    if (!u)
		      {
			werror_cstring("<Invalid utf-8 string>");
			break;
		      }
		    length = u->length;
		    s = u->data;
		  }
		if (do_hex)
		  {
		    assert(!do_paranoia);
		    werror_hexdump(length, s);
		  }
		else if (do_paranoia)
		  {
		    UINT32 i;
		    for (i=0; i<length; i++)
		      werror_paranoia_putc(*s++);
		  }
		else
		  werror_write(length, s);

		if (u)
		  lsh_string_free(u);
	      }
	      break;
	    case 'S':
	      {
		struct lsh_string *s = va_arg(args, struct lsh_string *);

		if (do_utf8)
		  {
		    s = utf8_to_local(s, 0, do_free);
		    if (!s)
		      {
			werror_cstring("<Invalid utf-8 string>");
			break;
		      }
		    do_free = 1;
		  }
		if (do_hex)
		  {
		    assert(!do_paranoia);
		    werror_hexdump(s->length, s->data);
		  }
		else if (do_paranoia)
		  {
		    UINT32 i;
		    for (i=0; i<s->length; i++)
		      werror_paranoia_putc(s->data[i]);
		  }
		else
		  werror_write(s->length, s->data);

		if (do_free)
		  lsh_string_free(s);
	      }
	      break;

	    default:
	      fatal("werror_vformat: bad format string");
	      break;
	    }
	}
      else
	werror_putc(*f++);
    }
}

void werror(const char *format, ...) 
{
  va_list args;

  if (!quiet_flag)
    {
      va_start(args, format);
      werror_vformat(format, args);
      va_end(args);
      werror_flush();
    }
}

void debug(const char *format, ...) 
{
  va_list args;

  if (debug_flag)
    {
      va_start(args, format);
      werror_vformat(format, args);
      va_end(args);
      werror_flush();
    }
}

void verbose(const char *format, ...) 
{
  va_list args;

  if (verbose_flag)
    {
      va_start(args, format);
      werror_vformat(format, args);
      va_end(args);
      werror_flush();
    }
}

void fatal(const char *format, ...) 
{
  va_list args;

  va_start(args, format);
  werror_vformat(format, args);
  va_end(args);
  werror_flush();

  abort();
}

static unsigned format_size_in_hex(UINT32 n)
{
  int i;
  int e;
  
  /* Table of 16^(2^n) */
  static const UINT32 powers[] = { 0x10UL, 0x100UL, 0x10000UL };

#define SIZE (sizeof(powers) / sizeof(powers[0])) 

  /* Determine the smallest e such that n < 10^e */
  for (i = SIZE - 1 , e = 0; i >= 0; i--)
    {
      if (n >= powers[i])
	{
	  e += 1UL << i;
	  n /= powers[i];
	}
    }

#undef SIZE
  
  return e+1;
}

