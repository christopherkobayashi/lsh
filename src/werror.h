/* werror.h
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

#ifndef LSH_ERROR_H_INCLUDED
#define LSH_ERROR_H_INCLUDED

#include "bignum.h"

/* Global variables */
extern int debug_flag;
extern int quiet_flag;
extern int verbose_flag;

void set_error_stream(int fd, int with_poll);

#ifdef HAVE_SYSLOG
void set_error_syslog(void);
#endif

/* Format specifiers:
 *
 * %%  %-charqacter
 * %i  UINT32
 * %c  int, interpreted as a single character to output
 * %n  mpz_t
 * %z  NUL-terminated string
 * %s  UINT32 length, UINT8 *data
 * %S  lsh_string *s
 *
 * Modifiers:
 *
 * x  hexadecimal output
 * f  Consume (and free) the input string
 * p  Filter out dangerous control characters
 * u  Input is in utf-8; convert to local charset
 */


void werror_vformat(const char *f, va_list args);

void werror(const char *format, ...);
void debug(const char *format, ...);
void verbose(const char *format, ...);

void fatal(const char *format, ...) NORETURN;

#endif /* LSH_ERROR_H_INCLUDED */
