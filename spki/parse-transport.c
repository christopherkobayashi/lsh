/* parse-transport.c */

/* libspki
 *
 * Copyright (C) 2002 Niels Möller
 *  
 * The libspki library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The libspki library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the libspki library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02111-1301, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "parse.h"

#include <nettle/sexp.h>

enum spki_type
spki_transport_iterator_first(struct spki_iterator *i,
			      unsigned length, uint8_t *expr)
{
  i->start = 0;
  if (sexp_transport_iterator_first(&i->sexp, length, expr))
    return spki_parse_type(i);

  return spki_parse_fail(i);
}
