/* lsh.h
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

#ifndef LSH_H_INCLUDED
#define LSH_H_INCLUDED

#include "lsh_types.h"

#include <stdlib.h>

/* Generic object */

#define LSH_ALLOC_HEAP 0
#define LSH_ALLOC_STATIC 1
#define LSH_ALLOC_STACK 2

struct lsh_class;

struct lsh_object
{
  /* Objects are chained together, for the sweep phase of the gc. */
  struct lsh_object *next; 
  struct lsh_class *isa;
  
  char alloc_method;
  char marked;
  char dead;
};

/* NOTE: Static objects have a NULL isa-pointer, and can therefore not
 * contain any references to non-static objects. This could be fixed,
 * by using an argument to the STATIC_HEADER macro, but then one must
 * use some class for lsh_class objects... */

#define STATIC_HEADER { NULL, NULL, LSH_ALLOC_STATIC, 0, 0 }
#define STACK_HEADER  { NULL, NULL, LSH_ALLOC_STACK, 0, 0 }

struct lsh_class
{
  struct lsh_object super;
  struct lsh_class *super_class;
  char *name;  /* For debugging */

  size_t size;
  
  void (*mark_instance)(struct lsh_object *instance,
			void (*mark)(struct lsh_object *o));
  void (*free_instance)(struct lsh_object *instance);

  /* Particular classes may add their own methods here */
};

#define MARK_INSTANCE(c, i, f) ((c)->mark_instance((i), (f)))
#define FREE_INSTANCE(c, i) ((c)->free_instance((i)))

#define CLASS(c) (c##_class)

#if DEBUG_ALLOC

struct lsh_string_header
{
  int magic; /* For a sentinel value */
};

#else   /* !DEBUG_ALLOC */

struct lsh_string_header {};

#endif  /* !DEBUG_ALLOC */

struct lsh_string
{
  struct lsh_string_header header;
  
  UINT32 sequence_number;
  /* NOTE: The allocated size may be larger than the string length. */
  UINT32 length; 
  UINT8 data[1];
};


/* Forward declarations of various structures */

/* abstract_crypto.h */

/* connection.h */
struct ssh_connection;

/* io.h */
struct lsh_fd;
struct address_info;
struct listen_value;

/* list.h */
struct int_list;
struct object_list;

/* randomness.h */
struct randomness;

/* sexp.h */
struct sexp;
struct sexp_iterator;

/* userauth.h */
struct lsh_user;

#endif /* LSH_H_INCLUDED */
