/* encrypt.c
 *
 */

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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "encrypt.h"

#include "format.h"
#include "xalloc.h"

#include "encrypt.c.x"

/* GABA:
   (class
     (name packet_encrypt)
     (super abstract_write_pipe)
     (vars
       (sequence_number . uint32_t)
       (connection object ssh_connection)))
*/

static void
do_encrypt(struct abstract_write *w,
	   struct lsh_string *packet)
{
  CAST(packet_encrypt, closure, w);
  struct ssh_connection *connection = closure->connection;
  struct lsh_string *new;
  uint8_t *mac;

  new = ssh_format("%lr%lr", packet->length, NULL,
		   connection->send_mac ? connection->send_mac->mac_size : 0,
		   &mac);

  if (connection->send_crypto)
    CRYPT(connection->send_crypto, packet->length, packet->data, new->data);
  else
    memcpy(new->data, packet->data, packet->length);
  
  if (connection->send_mac)
  {
    uint8_t s[4];
    WRITE_UINT32(s, closure->sequence_number);

    MAC_UPDATE(connection->send_mac, 4, s);
    MAC_UPDATE(connection->send_mac, packet->length, packet->data);
    MAC_DIGEST(connection->send_mac, mac);
  }
  lsh_string_free(packet);

  closure->sequence_number++;
  
  A_WRITE(closure->super.next, new);
}

struct abstract_write *
make_packet_encrypt(struct abstract_write *next,
		    struct ssh_connection *connection)
{
  NEW(packet_encrypt, closure);

  closure->super.super.write = do_encrypt;
  closure->super.next = next;
  closure->sequence_number = 0;
  closure->connection = connection;

  return &closure->super.super;
}

    
