/* read_data.c
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

#include "read_data.h"
#include "werror.h"
#include "xalloc.h"

static int do_read_data(struct read_handler **h,
			struct abstract_read *read)
{
  struct read_data *closure = (struct read_data *) *h;
  
  while(1)
    {
      struct lsh_string *packet = lsh_string_alloc(closure->block_size);
      int n = A_READ(read, packet->length, packet->data);
      
      switch(n)
	{
	case 0:
	  lsh_string_free(packet);
	  break;
	case A_FAIL:
	  /* Fall through */
	case A_EOF:
	  CALLBACK(closure->close_callback);
	  return WRITE_CLOSED;
	default:
	  {
	    int res;
	    packet->length = n;
	    /* FIXME: Use returned value */
	    res = A_WRITE(closure->handler, packet);
	    if (res != WRITE_OK)
	      return res;
	    break;
	  }
	}
    }
}

struct read_handler *make_read_data(struct abstract_write *handler,
				    struct callback *close_callback,
				    UINT32 block_size)
{
  struct read_data *closure = xalloc(sizeof(struct read_data));

  closure->super.handler = do_read_data;
  closure->block_size = block_size;

  closure->handler = handler;
  closure->close_callback = close_callback;

  return &closure->super;
}
