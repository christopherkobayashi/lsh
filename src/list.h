/* list.h
 *
 * Variable length lists of atoms (or other integers).
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

#ifndef LSH_LIST_H_INCLUDED
#define LSH_LIST_H_INCLUDED

#include "lsh_types.h"

#define CLASS_DECLARE
#include "list.h.x"
#undef CLASS_DECLARE

/* CLASS:
   (class
     (name list_header)
     (vars
       (length simple unsigned)))
*/

/* CLASS:
   (class
     (name int_list)
     (super list_header)
     (vars
       ; This is really of variable size
       (elements var-array int "super.length")))
*/

/* CLASS:
   (class
     (name object_list)
     (super list_header)
     (vars
       ; This is really of variable size
       (elements var-array (object lsh_object) "super.length")))
*/

#define LIST(x) ((x)->elements)
#define LIST_LENGTH(x) (((struct list_header *) (x))->length)

struct list_header *lsh_list_alloc(struct lsh_class *class,
				   unsigned length, size_t element_size);

#define alloc_int_list(n) \
  ((struct int_list *) lsh_list_alloc(&CLASS(int_list), (n), sizeof(int)))
     
struct int_list *make_int_list(unsigned length, ...);

#define alloc_object_list(n) \
  ((struct object_list *) \
   lsh_list_alloc(&CLASS(object_list), (n), sizeof(struct lsh_object *)))

struct object_list *make_object_list(unsigned length, ...);

#endif /* LSH_LIST_H_INCLUDED */
