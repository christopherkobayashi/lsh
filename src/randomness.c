/* randomness.c
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

#include <sys/types.h>
#include <time.h>

#include "randomness.h"

#include "xalloc.h"

#define CLASS_DEFINE
#include "randomness.h.x"
#undef CLASS_DEFINE

#include "randomness.c.x"

/* Random */
/* CLASS:
   (class
     (name poor_random)
     (super randomness)
     (vars
       (hash object hash_instance)
       (pos simple UINT32)
       (buffer space UINT8)))
*/

#if 0
struct poor_random
{
  struct randomness super;
  struct hash_instance *hash;
  UINT32 pos;
  UINT8 *buffer;
};
#endif

static void do_poor_random(struct randomness *r, UINT32 length, UINT8 *dst)
{
  CAST(poor_random, self, r);

  while(length)
    {
      UINT32 available = self->hash->hash_size - self->pos;
      UINT32 to_copy;
      
      if (!available)
	{
	  time_t now = time(NULL); /* To avoid cycles */
	  HASH_UPDATE(self->hash, sizeof(now), (UINT8 *) &now);
	  HASH_UPDATE(self->hash, self->hash->hash_size,
		      self->buffer);
	  HASH_DIGEST(self->hash, self->buffer);

	  available = self->hash->hash_size;
	  self->pos = 0;
	}
      to_copy = MIN(available, length);

      memcpy(dst, self->buffer + self->pos, to_copy);
      length -= to_copy;
      dst += to_copy;
      self->pos += to_copy;
    }
}

struct randomness *make_poor_random(struct hash_algorithm *hash,
				    struct lsh_string *init)
{
  NEW(poor_random, self);
  time_t now = time(NULL); /* To avoid cycles */

  self->super.random = do_poor_random;
  self->hash = MAKE_HASH(hash);
  self->buffer = lsh_space_alloc(hash->hash_size);
  
  HASH_UPDATE(self->hash, sizeof(now), (UINT8 *) &now);
  HASH_UPDATE(self->hash, init->length, init->data);
  HASH_DIGEST(self->hash, self->buffer);
  
  self->pos = 0;

  return &self->super;
}
