/* sexp.c
 *
 * S-exp functions specific to lsh.
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2002 Niels M�ller
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

#include "sexp.h"

#include "atoms.h"
#include "format.h"
#include "xalloc.h"

#include "nettle/buffer.h"
#include "nettle/sexp.h"

#include <assert.h>


/* Formatting s-expressions */
struct lsh_string *
lsh_sexp_format(int transport, const char *format, ...)
{
  struct lsh_string *s;
  va_list args;
  unsigned length;
  struct nettle_buffer buffer;

  unsigned (*vformat)(struct nettle_buffer *, const char *, va_list)
    = transport ? sexp_transport_vformat : sexp_vformat;
  
  va_start(args, format);
  length = vformat(NULL, format, args);
  va_end(args);

  s = lsh_string_alloc(length);
  nettle_buffer_init_size(&buffer, s->length, s->data);

  va_start(args, format);
  length = vformat(&buffer, format, args);
  va_end(args);

  assert(length == s->length);

  return s;
}


/* Conversions */

int
lsh_sexp_to_atom(struct sexp_iterator *i)
{
  int type;
  
  if (i->type != SEXP_ATOM || i->display)
    return 0;
  
  type = lookup_atom(i->atom_length, i->atom);

  return sexp_iterator_next(i) ? type : 0;
}

/* Returns 0 or an atom */
int
lsh_sexp_get_type(struct sexp_iterator *i)
{
  if (!sexp_iterator_enter_list(i))
    return 0;

  return lsh_sexp_to_atom(i);
}

struct lsh_string *
lsh_sexp_to_string(struct sexp_iterator *i, struct lsh_string **display)
{
  struct lsh_string *s;
  
  if (i->type != SEXP_ATOM)
    return NULL;

  if (display)
    {
      *display = i->display
	? ssh_format("%ls", i->display_length, i->display) : NULL;
    }
  else if (i->display)
    return NULL;
  
  s = ssh_format("%ls", i->atom_length, i->atom);

  if (sexp_iterator_next(i))
    return s;
  
  lsh_string_free(s);
  if (display)
    {
      lsh_string_free(*display);
      *display = NULL;
    }
  return NULL;
}

/* Copy the current expression. */
struct lsh_string *
lsh_sexp_copy(struct sexp_iterator *i)
{
  unsigned length;
  const UINT8 *subexpr = sexp_iterator_subexpr(i, &length);

  return subexpr ? ssh_format("%ls", length, subexpr) : NULL;
}
