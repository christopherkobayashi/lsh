/* compress.c
 *
 * packet compressor
 * 
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998 Balazs Scheidler, Niels M�ller
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

#include "lsh_types.h"
#include "xalloc.h"

#include "compress.h"

#include "compress.c.x"

/* CLASS:
   (class
     (name packet_compressor)
     (super abstract_write_pipe)
     (vars
       (compressor object compress_instance)
       (connection object ssh_connection)
       (mode simple int)))
*/

static int do_packet_deflate(struct abstract_write *closure,
			     struct lsh_string *packet)
{
  CAST(packet_compressor, self, closure);
  
  return A_WRITE(self->super.next,
		 (self->connection->send_compress
		  ? CODEC(self->connection->send_compress, packet, 1)
		  : packet));
}

static int do_packet_inflate(struct abstract_write *closure,
			     struct lsh_string *packet)
{
  CAST(packet_compressor, self, closure);

  return A_WRITE(self->super.next,
		 (self->connection->rec_compress
		  ? CODEC(self->connection->rec_compress, packet, 1)
		  : packet));
}

struct abstract_write *make_packet_codec(struct abstract_write *next,
					 struct ssh_connection *connection,
					 int mode)
{
  NEW(packet_compressor, res);
	
  res->super.super.write = (mode == COMPRESS_INFLATE)
    ? do_packet_inflate
    : do_packet_deflate;
  
  res->super.next = next;
  res->connection = connection;

  return &res->super.super;
}
