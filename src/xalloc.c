/* xalloc.c
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

#include "list.h"
#include "werror.h"


#if DEBUG_ALLOC

/* There are two sets of allocation functions: Low level allocation *
 * that can allocate memory for any purpose, and object allocators
 * that assume that the allocated object begins with a type field. */

/* UNIT should be a type of a size that is a multiple of the alignment
 * requirement of the machine. */

/* NOTE: The code breaks horribly if UNIT is of the wrong size. But it
 * doesn't matter much if we guess wrong on some platforms, as this
 * affects only optional debug code. */

#define UNIT unsigned long 

#define SIZE_IN_UNITS(x) (((x) + sizeof(UNIT)-1) / sizeof(UNIT))

void *
debug_malloc(size_t real_size)
{
  static int count = 4711;
  UNIT *res;
  UNIT size = SIZE_IN_UNITS(real_size);
  
  res = malloc((size + 3)*sizeof(UNIT));

  if (!res)
    return NULL;

  res += 2;
  
  res[-2] = count;
  res[-1] = real_size;
  res[size] = ~count;
  count++;

  return (void *) res;
}

void
debug_free(const void *m)
{
  if (m)
    {
      UNIT *p = (UNIT *) m;
      UNIT real_size = p[-1];
      UNIT size = SIZE_IN_UNITS(real_size);
      
      if (~p[-2] != p[size])
	fatal("Memory corrupted!\n");
      
      p[-2] = p[size] = 0;
      
      free(p-2);
    }
}

void *
debug_realloc(void *m, size_t real_size)
{
  if (!real_size)
    {
      debug_free(m);
      return NULL;
    }
  else if (!m)
    return debug_malloc(real_size);
  else
    {
      UNIT *p = (UNIT *) m;
      UNIT *res;
      UNIT old_size = p[-1];
      UNIT size = SIZE_IN_UNITS(real_size);
      
      if (~p[-2] != p[SIZE_IN_UNITS(old_size)])
	fatal("Memory corrupted!\n");

      res = realloc(p-2, (size + 3) * sizeof(UNIT)); 
      if (!res)
	return NULL;

      res += 2;
      res[-1] = real_size;
      res[size] = ~res[-2];

      return res;      
    }
}
#else /* !DEBUG_ALLOC */

/* Simple hack to make xalloc and xrealloc accessible, under different
   official names, to avoid trivial wrapper functions for
   lsh_space_(re)alloc. */

#define xalloc lsh_space_alloc
#define xrealloc lsh_space_realloc
#define static

#endif /* !DEBUG_ALLOC */

static void *xalloc(size_t size)
{
  void *res = lsh_malloc(size);
  if (!res)
    fatal("Virtual memory exhausted");

  /* NOTE: The gc can't handle uninitialized pointers. The simple way
   * is to make sure there are no uninitialized pointers, by
   * zero-filling all memory as it is allocated, even though this is
   * mostly redundant since the constructor functions ought to
   * initialize all fields explicitly. */
  memset(res, 0, size);

  return res;
}

static void *xrealloc(void *p, size_t size)
{
  void *res = lsh_realloc(p, size);
  if (size && !res)
    fatal("Virtual memory exhausted");

  return res;
}


/* General allocator that can handle variable size objects */
struct lsh_object *
lsh_var_alloc(struct lsh_class *class,
	      size_t size)
{
  struct lsh_object *instance = xalloc(size);

  instance->isa = class;
  instance->alloc_method = LSH_ALLOC_HEAP;

  gc_register(instance);

  return instance;
}

struct lsh_object *
lsh_object_alloc(struct lsh_class *class)
{
  return lsh_var_alloc(class, class->size);
}

struct lsh_object *
lsh_var_clone(struct lsh_object *o, size_t size)
{
  struct lsh_object *i = xalloc(size);

  /* Copy header and all instance variables. Note that the header is
   * now invalid, as the next pointer can't be copied directly. This
   * is fixed by the gc_register call below. */
  memcpy(i, o, size);

  i->alloc_method = LSH_ALLOC_HEAP;
  gc_register(i);

  return i;
}

struct lsh_object *
lsh_object_clone(struct lsh_object *o)
{
  return lsh_var_clone(o, o->isa->size);
}


/* Should be called *only* by the gc */
void
lsh_object_free(const struct lsh_object *o)
{
  if (!o)
    return;
  
  if (o->alloc_method != LSH_ALLOC_HEAP)
    fatal("lsh_object_free: Object not allocated on the heap!\n");
  
  lsh_free(o);
}

#if DEBUG_ALLOC
struct lsh_object *
lsh_object_check(struct lsh_class *class,
		 struct lsh_object *instance)
{
  if (!instance)
    return instance;
  
  if (instance->marked)
    fatal("lsh_object_check: Unexpected marked object!\n");

  if (instance->dead)
    fatal("lsh_object_check: Reference to dead object!\n");

  if ( (instance->alloc_method == LSH_ALLOC_HEAP)
       && (instance->isa != class))
    fatal("lsh_object_check: Type error, expected %z, got %z!\n",
	  class->name, instance->isa->name);

  return instance;
}

struct lsh_object *
lsh_object_check_subtype(struct lsh_class *class,
			 struct lsh_object *instance)
{
  struct lsh_class *type;
  
  if (!instance)
    return instance;

  if (instance->marked)
    fatal("lsh_object_check: Unexpected marked object!\n");

  if (instance->dead)
    fatal("lsh_object_check: Reference to dead object!\n");

  /* Only heap allocated objects have a valid isa-pointer */
  switch (instance->alloc_method)
    {
    case LSH_ALLOC_STATIC:
      return instance;
    case LSH_ALLOC_HEAP:
      break;
    default:
      fatal("lsh_object_check_subtype: Memory corrupted!\n");
    }
  
  for (type = instance->isa; type; type=type->super_class)
    if (type == class)
      return instance;

  fatal("lsh_object_check_subtype: Type error, expected %z, got %z!\n",
	class->name, instance->isa->name);
}

void *lsh_space_alloc(size_t size)
{
  UNIT *p = xalloc(size + sizeof(UNIT));

  *p = -1919;

  return (void *) (p + 1);
}

void lsh_space_free(const void *p)
{
  UNIT *m;
  
  if (!p)
    return;

  m = (UNIT *) p;
  
  if (m[-1] != (UNIT) -1919)
    fatal("lsh_space_free: Type error!\n");

  lsh_free(m-1);
}

void *lsh_space_realloc(void *p, size_t size)
{
  if (!size)
    {
      lsh_space_free(p);
      return NULL;
    }
  else if (!p)
    return lsh_space_alloc(size);
  else
    {
      UNIT *m = p;
      if (m[-1] != (UNIT) -1919)
	fatal("lsh_space_realloc: Type error!\n");

      m = xrealloc(m - 1, size + sizeof(UNIT));
      return m + 1;
    }
}
#endif /* DEBUG_ALLOC */
