/* connection.c
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "connection.h"

#include "debug.h"
#include "encrypt.h"
#include "format.h"
#include "disconnect.h"
#include "keyexchange.h"
#include "compress.h"
#include "packet_ignore.h"
#include "pad.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#define CLASS_DEFINE
#include "connection.h.x"
#undef CLASS_DEFINE

static int handle_connection(struct abstract_write *w,
			     struct lsh_string *packet)
{
  CAST(ssh_connection, closure, w);
  UINT8 msg;

  if (!packet->length)
    {
      werror("connection.c: Received empty packet!\n");
      return LSH_FAIL | LSH_DIE;
    }

  msg = packet->data[0];

  debug("handle_connection: Received packet of type %d\n", msg);

  switch(closure->kex_state)
    {
    case KEX_STATE_INIT:
      if (msg == SSH_MSG_NEWKEYS)
	{
	  werror("Unexpected NEWKEYS message!\n");
	  lsh_string_free(packet);
	  return LSH_FAIL | LSH_DIE;
	}
      break;
    case KEX_STATE_IGNORE:
      debug("handle_connection: Ignoring packet %d\n", msg);

      /* It's concievable with key exchange methods for which one
       * wants to switch to the NEWKEYS state immediately. But for
       * now, we always switch to the IN_PROGRESS state, to wait for a
       * KEXDH_INIT or KEXDH_REPLY message. */
      closure->kex_state = KEX_STATE_IN_PROGRESS;
      lsh_string_free(packet);
      return LSH_OK | LSH_GOON;

    case KEX_STATE_IN_PROGRESS:
      if ( (msg == SSH_MSG_NEWKEYS)
	   || (msg == SSH_MSG_KEXINIT))
	{
	  werror("Unexpected KEXINIT or NEWKEYS message!\n");
	  lsh_string_free(packet);
	  return LSH_FAIL | LSH_DIE;
	}
      break;
    case KEX_STATE_NEWKEYS:
      if ( (msg != SSH_MSG_NEWKEYS)
	   && (msg != SSH_MSG_DISCONNECT) )
	{
	  werror("Expected NEWKEYS message, but received message %d!\n",
		 msg);
	  lsh_string_free(packet);
	  return LSH_FAIL | LSH_DIE;
	}
      break;
    default:
      fatal("handle_connection: Internal error.\n");
    }

  return HANDLE_PACKET(closure->dispatch[msg], closure, packet);
}

static int do_fail(struct packet_handler *closure,
		   struct ssh_connection *connection,
		   struct lsh_string *packet)
{
  CHECK_TYPE(packet_handler, closure);

  lsh_string_free(packet);
  return LSH_FAIL | LSH_DIE;
}

struct packet_handler *make_fail_handler(void)
{
  NEW(packet_handler, res);

  res->handler = do_fail;
  return res;
}

static int do_unimplemented(struct packet_handler *closure,
			    struct ssh_connection *connection,
			    struct lsh_string *packet)
{
  int res;
  CHECK_TYPE(packet_handler, closure);

  res =  A_WRITE(connection->write,
		 ssh_format("%c%i",
			    SSH_MSG_UNIMPLEMENTED,
			    packet->sequence_number));
  verbose("Received packet of unimplemented type %d.\n",
	  packet->data[0]);
  
  lsh_string_free(packet);
  return res;
}

struct packet_handler *make_unimplemented_handler(void)
{
  NEW(packet_handler, res);

  res->handler = do_unimplemented;
  return res;
}


struct ssh_connection *make_ssh_connection(struct packet_handler *kex_handler)
{
  int i;

  NEW(ssh_connection, connection);
  
  connection->super.write = handle_connection;

  /* Initialize instance variables */
  connection->client_version
    = connection->server_version
    = connection->session_id = NULL;

  connection->resources = empty_resource_list();
  
  connection->rec_max_packet = 0x8000;
  connection->rec_mac = NULL;
  connection->rec_crypto = NULL;

  connection->send_mac = NULL;
  connection->send_crypto = NULL;
  
  connection->kex_state = KEX_STATE_INIT;

  connection->kexinits[0]
    = connection->kexinits[1] = NULL;

  connection->literal_kexinits[0]
    = connection->literal_kexinits[1] = NULL;

  connection->newkeys = NULL;
  
  /* Initialize dispatch */
  connection->ignore = make_ignore_handler();
  connection->unimplemented = make_unimplemented_handler();
  connection->fail = make_fail_handler();
  
  for (i = 0; i < 0x100; i++)
    connection->dispatch[i] = connection->unimplemented;

  connection->dispatch[0] = connection->fail;
  connection->dispatch[SSH_MSG_DISCONNECT] = make_disconnect_handler();
  connection->dispatch[SSH_MSG_IGNORE] = connection->ignore;
  connection->dispatch[SSH_MSG_UNIMPLEMENTED] = connection->ignore;

  /* FIXME: Write a debug handler */
  connection->dispatch[SSH_MSG_DEBUG] = make_rec_debug_handler();

  connection->dispatch[SSH_MSG_KEXINIT] = kex_handler;

  /* Make all other known message types terminate the connection */

  connection->dispatch[SSH_MSG_SERVICE_REQUEST] = connection->fail;
  connection->dispatch[SSH_MSG_SERVICE_ACCEPT] = connection->fail;
  connection->dispatch[SSH_MSG_NEWKEYS] = connection->fail;
  connection->dispatch[SSH_MSG_KEXDH_INIT] = connection->fail;
  connection->dispatch[SSH_MSG_KEXDH_REPLY] = connection->fail;
  connection->dispatch[SSH_MSG_USERAUTH_REQUEST] = connection->fail;
  connection->dispatch[SSH_MSG_USERAUTH_FAILURE] = connection->fail;
  connection->dispatch[SSH_MSG_USERAUTH_SUCCESS] = connection->fail;
  connection->dispatch[SSH_MSG_USERAUTH_BANNER] = connection->fail;
  connection->dispatch[SSH_MSG_USERAUTH_PK_OK] = connection->fail;
  connection->dispatch[SSH_MSG_USERAUTH_PASSWD_CHANGEREQ] = connection->fail;
  connection->dispatch[SSH_MSG_GLOBAL_REQUEST] = connection->fail;
  connection->dispatch[SSH_MSG_REQUEST_SUCCESS] = connection->fail;
  connection->dispatch[SSH_MSG_REQUEST_FAILURE] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_OPEN] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_OPEN_CONFIRMATION] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_OPEN_FAILURE] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_WINDOW_ADJUST] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_DATA] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_EXTENDED_DATA] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_EOF] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_CLOSE] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_REQUEST] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_SUCCESS] = connection->fail;
  connection->dispatch[SSH_MSG_CHANNEL_FAILURE] = connection->fail;
  
  return connection;
}

void connection_init_io(struct ssh_connection *connection,
			struct abstract_write *raw,
			struct randomness *r)
{
  /* Initialize i/o hooks */
  connection->raw = raw;
  connection->write = make_packet_deflate(
                        make_packet_pad(
                          make_packet_encrypt(raw, connection), 
                          connection,
                          r),
		        connection
		      );

  connection->send_crypto = connection->rec_crypto = NULL;
  connection->send_mac = connection->rec_mac = NULL;
  connection->send_compress = connection->rec_compress = NULL;
}
