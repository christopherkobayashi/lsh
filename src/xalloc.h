/* xalloc.h
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LSH_XALLOC_H_INCLUDED
#define LSH_XALLOC_H_INCLUDED

#include "lsh_types.h"
#include <stdlib.h>

/* Allocation */

/* The memory allocation model (for strings) is as follows:
 *
 * Packets are allocated when the are needed. A packet may be passed
 * through a chain of processing functions, until it is finally
 * discarded or transmitted, at which time it is deallocated.
 * Processing functions may deallocate their input packets and
 * allocate fresh packets to pass on; therefore, any data from a
 * packet that is needed later must be copied into some other storage.
 *
 * At any time, each packet is own by a a particular processing
 * function. Pointers into a packet are valid only while you own it.
 * */

struct lsh_string *lsh_string_alloc(UINT32 size);
void lsh_string_free(struct lsh_string *packet);

struct lsh_object *lsh_object_alloc(struct lsh_class *class);
void lsh_object_free(struct lsh_object *o);

void *lsh_space_alloc(size_t size);
void lsh_space_free(void *p);


#ifdef DEBUG_ALLOC

struct lsh_object *lsh_object_check(struct lsh_object *instance,
				    struct lsh_class *class);
struct lsh_object *lsh_object_check_subtype(struct lsh_object *instance,
					    struct lsh_class *class);

#if 0
#define MDEBUG(x) lsh_object_check((x), sizeof(*(x)))
#define MDEBUG_SUBTYPE(x) lsh_object_check_subtype((x), sizeof(*(x)))

#define CHECK_TYPE(c, i) lsh_object_check((c), (struct lsh_object *) (i))
#define CHECK_SUBTYPE(c, i) \
  lsh_object_check_subtype((c), (struct lsh_object *) (i))
#endif

#define CAST(class, var, o) \
     struct class *(var) = (struct class *) \
  (lsh_check_object(class##_class, (struct lsh_object *) (o))

#define CAST_SUBTYPE(class, var, o) \
     struct class *(var) = (struct class *) \
  (lsh_check_object_subtype(class##_class, (struct lsh_object *) (o))
   

#else   /* !DEBUG_ALLOC */

#if 0
#define MDEBUG(x)
#define MDEBUG_SUBTYPE(x)
#endif

#define CAST(class, var, o) \
   struct class *(var) = (struct class *) (o)

#define CAST_SUBTYPE(class, var, o) CAST(class, var, o)
   
#endif  /* !DEBUG_ALLOC */

#define NEW(class, var) struct (class) * (var) = lsh_object_alloc(class##_class)
#define NEW_SPACE(x) ((x) = lsh_space_alloc(sizeof(*(x))))

#include "gc.h"
#define KILL(x) gc_kill((x))

#endif /* LSH_XALLOC_H_INCLUDED */
