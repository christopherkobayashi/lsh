/* transport.c
 *
 * Interface for the ssh transport protocol.
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2005 Niels M�ller
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

#include <assert.h>

#include "format.h"
#include "io.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#include "transport.h"

#define GABA_DEFINE
# include "transport.h.x"
#undef GABA_DEFINE

#include "transport.c.x"

/* Maximum time for keyexchange to complete */
#define TRANSPORT_TIMEOUT_KEYEXCHANGE (10 * 60)

/* Session key lifetime */
#define TRANSPORT_TIMEOUT_REEXCHANGE (40 * 60)

/* Time to wait for write buffer to drain after disconnect */
#define TRANSPORT_TIMEOUT_CLOSE (5 * 60)

void
init_transport_connection(struct transport_connection *self,
			  void (*kill)(struct resource *s),
			  struct transport_context *ctx,
			  int ssh_input, int ssh_output,
			  void (*event)(struct transport_connection *,
					enum transport_event event))
{
  init_resource(&self->super, kill);
  
  self->ctx = ctx;

  init_kexinit_state(&self->kex);
  self->session_id = NULL;
  self->keyexchange_handler = NULL;
  self->new_mac = NULL;
  self->new_crypto = NULL;
  self->new_inflate = NULL;
  
  self->ssh_input = ssh_input;
  self->reader = make_transport_read_state();

  self->ssh_output = ssh_output;
  self->writer = make_transport_write_state();

  self->event_handler = event;
}

/* GABA:
   (class
     (name transport_timeout)
     (super lsh_callback)
     (vars
       (connection object transport_connection)))
*/

static void
transport_timeout(struct transport_connection *connection,
		  unsigned seconds,
		  void (*callback)(struct lsh_callback *s))
{
  NEW(transport_timeout, self);
  self->super.f = callback;
  self->connection = connection;

  if (connection->expire)
    KILL_RESOURCE(connection->expire);

  connection->expire = io_callout(&self->super, seconds);
}

static void
transport_timeout_close(struct lsh_callback *s)
{
  CAST(transport_timeout, self, s);
  struct transport_connection *connection = self->connection;

  transport_close(connection, 0);
}

/* FIXME: Need to figure out how to interact with lshd close, and with
   the kill method. One idea: Generate a TRANSPORT_EVENT_CLOSE event,
   and let it return a value saying if we should close immediately, or
   linger a while for buffers to drain. */
void
transport_close(struct transport_connection *connection, int flush)
{
  if (connection->super.alive)
    {
      oop_source *source = connection->ctx->oop;
      connection->event_handler(connection, TRANSPORT_EVENT_CLOSE);
      if (connection->expire)
	{
	  KILL_RESOURCE(connection->expire);
	  connection->expire = NULL;
	}
      if (!flush)
	connection->super.alive = 0;
      
      if (connection->ssh_input >= 0)
	{
	  source->cancel_fd(source, connection->ssh_input, OOP_READ);
	  if (connection->ssh_input != connection->ssh_output)
	    close (connection->ssh_input);
	  connection->ssh_input = -1;
	}
      if (connection->ssh_output >= 0)
	{
	  if (flush && connection->write_pending)
	    {
	      /* Stay open for a while, to allow buffer to drain. */
	      transport_timeout(connection,
				TRANSPORT_TIMEOUT_CLOSE,
				transport_timeout_close);
	    }
	  else
	    {
	      source->cancel_fd(source, connection->ssh_output, OOP_WRITE);
	      close(connection->ssh_output);
	      connection->ssh_output = -1;
	    }
	}
    }
}

void
transport_kexinit_handler(struct transport_connection *connection,
			  uint32_t length, const uint8_t *packet)
{
  int is_server = connection->ctx->is_server;
  const char *error;

  /* Have we sent a kexinit message already? */
  if (!connection->kex.kexinit[is_server])
    transport_send_kexinit(connection);
  
  error = handle_kexinit(&connection->kex, length, packet,
			 connection->ctx->algorithms,
			 is_server);

  if (error)
    {
      transport_disconnect(connection,
			   SSH_DISCONNECT_KEY_EXCHANGE_FAILED, error);
      return;
    }
  {
    CAST_SUBTYPE(keyexchange_algorithm, kex_algorithm,
		 LIST(connection->kex.algorithm_list)[KEX_KEY_EXCHANGE]);
    
    /*�FIXME: Figure out precisely what KEYEXCHANGE_INIT is supposed
       to do */
    connection->keyexchange_handler
      = KEYEXCHANGE_INIT(kex_algorithm,
			 connection,
			 &connection->kex);

    assert(connection->keyexchange_handler);
  }  
}

static void
transport_timeout_reexchange(struct lsh_callback *s)
{
  CAST(transport_timeout, self, s);
  struct transport_connection *connection = self->connection;

  verbose("Session key expired. Initiating key re-exchange.\n");
  transport_send_kexinit(connection);
}

static void *
oop_read_ssh(oop_source *source, int fd, oop_event event, void *state)
{
  CAST_SUBTYPE(transport_connection, connection, (struct lsh_object *) state);
  int error;
  const char *error_msg;
  int res = 0;

  assert(event == OOP_READ);
  assert(fd == connection->ssh_input);

  while (connection->line_handler && connection->ssh_input >= 0)
    {
      uint32_t length;
      const uint8_t *line;
  
      res = transport_read_line(connection->reader, fd, &error, &error_msg,
				&length, &line);
      if (res != 1)
	goto done;
      
      fd = -1;

      if (!line)
	{
	  werror("Unexpected EOF at start of line.\n");
	  transport_close(connection, 0);
	}
      else
	connection->line_handler(connection, length, line);
    }
  while (connection->ssh_input >= 0)
    {
      uint32_t seqno;
      uint32_t length;
      const uint8_t *packet;
      
      uint8_t msg;

      res = transport_read_packet(connection->reader, fd, &error, &error_msg,
				  &seqno, &length, &packet);
      if (res != 1)
	goto done;
      
      fd = -1;
      
      /* Process packet */
      if (!packet)
	{
	  werror("Unexpected EOF at start of packet.\n");
	  transport_close(connection, 0);	  
	}
      if (length == 0)
	{
	  transport_protocol_error(connection, "Received empty packet");
	  return OOP_CONTINUE;
	}
      msg = packet[0];

      /* Messages of type IGNORE, DISCONNECT and DEBUG are always
	 acceptable. */
      if (msg == SSH_MSG_IGNORE)
	{
	  /* Do nothing */	  
	}
      else if (msg == SSH_MSG_DISCONNECT)
	{
	  verbose("Received disconnect message.\n");
	  transport_close(connection, 0);
	}
      else if (msg == SSH_MSG_DEBUG)
	{
	  /* Ignore it. Perhaps it's best to pass it on to the
	     application? */
	}

      /* Otherwise, behaviour depends on the kex state */
      else switch (connection->kex.read_state)
	{
	default:
	  abort();
	case KEX_STATE_IGNORE:
	  connection->kex.read_state = KEX_STATE_IN_PROGRESS;
	  break;
	case KEX_STATE_IN_PROGRESS:
	  if (msg < SSH_FIRST_KEYEXCHANGE_SPECIFIC
	      || msg >= SSH_FIRST_USERAUTH_GENERIC)
	    transport_protocol_error(connection,
			    "Unexpected message during key exchange");
	  else
	    connection->keyexchange_handler->handler(connection->keyexchange_handler,
						     connection, length, packet);
	  break;
	case KEX_STATE_NEWKEYS:
	  if (msg != SSH_MSG_NEWKEYS)
	    transport_protocol_error(connection, "NEWKEYS expected");
	  else if (length != 1)
	    transport_protocol_error(connection, "Invalid NEWKEYS message");
	  else
	    {
	      transport_read_new_keys(connection->reader,
				      connection->new_mac,
				      connection->new_crypto,
				      connection->new_inflate);
	      connection->new_mac = NULL;
	      connection->new_crypto = NULL;
	      connection->new_inflate = NULL;

	      transport_timeout(connection,
				TRANSPORT_TIMEOUT_REEXCHANGE,
				transport_timeout_reexchange);	      
	    }
	  break;

	case KEX_STATE_INIT:
	  if (msg == SSH_MSG_KEXINIT)
	    transport_kexinit_handler(connection, length, packet);
	  else if (msg >= SSH_FIRST_USERAUTH_GENERIC
		   && connection->packet_handler)
	    connection->packet_handler(connection, seqno, length, packet);
	  else
	    transport_send_packet(connection, format_unimplemented(seqno));
	  break;
	}
      if (connection->ssh_input < 0)
	{
	  /* We've been closed? */
	  return OOP_CONTINUE;
	}
    }
 done:
  switch (res)
    {
    default:
      abort();
    case 0: case 1:
      break;
    case -1:
      /* I/O error */
      werror("Read error: %e\n", error);
      transport_close(connection, 0);
      break;
    case -2:
      transport_disconnect(connection, error, error_msg);
      break;
    }
  return OOP_CONTINUE;
}

static void *
oop_write_ssh(oop_source *source, int fd, oop_event event, void *state)
{
  CAST_SUBTYPE(transport_connection, connection, (struct lsh_object *) state);
  int res;

  assert(event == OOP_WRITE);
  assert(fd == connection->ssh_output);

  res = transport_write_flush(connection->writer, fd);
  switch(res)
    {
    default: abort();
    case 0:
      /* More to write */
      break;
    case 1:
      transport_write_pending(connection, 0);
      break;
    case -1:
      if (errno != EWOULDBLOCK)
	{
	  werror("Write failed: %e\n", errno);
	  transport_close(connection, 0);
	}
      break;
    }
  return OOP_CONTINUE;
}

void
transport_write_pending(struct transport_connection *connection, int pending)
{
  if (pending != connection->write_pending)
    {
      oop_source *source = connection->ctx->oop;

      connection->write_pending = pending;
      if (pending)
	{
	  source->on_fd(source, connection->ssh_output,
			OOP_WRITE, oop_write_ssh, connection);
	  if (!connection->kex.write_state)
	    connection->event_handler(connection,
				      TRANSPORT_EVENT_STOP_APPLICATION);
	}
      else
	{
	  source->cancel_fd(source, connection->ssh_output, OOP_WRITE);
	  if (!connection->kex.write_state)
	    connection->event_handler(connection,
				      TRANSPORT_EVENT_START_APPLICATION); 
	}
    }
}

/* FIXME: Naming is unfortunate, with transport_write_packet vs
   transport_send_packet */
void
transport_send_packet(struct transport_connection *connection,
		      struct lsh_string *packet)
{
  int res;
  
  if (!connection->super.alive)
    {
      werror("connection_write_data: Connection is dead.\n");
      lsh_string_free(packet);
      return;
    }
  
  res = transport_write_packet(connection->writer, connection->ssh_output,
			       1, packet, connection->ctx->random);
  switch(res)
  {
  case -2:
    werror("Remote peer not responsive. Disconnecting.\n");
    transport_close(connection, 0);
    break;
  case -1:
    werror("Write failed: %e\n", errno);
    transport_close(connection, 0);
    break;
  case 0:
    transport_write_pending(connection, 1);
    break;
  case 1:
    transport_write_pending(connection, 0);
    break;
  }
}

void
transport_disconnect(struct transport_connection *connection,
		     int reason, const uint8_t *msg)
{
  if (msg)
    werror("Disconnecting: %z\n", msg);
  
  if (reason)
    transport_send_packet(connection, format_disconnect(reason, msg, ""));

  transport_close(connection, 1);
};

static void
transport_timeout_keyexchange(struct lsh_callback *s)
{
  CAST(transport_timeout, self, s);
  struct transport_connection *connection = self->connection;

  transport_disconnect(connection, SSH_DISCONNECT_BY_APPLICATION,
		       "Key exchange timeout");  
}

void
transport_send_kexinit(struct transport_connection *connection)
{
  int is_server = connection->ctx->is_server;
  struct lsh_string *s;
  struct kexinit *kex;

  connection->kex.write_state = 1;
  if (!connection->write_pending)
    connection->event_handler(connection, TRANSPORT_EVENT_STOP_APPLICATION);
  
  kex = MAKE_KEXINIT(connection->ctx->kexinit, connection->ctx->random);
  connection->kex.kexinit[is_server] = kex;

  
  assert(kex->first_kex_packet_follows == !!kex->first_kex_packet);
  assert(connection->kex.read_state == KEX_STATE_INIT);
  
  s = format_kexinit(kex);
  connection->kex.literal_kexinit[is_server] = lsh_string_dup(s); 
  transport_send_packet(connection, s);

  /* NOTE: This feature isn't fully implemented, as we won't tell
   * the selected key exchange method if the guess was "right". */
  if (kex->first_kex_packet_follows)
    {
      s = kex->first_kex_packet;
      kex->first_kex_packet = NULL;

      transport_send_packet(connection, s);
    }

  if (connection->session_id)
    {
      /* This is a reexchange; no more data can be sent */
      connection->event_handler(connection,
				TRANSPORT_EVENT_START_APPLICATION);
    }

  transport_timeout(connection,
		    TRANSPORT_TIMEOUT_KEYEXCHANGE,
		    transport_timeout_keyexchange);
}

void
transport_keyexchange_finish(struct transport_connection *connection,
			     const struct hash_algorithm *H,
			     struct lsh_string *exchange_hash,
			     struct lsh_string *K)
{
  int first = !connection->session_id;
  
  transport_send_packet(connection, format_newkeys());

  connection->kex.write_state = 0;

  if (first)
    connection->session_id = exchange_hash;

  if (!keyexchange_finish(connection, H, exchange_hash, K))
    {
      transport_disconnect(connection, SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
			   "Key exchange resulted in weak keys!");
      return;
    }

  if (first)    
    connection->event_handler(connection,
			      TRANSPORT_EVENT_KEYEXCHANGE_COMPLETE);
  else
    {
      lsh_string_free(exchange_hash);
      connection->event_handler(connection,
				TRANSPORT_EVENT_START_APPLICATION);
    }
}

void
transport_handshake(struct transport_connection *connection,
		    struct lsh_string *version,
		    void (*line_handler)
		      (struct transport_connection *connection,
		       uint32_t length,
		       const uint8_t *line))
{
  oop_source *source = connection->ctx->oop;
  int is_server = connection->ctx->is_server;
  int res;
  
  connection->kex.version[is_server] = version;
  res = transport_write_line(connection->writer,
			     connection->ssh_output,
			     ssh_format("%lS\r\n", version));

  if (res < 0)
    {
      werror("Writing version string failed: %e\n", errno);
      transport_close(connection, 0);
    }

  transport_send_kexinit(connection);

  connection->line_handler = line_handler;

  source->on_fd(source, connection->ssh_input, OOP_READ,
		oop_read_ssh, connection);
}
