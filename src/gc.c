/* gc.c
 *
 * Simple mark&sweep garbage collector.
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

#include "gc.h"

#include "io.h"
#include "resource.h"
#include "werror.h"
#include "xalloc.h"

#include <assert.h>

/* Global variables */
static struct lsh_object *all_objects = NULL;
static unsigned number_of_objects = 0;
static unsigned live_objects = 0;
static struct resource_list *root_set = NULL;

static int gc_scheduled = 0;

#if DEBUG_ALLOC
static void sanity_check_object_list(void)
{
  unsigned i = 0;
  struct lsh_object *o;

#if 0
  wwrite("sanity_check_object_list: Objects on list:\n");
  for(o = all_objects; o; o = o->next)
    werror("  %xi, class: %z\n", (UINT32) o, o->isa ? o->isa->name : "UNKNOWN");
#endif
  
  for(o = all_objects; o; o = o->next)
    i++;

  if (i != number_of_objects)
    fatal("sanity_check_object_list: Found %i objects, expected %i.\n",
	  i, number_of_objects);
}
#else
#define sanity_check_object_list()
#endif

/* FIXME: This function recurses heavily. One could use some trickery
 * to emulate tail recursion, which would help marking linked lists.
 * Or one could use some more efficient datastructures than the C
 * stack for keeping track of the marked but not yet traced objects.
 * Or use something more sophisticated, like pointer reversal. */
static void gc_mark(struct lsh_object *o)
{
  if (!o)
    return;
  
  switch(o->alloc_method)
    {
    case LSH_ALLOC_STACK:
      fatal("gc_mark: Unexpected stack object!\n");

    case LSH_ALLOC_HEAP:
      if (o->marked)
	return;
      o->marked = 1;
      /* Fall through */
    case LSH_ALLOC_STATIC:
      /* Can't use mark bit on static objects, as there's no way to
       * reset all the bits */
      assert(!o->dead);
      {
	struct lsh_class *class;

#if 0
	debug("gc_mark: Marking object of class '%z'\n",
	      o->isa ? o->isa->name : "UNKNOWN");
#endif
	
	for (class = o->isa; class; class = class->super_class)
	  {
	    if (class->mark_instance)
	      MARK_INSTANCE(class, o, gc_mark);
	  }
      }
      break;
    default:
      fatal("gc_mark: Memory corrupted!\n");
    }
}

static void gc_sweep(void)
{
  struct lsh_object *o;
  struct lsh_object **o_p;

  live_objects = 0;
  
  for(o_p = &all_objects; (o = *o_p); )
    {
      if (o->marked)
	{
	  /* Paralyze the living... */
	  live_objects++;
	  o->marked = 0;
	}
      else
	{
	  /* ... and resurrect the dead. */
	  struct lsh_class *class;

#if 0
	  debug("gc_sweep: Freeing object of class '%z'\n",
		o->isa->name);
#endif  
	  for (class = o->isa; class; class = class->super_class)
	    if (class->free_instance)
	      FREE_INSTANCE(class, o);

	  *o_p = o->next;
	  number_of_objects--;
	  
	  lsh_object_free(o);
	  continue;
	}
      o_p = &o->next;
    }
  assert(live_objects == number_of_objects);
}

static void
do_gc(struct lsh_callback *s UNUSED)
{
  assert(gc_scheduled);
  gc_scheduled = 0;

  gc();
}

struct lsh_callback
gc_callback = { STATIC_HEADER, do_gc };

void
gc_register(struct lsh_object *o)
{
  sanity_check_object_list();

  o->marked = o->dead = 0;
  o->next = all_objects;
  all_objects = o;

  number_of_objects++;

  if (!gc_scheduled && (number_of_objects > 100 + 2 * live_objects))
    {
      gc_scheduled = 1;
      io_callout(&gc_callback);
    }
  
  sanity_check_object_list();
}

/* NOTE: If the object is close to the start of the object list, it is
 * deallocated and forgotten immediately. If the object is not found,
 * we don't search the entire list, but instead defer deallocation to
 * gc_sweep. */
void gc_kill(struct lsh_object *o)
{
  sanity_check_object_list();

  if (!o)
    return;

  assert(!o->dead);

  o->dead = 1;

#if 0
  debug("gc_kill: Killing object of type %z.\n",
	o->isa ? o->isa->name : "UNKNOWN");
#endif
  
  if (o == all_objects)
    {
      struct lsh_class *class;
      
#if 0
      debug("gc_kill:   Deallocating immediately.\n");
#endif
      
      for (class = o->isa; class; class = class->super_class)
	if (class->free_instance)
	  FREE_INSTANCE(class, o);

      all_objects = o->next;
      number_of_objects--;
      lsh_object_free(o);
    }
  else
    {
#if 0
      debug("gc_kill:   Deferring deallocation to gc_sweep\n");
#endif
    }
  
  sanity_check_object_list();
}

void
gc_global(struct resource *o)
{
  if (!root_set)
    root_set = empty_resource_list();
  
  assert(root_set->super.alive);

  REMEMBER_RESOURCE(root_set, o);
}

void gc(void)
{
  unsigned objects_before = number_of_objects;
#if DEBUG_ALLOC
  unsigned strings_before = number_of_strings;
#endif

  gc_mark(&root_set->super.super);  
  gc_sweep();
  
  verbose("Objects alive: %i, garbage collected: %i\n",
	  live_objects, objects_before - live_objects);
#if DEBUG_ALLOC
  verbose("Used strings:  %i, garbage collected: %i\n",
	  number_of_strings, strings_before - number_of_strings);
#endif
}

#if 0
void gc_maybe(int busy)
{
  sanity_check_object_list();

  if (number_of_objects > (100 + live_objects*(2+busy)))
    {
      verbose("Garbage collecting while %z...\n", busy ? "busy" : "idle");
      gc();
    }
}
#endif

/* Deallocate all objects. */

#if DEBUG_ALLOC
int gc_final_p = 0;
#endif

void gc_final(void)
{
  KILL_RESOURCE_LIST(root_set);
  root_set = NULL;

#if DEBUG_ALLOC
  gc_final_p = 1;

  gc_sweep();
  assert(!number_of_objects);

  if (number_of_strings)
    {
      struct lsh_string *s;
      werror("gc_final: %i strings leaked!\n", number_of_strings);
      for (s = all_strings; s; s = s->header.next)
	werror("  clue: %z\n", s->header.clue);
      fatal("gc_final: Internal error!\n");
    }
#endif /* DEBUG_ALLOC */
}
