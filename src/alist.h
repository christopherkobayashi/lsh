/* alist.h
 *
 * Associate atoms with objects (or functions) .
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LSH_ALIST_H_INCLUDED
#define LSH_ALIST_H_INCLUDED

#include "lsh_types.h"

#define ALIST_USE_SIZE 0
/* Abstract interface allows for multiple implementations ("real"
 * alists, linear tables, hash tables */
struct alist
{
  struct lsh_object header;

#if ALIST_USE_SIZE
  int size; /* Number of associations with non-zero values */
  int * (*keys)(struct alist *self);
#endif
  
  void * (*get)(struct alist *self, int atom);
  void (*set)(struct alist *self, int atom, void *value);

};

#define ALIST_GET(alist, atom) ((alist)->get((alist), (atom)))
#define ALIST_SET(alist, atom, value) ((alist)->set((alist), (atom), (value)))

#if ALIST_USE_SIZE
#define ALIST_KEYS(alist) ((alist)->keys((alist)))
#endif

/* n is the number of pairs. The argument list should be terminated
 * with -1, for sanity checks. */

struct alist *make_linear_alist(int n, ...);
struct alist *make_linked_alist(int n, ...);

#define make_alist make_linear_alist

#endif /* LSH_ALIST_H_INCLUDED */
