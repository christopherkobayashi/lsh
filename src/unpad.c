/* unpad.c
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

#include "unpad.h"

#include "format.h"
#include "xalloc.h"

static int do_unpad(struct abstract_write **w,
		    struct lsh_string *packet)
{
  struct abstract_write_pipe *closure = (struct abstract_write_pipe *) *w;
  
  UINT8 padding_length;
  UINT32 payload_length;
  struct lsh_string *new;
  
  MDEBUG(closure);

  if (packet->length < 1)
    return 0;
  
  padding_length = packet->data[0];

  if ( (padding_length < 4)
       || (padding_length >= packet->length) )
    return 0;

  payload_length = packet->length - 1 - padding_length;
  
  new = ssh_format("%ls", payload_length, packet->data + 1);

  /* Keep sequence number */
  new->sequence_number = packet->sequence_number;

  lsh_string_free(packet);

  return A_WRITE(closure->next, new);
}

struct abstract_write *
make_packet_unpad(struct abstract_write *continuation)
{
  struct abstract_write_pipe *closure;

  NEW(closure);

  closure->super.write = do_unpad;
  closure->next = continuation;

  return &closure->super;
}
