/* sexp_commands.c
 *
 * Reading and writing of s-expressions.
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1999 Bal�zs Scheidler, Niels M�ller
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

#include "sexp_commands.h"

#include "format.h"
#include "io.h"
#include "read_file.h"
#include "werror.h"
#include "xalloc.h"

#include <assert.h>

/* Forward declarations */
static struct catch_command catch_sexp_exceptions;
#define CATCH_SEXP (&catch_sexp_exceptions.super.super.super)

static struct read_sexp_command read_sexp;
#define READ_SEXP (&read_sexp.super.super)

#define GABA_DEFINE
#include "sexp_commands.h.x"
#undef GABA_DEFINE

#include "sexp_commands.c.x"

/* (write out sexp)
 *
 * Prints the sexp to tha abstract_write OUT. Returns the sexp. */

/* GABA:
   (class
     (name sexp_print_to)
     (super command)
     (vars
       (format . int)
       (dest object abstract_write)))
*/

static void
do_sexp_print(struct command *s,
	      struct lsh_object *a,
	      struct command_continuation *c,
	      struct exception_handler *e UNUSED)
{
  CAST(sexp_print_to, self, s);
  CAST_SUBTYPE(sexp, o, a);

  A_WRITE(self->dest, sexp_format(o, self->format, 0));
  if (self->format != SEXP_CANONICAL)
    A_WRITE(self->dest, ssh_format("\n"));

  COMMAND_RETURN(c, a);
}

struct command *
make_sexp_print_to(int format, struct abstract_write *dest)
{
  NEW(sexp_print_to, self);
  self->super.call = do_sexp_print;
  self->format = format;
  self->dest = dest;

  return &self->super;
}

struct lsh_object *
do_sexp_print_simple(struct command_simple *s,
		     struct lsh_object *a)
{
  CAST(sexp_print_command, self, s);
  CAST_SUBTYPE(abstract_write, dest, a);

  return &make_sexp_print_to(self->format, dest)->super;
}

struct command_simple *
make_sexp_print_command(int format)
{
  NEW(sexp_print_command, self);
  self->super.super.call = do_call_simple_command;
  self->super.call_simple = do_sexp_print_simple;
  self->format = format;

  return &self->super;
}

/* GABA:
   (class
     (name sexp_print_raw_hash_to)
     (super command)
     (vars
       (algorithm object hash_algorithm)
       (dest object abstract_write)))
*/

static void
do_print_raw_hash_to(struct command *s,
		     struct lsh_object *a,
		     struct command_continuation *c,
		     struct exception_handler *e UNUSED)
{
  CAST(sexp_print_raw_hash_to, self, s);
  CAST_SUBTYPE(sexp, o, a);

  struct lsh_string *canonical = sexp_format(o, SEXP_CANONICAL, 0);
  struct hash_instance *hash = MAKE_HASH(self->algorithm);
  struct lsh_string *digest = lsh_string_alloc(hash->hash_size);

  HASH_UPDATE(hash, canonical->length, canonical->data);
  HASH_DIGEST(hash, digest->data);
  
  lsh_string_free(canonical);
  KILL(hash);

  A_WRITE(self->dest, ssh_format("%lxfS\n", digest));

  COMMAND_RETURN(c, a);
}

struct command *
make_sexp_print_raw_hash_to(struct hash_algorithm *algorithm,
			    struct abstract_write *dest)
{
  NEW(sexp_print_raw_hash_to, self);
  self->super.call = do_print_raw_hash_to;
  self->algorithm = algorithm;
  self->dest = dest;

  return &self->super;
}

static struct lsh_object *
collect_print_raw_hash_2(struct collect_info_2 *info,
			 struct lsh_object *a,
			 struct lsh_object *d)
{
  CAST_SUBTYPE(hash_algorithm, algorithm, a);
  CAST_SUBTYPE(abstract_write, dest, d);

  assert(!info->next);
  
  return &make_sexp_print_raw_hash_to(algorithm, dest)->super;
}

struct collect_info_2 collect_info_print_raw_2 =
STATIC_COLLECT_2_FINAL(collect_print_raw_hash_2);

struct collect_info_1 sexp_print_raw_hash =
STATIC_COLLECT_1(&collect_info_print_raw_2);

struct command *
make_sexp_print_raw_hash(struct hash_algorithm *algorithm)
{
  CAST_SUBTYPE(command, print,
	       make_collect_state_1(&sexp_print_raw_hash, &algorithm->super));
  
  return print;
}

/* GABA:
   (class
     (name sexp_parser)
     (super abstract_write)
     (vars
       (style . int)
       (c object command_continuation)
       (e object exception_handler)))
*/

static const struct exception
sexp_syntax_exception = STATIC_EXCEPTION(EXC_SEXP_SYNTAX,
					 "Sexp syntax error");

static const struct exception
sexp_eof_exception = STATIC_EXCEPTION(EXC_SEXP_EOF, "All sexps read");

static void
do_sexp_parse_once(struct abstract_write *s, struct lsh_string *input)
{
  CAST(sexp_parser, self, s);
  struct simple_buffer buffer;
  struct sexp *expr;
  
  assert(input);

  simple_buffer_init(&buffer, input->length, input->data);

  expr = sexp_parse(self->style, &buffer);
  if (!expr)
    EXCEPTION_RAISE(self->e, &sexp_syntax_exception);
  else
    COMMAND_RETURN(self->c, expr);

  lsh_string_free(input);
}

static void
do_sexp_parse_many(struct abstract_write *s, struct lsh_string *input)
{
  CAST(sexp_parser, self, s);
  struct simple_buffer buffer;
  const struct exception *e = &sexp_eof_exception;
    
  assert(input);

  simple_buffer_init(&buffer, input->length, input->data);

  while (!parse_eod(&buffer))
    {
      struct sexp *expr = sexp_parse(self->style, &buffer);
      if (!expr)
	{
	  e = &sexp_syntax_exception;
	  break;
	}
      else
	COMMAND_RETURN(self->c, expr);
    }

  EXCEPTION_RAISE(self->e, e);
  lsh_string_free(input);
}

static struct abstract_write *
make_sexp_parser(int style, int goon,
		 struct command_continuation *c,
		 struct exception_handler *e)
{
  NEW(sexp_parser, self);
  self->super.write = goon ? do_sexp_parse_many : do_sexp_parse_once;
  self->style = style;
  self->c = c;
  self->e = e;

  return &self->super;
}

#define SEXP_BUFFER_SIZE 1024

void
do_read_sexp(struct command *s,
	     struct lsh_object *a,
	     struct command_continuation *c,
	     struct exception_handler *e)
{
  CAST(read_sexp_command, self, s);
  CAST(lsh_fd, fd, a);

  trace("do_read_sexp\n");
    
  assert(fd);

  io_read(fd,
	  make_buffered_read
	  (SEXP_BUFFER_SIZE,
	   make_read_file
	   (make_sexp_parser(self->format,
			     self->goon,
			     c, e),
	    self->max_size)),
	  NULL);
}

struct command *
make_read_sexp_command(int format, int goon, UINT32 max_size)
{
  NEW(read_sexp_command, self);

  trace("make_read_sexp_command\n");
  
  self->super.call = do_read_sexp;
  self->format = format;
  self->goon = goon;
  self->max_size = max_size;

  return &self->super;
}

static struct catch_command catch_sexp_exceptions
= STATIC_CATCH_COMMAND(EXC_ALL, EXC_SEXP_EOF, 1);

/* Arbitrary limit on file size. */
#define MAX_SEXP_SIZE 10000

static struct read_sexp_command read_sexp
= STATIC_READ_SEXP(SEXP_TRANSPORT, 1, MAX_SEXP_SIZE);

/* GABA:
   (expr
     (name for_sexp)
     (params
       (handler object command))
     (expr
       (lambda (proc)
         (catch_sexp handler
	             (lambda (file)
		       (proc (read_sexp file)))))))
*/

DEFINE_COMMAND_SIMPLE(for_sexp_command, a)
{
  CAST_SUBTYPE(command, handler, a);
  return for_sexp(handler);
}
