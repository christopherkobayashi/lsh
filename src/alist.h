/* alist.h
 *
 * Associate atoms with objects (or functions) .
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998 Niels Möller
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301  USA
 */

#ifndef LSH_ALIST_H_INCLUDED
#define LSH_ALIST_H_INCLUDED

#include <stdarg.h>

#include "lsh.h"

/* Forward declaration */
struct alist;

#define GABA_DECLARE
#include "alist.h.x"
#undef GABA_DECLARE

/* Abstract interface allows for multiple implementations ("real"
 * alists, linear tables, hash tables */

/* GABA:
   (meta
     (name alist)
     (methods
       "struct lsh_object * (*get)(const struct alist *self, int atom)"
       "void (*set)(struct alist *self, int atom, struct lsh_object *value)"))
*/

/* GABA:
   (class
     (name alist)
     (meta alist)
     ; Only subclasses has methods 
     (methods NULL NULL))
*/

#define ALIST_CLASS(l) ((struct alist_meta *) ((l)->super.isa))

#define ALIST_GET(alist, atom) \
     (ALIST_CLASS(alist)->get((alist), (atom)))

#define ALIST_SET(alist, atom, value) \
     (ALIST_CLASS(alist)->set((alist), (atom), (value)))

struct alist *alist_addv(struct alist *a, unsigned n, va_list args);

/* n is the number of pairs. The argument list should be terminated
 * with -1, for sanity checks. */
     
struct alist *make_linear_alist(unsigned n, ...);
struct alist *make_linked_alist(unsigned n, ...);

#define make_alist make_linear_alist

struct alist *
alist_select_l(const struct alist *src, unsigned n, ...);

#endif /* LSH_ALIST_H_INCLUDED */
