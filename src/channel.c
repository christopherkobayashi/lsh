/* channel.c
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

#include <assert.h>
#include <string.h>

#include "channel.h"

#include "format.h"
#include "io.h"
#include "lsh_string.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#define GABA_DEFINE
#include "channel.h.x"
#undef GABA_DEFINE

#include "channel.c.x"

/* Opening a new channel: There are two cases, depending on which side
   sends the CHANNEL_OPEN_REQUEST. When we send it, the following
   steps are taken:

   1. Create a new channel object of the appropriate type.
   
   2. Call request_channel_open. This allocates a channel number,
      registers the object, and sends a CHANNEL_OPEN request.

   3. If the remote end replies with CHANNEL_OPEN_CONFIRMATION, the
      channel's event handler is invoked, with CHANNEL_EVENT_CONFIRM.
      If the remote end replies with CHANNEL_OPEN_FAILURE, then event
      handler is invoked with CHANNEL_EVENT_DENY, and then the channel
      is killed.

   When the other side requests a new channel, the steps are:

   1. Receive CHANNEL_OPEN. Reserve a channel number, and invoke the
      command associated with the channel type.

   2. This command returns a new channel object, or raises an
      exception.

   3. Install the channel object, and reply with
      CHANNEL_OPEN_CONFIRMATION. Generate a CHANNEL_EVENT_CONFIRM on
      the channel. On error, deallocate channel number, and reply with
      CHANNEL_OPEN_FAILURE.

   FIXME: The first part of this description is correct, but the
   second half is not yet accurate.
*/

struct exception *
make_channel_open_exception(uint32_t error_code, const char *msg)
{
#define MAX_ERROR 4
  static const char *msgs[MAX_ERROR + 1] = {
    "",
    "Administratively prohibited",
    "Connect failed",
    "Unknown channel type",
    "Resource shortage"
  };

  assert(error_code > 0);
  assert(error_code <= MAX_ERROR);
#undef MAX_ERROR

  return make_exception(EXC_CHANNEL_OPEN, error_code,
			msg ? msg : msgs[error_code]);
}


static struct lsh_string *
format_global_failure(void)
{
  return ssh_format("%c", SSH_MSG_REQUEST_FAILURE);
}

static struct lsh_string *
format_global_success(void)
{
  return ssh_format("%c", SSH_MSG_REQUEST_SUCCESS);
}

/* The advertised rec_max_size must be a little smaller than SSH_MAX_PACKET,
 * to make sure that our peer won't send us packets exceeding our limit for
 * the connection. */

/* NOTE: It would make some sense to use the connection's
 * rec_max_packet instead of the SSH_MAX_PACKET constant. */

#define SSH_MAX_DATA_SIZE (SSH_MAX_PACKET - SSH_CHANNEL_MAX_PACKET_FUZZ)

static void
check_rec_max_packet(struct ssh_channel *channel)
{
  /* Never advertise a larger rec_max_packet than we're willing to
   * handle. */

  if (channel->rec_max_packet > SSH_MAX_DATA_SIZE)
    {
      debug("check_rec_max_packet: Reduced rec_max_packet from %i to %i.\n",
	    channel->rec_max_packet, SSH_MAX_DATA_SIZE);
      channel->rec_max_packet = SSH_MAX_DATA_SIZE;
    }
}

struct lsh_string *
format_open_confirmation(struct ssh_channel *channel,
			 const char *format, ...)
{
  va_list args;
  uint32_t l1, l2;
  struct lsh_string *packet;
  
#define CONFIRM_FORMAT "%c%i%i%i%i"
#define CONFIRM_ARGS \
  SSH_MSG_CHANNEL_OPEN_CONFIRMATION, \
  channel->remote_channel_number, channel->local_channel_number, \
  channel->rec_window_size, channel->rec_max_packet
    
  check_rec_max_packet(channel);

  debug("format_open_confirmation: rec_window_size = %i,\n"
	"                          rec_max_packet = %i,\n",
       channel->rec_window_size,
       channel->rec_max_packet);
  l1 = ssh_format_length(CONFIRM_FORMAT, CONFIRM_ARGS);

  va_start(args, format);
  l2 = ssh_vformat_length(format, args);
  va_end(args);

  packet = lsh_string_alloc(l1 + l2);

  ssh_format_write(CONFIRM_FORMAT, packet, 0, CONFIRM_ARGS);

  va_start(args, format);
  ssh_vformat_write(format, packet, l1, args);
  va_end(args);

  return packet;
#undef CONFIRM_FORMAT
#undef CONFIRM_ARGS
}

struct lsh_string *
format_open_failure(uint32_t channel, uint32_t reason,
		    const char *msg, const char *language)
{
  return ssh_format("%c%i%i%z%z", SSH_MSG_CHANNEL_OPEN_FAILURE,
		    channel, reason, msg, language);
}

struct lsh_string *
format_channel_success(uint32_t channel)
{
  return ssh_format("%c%i", SSH_MSG_CHANNEL_SUCCESS, channel);
}

struct lsh_string *
format_channel_failure(uint32_t channel)
{
  return ssh_format("%c%i", SSH_MSG_CHANNEL_FAILURE, channel);
}

static struct lsh_string *
format_channel_data(uint32_t number, uint32_t length, const uint8_t *data)
{
  return ssh_format("%c%i%s", SSH_MSG_CHANNEL_DATA,
		    number, length, data);
}

static struct lsh_string *
format_channel_extended_data(uint32_t number, uint32_t type,
			     uint32_t length, const uint8_t *data)
{
  return ssh_format("%c%i%i%s", SSH_MSG_CHANNEL_EXTENDED_DATA,
		    number, type, length, data);
}

static struct lsh_string *
format_channel_window_adjust(uint32_t number, uint32_t add)
{
  return ssh_format("%c%i%i",
		    SSH_MSG_CHANNEL_WINDOW_ADJUST,
		    number, add);
}

static struct lsh_string *
format_channel_close(struct ssh_channel *channel)
{
  return ssh_format("%c%i",
		    SSH_MSG_CHANNEL_CLOSE,
		    channel->remote_channel_number);
}

static struct lsh_string *
format_channel_eof(uint32_t number)
{
  return ssh_format("%c%i",
		    SSH_MSG_CHANNEL_EOF, number);
}

static void
channel_finished(struct ssh_channel *channel)
{
  if (!channel->super.alive)
    werror("channel_finished called on a dead channel.\n");
  else
    {
      struct ssh_connection *connection = channel->connection;

      trace("channel_finished: Deallocating channel %i\n", channel->local_channel_number);
      KILL_RESOURCE(&channel->super);

      /* Disassociate from the connection. */
      channel->connection = NULL;
      
      ssh_connection_dealloc_channel(connection, channel->local_channel_number);

      trace("channel_finished: connection->pending_close = %i,\n"
	    "                  connection->channel_count = %i\n",
	    connection->pending_close, connection->channel_count);

      if (connection->pending_close && !connection->channel_count)
	KILL_RESOURCE(&connection->super);
    }
}


/* Channel objects */

/* In principle, this belongs to connection.h, but it needs the
   definition of ssh_channel. */
void
ssh_connection_register_channel(struct ssh_connection *connection,
				uint32_t local_channel_number,
				struct ssh_channel *channel)
{
  assert(local_channel_number < connection->used_channels);
  assert(!connection->channels[local_channel_number]);
  assert(connection->alloc_state[local_channel_number] != CHANNEL_FREE);

  trace("ssh_connection_register_channel: local_channel_number: %i.\n",
	local_channel_number);

  connection->channels[local_channel_number] = channel;
  channel->connection = connection;
  remember_resource(connection->resources, &channel->super);  
}

static void
send_window_adjust(struct ssh_channel *channel,
		   uint32_t add)
{
  channel->rec_window_size += add;

  SSH_CONNECTION_WRITE(
    channel->connection,   
    format_channel_window_adjust(channel->remote_channel_number, add));
}

/* FIXME: It seems suboptimal to send a window adjust message for
 * *every* write that we do. A better scheme might be as follows:
 *
 * Delay window adjust messages, keeping track of both the locally
 * maintained window size, which is updated after each write, and the
 * size that has been reported to the remote end. When the difference
 * between these two values gets large enough (say, larger than one
 * half or one third of the maximum window size), we send a
 * window_adjust message to sync them. */
void
channel_adjust_rec_window(struct ssh_channel *channel, uint32_t written)
{
  /* NOTE: The channel object (referenced as a flow-control callback)
   * may live longer than the actual channel. */
  if (written && ! (channel->flags & (CHANNEL_RECEIVED_EOF | CHANNEL_RECEIVED_CLOSE
				      | CHANNEL_SENT_CLOSE)))
    send_window_adjust(channel, written);
}

void
channel_start_receive(struct ssh_channel *channel,
		      uint32_t initial_window_size)
{
  if (channel->rec_window_size < initial_window_size)
    send_window_adjust(channel,
		       initial_window_size - channel->rec_window_size);
}

/* Channel related messages */

/* GABA:
   (class
     (name request_status)
     (vars
       ; -1 for still active requests,
       ; 0 for failure,
       ; 1 for success
       (status . int)))
*/

static struct request_status *
make_request_status(void)
{
  NEW(request_status, self);
  self->status = -1;

  return self;
}

/* GABA:
   (class
     (name global_request_continuation)
     (super command_continuation)
     (vars
       (connection object ssh_connection)
       (active object request_status)))
*/

static void 
send_global_request_responses(struct ssh_connection *connection)
{
  struct object_queue *q = &connection->active_global_requests;

  assert(!object_queue_is_empty(q));

  for (;;)
    {
      CAST(request_status, n, object_queue_peek_head(q));
      if (!n || (n->status < 0))
	break;
 
      object_queue_remove_head(q);

      SSH_CONNECTION_WRITE(connection, (n->status
				  ? format_global_success()
				  : format_global_failure()));
    }
}

static void
do_global_request_response(struct command_continuation *s,
			   struct lsh_object *x UNUSED)
{
  CAST(global_request_continuation, self, s);

  assert(self->active->status == -1);
  self->active->status = 1;

  send_global_request_responses(self->connection);
}

static struct command_continuation *
make_global_request_response(struct ssh_connection *connection,
			     struct request_status *active)
{
  NEW(global_request_continuation, self);

  self->super.c = do_global_request_response;
  self->connection = connection;
  self->active = active;
   
  return &self->super;
}


/* GABA:
   (class
     (name global_request_exception_handler)
     (super exception_handler)
     (vars
       (connection object ssh_connection)
       (active object request_status)))
*/

/* All exceptions are treated as a failure. */
static void 
do_exc_global_request_handler(struct exception_handler *c,
			      const struct exception *e)
{
  CAST(global_request_exception_handler, self, c);

  assert(self->active->status == -1);
  self->active->status = 0;

  werror("Denying global request: %z\n", e->msg);
  send_global_request_responses(self->connection);
}

static struct exception_handler *
make_global_request_exception_handler(struct ssh_connection *connection,
				      struct request_status *active,
				      const char *context)
{
  NEW(global_request_exception_handler, self);

  self->super.raise = do_exc_global_request_handler;
  self->super.context = context;
  self->active = active;
  self->connection = connection;
  return &self->super;
}

static void
handle_global_request(struct ssh_connection *connection,
		      struct simple_buffer *buffer)
{
  enum lsh_atom name;
  int want_reply;
  
  if (parse_atom(buffer, &name)
      && parse_boolean(buffer, &want_reply))
    {
      struct global_request *req = NULL;

      if (name && connection->global_requests)
	{
	  CAST_SUBTYPE(global_request, r,
		       ALIST_GET(connection->global_requests,
				 name));
	  req = r;
	}
      if (!req)
	{
	  SSH_CONNECTION_WRITE(connection, format_global_failure());
	  return;
	}
      else
	{
	  struct command_continuation *c;
	  struct exception_handler *e;
	  if (want_reply)
	    {
	      struct request_status *a = make_request_status();
	      
	      object_queue_add_tail(&connection->active_global_requests,
				    &a->super);
	      
	      c = make_global_request_response(connection, a);
	      e = make_global_request_exception_handler(connection, a,
							HANDLER_CONTEXT);
	    }
	  else
	    {
	      /* We should ignore failures. */
	      c = &discard_continuation;
	      e = &ignore_exception_handler;
	    }
	  GLOBAL_REQUEST(req, connection, name, want_reply, buffer, c, e);
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid SSH_MSG_GLOBAL_REQUEST message.");
}

static void
handle_global_success(struct ssh_connection *connection,
		      struct simple_buffer *buffer)
{
  if (!parse_eod(buffer))
    {
      SSH_CONNECTION_ERROR(connection, "Invalid GLOBAL_REQUEST_SUCCESS message.");
      return;
    }

  if (object_queue_is_empty(&connection->pending_global_requests))
    {
      werror("do_global_request_success: Unexpected message, ignoring.\n");
      return;
    }
  {
    CAST_SUBTYPE(command_context, ctx,
		 object_queue_remove_head(&connection->pending_global_requests));
    COMMAND_RETURN(ctx->c, connection);
  }
}

struct exception global_request_exception =
STATIC_EXCEPTION(EXC_GLOBAL_REQUEST, 0, "Global request failed");

static void
handle_global_failure(struct ssh_connection *connection,
		      struct simple_buffer *buffer)
{
  if (!parse_eod(buffer))
    {
      SSH_CONNECTION_ERROR(connection, "Invalid GLOBAL_REQUEST_FAILURE message.");
      return;
    }

  if (object_queue_is_empty(&connection->pending_global_requests))
    {
      werror("do_global_request_failure: Unexpected message, ignoring.\n");
    }
  else
    {
      CAST_SUBTYPE(command_context, ctx,
		   object_queue_remove_head(&connection->pending_global_requests));
      EXCEPTION_RAISE(ctx->e, &global_request_exception);
    }
}

/* FIXME: Don't store the channel here, instead have it passed as the
 * argument of the continuation. This might also allow some
 * unification with the handling of global_requests.
 *
 * This won't quite work yet, because not all channel request
 * handlers, in particular gateway_channel_request and
 * x11_req_handler, return the channel in question. */

/* GABA:
   (class
     (name channel_request_continuation)
     (super command_continuation)
     (vars
       (channel object ssh_channel)
       (active object request_status)))
*/

static void
send_channel_request_responses(struct ssh_channel *channel)
{
  struct object_queue *q = &channel->active_requests;
  assert(!object_queue_is_empty(q));

  for (;;)
    {
      CAST(request_status, n, object_queue_peek_head(q));
      if (!n || (n->status < 0))
	break;

      object_queue_remove_head(q);

      SSH_CONNECTION_WRITE(channel->connection,
		   (n->status
		    ? format_channel_success(channel->remote_channel_number)
		    : format_channel_failure(channel->remote_channel_number)));
    }
}

static void
do_channel_request_response(struct command_continuation *s,
			    struct lsh_object *x UNUSED)
{
  CAST(channel_request_continuation, self, s);

  trace("do_channel_request_response\n");
  assert(self->active->status == -1);
  self->active->status = 1;

  send_channel_request_responses(self->channel);
}

static struct command_continuation *
make_channel_request_response(struct ssh_channel *channel,
			      struct request_status *active)
{
  NEW(channel_request_continuation, self);

  trace("make_channel_request_response\n");

  self->super.c = do_channel_request_response;
  self->channel = channel;
  self->active = active;

  return &self->super;
}

/* GABA:
   (class
     (name channel_request_exception_handler)
     (super exception_handler)
     (vars
       (channel object ssh_channel)
       (active object request_status)))
*/

/* All exceptions are treated as a failure. */
static void 
do_exc_channel_request_handler(struct exception_handler *c,
			       const struct exception *e)
{
  CAST(channel_request_exception_handler, self, c);

  assert(self->active->status == -1);
  self->active->status = 0;

  werror("Denying channel request: %z\n", e->msg);
  send_channel_request_responses(self->channel);
}

static struct exception_handler *
make_channel_request_exception_handler(struct ssh_channel *channel,
				       struct request_status *active,
				       const char *context)
{
  NEW(channel_request_exception_handler, self);

  self->super.raise = do_exc_channel_request_handler;
  self->super.context = context;

  self->channel = channel;
  self->active = active;

  return &self->super;
}

static void
handle_channel_request(struct ssh_connection *connection,
		       struct simple_buffer *buffer)
{
  uint32_t channel_number;
  struct channel_request_info info;
  
  if (parse_uint32(buffer, &channel_number)
      &&parse_string(buffer,
		     &info.type_length, &info.type_data)
      && parse_boolean(buffer, &info.want_reply))
    {    
      struct ssh_channel *channel
	= ssh_connection_lookup_channel(connection,
					channel_number,
					CHANNEL_ALLOC_ACTIVE);
      if (channel)
	{
	  if (channel->request_methods)
	    {
	      channel->request_methods->request(channel, &info, buffer);
	      return;
	    }
	  else if (channel->request_types)
	    {
	      struct command_continuation *c;
	      struct exception_handler *e;
	      struct channel_request *req;

	      trace("handle_channel_request: Request type `%ps' on channel %i\n",
		    info.type_length, info.type_data, channel_number);

	      info.type = lookup_atom(info.type_length, info.type_data);

	      {
		CAST_SUBTYPE(channel_request, r,
			     ALIST_GET(channel->request_types, info.type));

		req = r;
	      }

	      if (req)
		{
		  if (info.want_reply)
		    {
		      struct request_status *a = make_request_status();

		      object_queue_add_tail(&channel->active_requests,
					    &a->super);

		      c = make_channel_request_response(channel, a);
		      e = make_channel_request_exception_handler(channel, a,
								 HANDLER_CONTEXT);
		    }
		  else
		    {
		      /* We should ignore failures. */
		      c = &discard_continuation;
		      e = &ignore_exception_handler;
		    }
	      
		  CHANNEL_REQUEST(req, channel, &info, buffer, c, e);
		  return;
		}
	    }

	  if (info.want_reply)
	    SSH_CONNECTION_WRITE(connection,
				 format_channel_failure(channel->remote_channel_number));
	  return;
	}
      
      werror("SSH_MSG_CHANNEL_REQUEST on nonexistant channel %i.\n",
	     channel_number);
      /* Fall through to error case. */
    }
  
  SSH_CONNECTION_ERROR(connection,
		       "Invalid SSH_MSG_CHANNEL_REQUEST message.");
}


/* GABA:
   (class
     (name channel_open_continuation)
     (super command_continuation)
     (vars
       (connection object ssh_connection)
       (local_channel_number . uint32_t)
       (remote_channel_number . uint32_t)
       (send_window_size . uint32_t)
       (send_max_packet . uint32_t)))
*/

static void
do_channel_open_continue(struct command_continuation *c,
			 struct lsh_object *value)
{
  CAST(channel_open_continuation, self, c);
  CAST_SUBTYPE(ssh_channel, channel, value);

  assert(channel);

  /* FIXME: This copying could just as well be done by the
   * CHANNEL_OPEN handler? Then we can remove the corresponding fields
   * from the closure as well. */
  channel->send_window_size = self->send_window_size;
  channel->send_max_packet = self->send_max_packet;
  channel->remote_channel_number = self->remote_channel_number;
  
  ssh_connection_register_channel(self->connection,
				  self->local_channel_number,
				  channel);
  ssh_connection_activate_channel(self->connection,
				  self->local_channel_number);

  /* FIXME: Doesn't support sending extra arguments with the
   * confirmation message. */

  SSH_CONNECTION_WRITE(self->connection,
		       format_open_confirmation(channel, ""));

  CHANNEL_EVENT(channel, CHANNEL_EVENT_CONFIRM);  
}

static struct command_continuation *
make_channel_open_continuation(struct ssh_connection *connection,
			       uint32_t local_channel_number,
			       uint32_t remote_channel_number,
			       uint32_t send_window_size,
			       uint32_t send_max_packet)
{
  NEW(channel_open_continuation, self);

  self->super.c = do_channel_open_continue;
  self->connection = connection;
  self->local_channel_number = local_channel_number;
  self->remote_channel_number = remote_channel_number;
  self->send_window_size = send_window_size;
  self->send_max_packet = send_max_packet;

  return &self->super;
}
			       
/* GABA:
   (class
     (name exc_channel_open_handler)
     (super exception_handler)
     (vars
       (connection object ssh_connection)
       (local_channel_number . uint32_t)
       (remote_channel_number . uint32_t)))
*/

static void
do_exc_channel_open_handler(struct exception_handler *s,
			    const struct exception *e)
{
  CAST(exc_channel_open_handler, self, s);
  struct ssh_connection *connection = self->connection;
  uint32_t error_code = (e->type == EXC_CHANNEL_OPEN)
    ? e->subtype : SSH_OPEN_RESOURCE_SHORTAGE;

  assert(self->local_channel_number < connection->used_channels);  
  assert(connection->alloc_state[self->local_channel_number]
	 == CHANNEL_ALLOC_SENT_OPEN);
  assert(!connection->channels[self->local_channel_number]);

  ssh_connection_dealloc_channel(connection, self->local_channel_number);

  werror("Denying channel open: %z\n", e->msg);
  
  SSH_CONNECTION_WRITE(connection,
		       format_open_failure(self->remote_channel_number,
					   error_code, e->msg, ""));
}

static struct exception_handler *
make_exc_channel_open_handler(struct ssh_connection *connection,
			      uint32_t local_channel_number,
			      uint32_t remote_channel_number,
			      const char *context)
{
  NEW(exc_channel_open_handler, self);
  self->super.raise = do_exc_channel_open_handler;
  self->super.context = context;
  
  self->connection = connection;
  self->local_channel_number = local_channel_number;
  self->remote_channel_number = remote_channel_number;

  return &self->super;
}

static void
handle_channel_open(struct ssh_connection *connection,
		    struct simple_buffer *buffer)
{
  struct channel_open_info info;

  trace("handle_channel_open\n");
  
  if (parse_string(buffer, &info.type_length, &info.type_data)
      && parse_uint32(buffer, &info.remote_channel_number)
      && parse_uint32(buffer, &info.send_window_size)
      && parse_uint32(buffer, &info.send_max_packet))
  {
      struct channel_open *open = NULL;

      info.type = lookup_atom(info.type_length, info.type_data);

      /* We don't support larger packets than the default,
       * SSH_MAX_PACKET. */
      if (info.send_max_packet > SSH_MAX_PACKET)
	{
	  werror("handle_channel_open: The remote end asked for really large packets.\n");
	  info.send_max_packet = SSH_MAX_PACKET;
	}
      
      if (connection->pending_close)
	{
	  /* We are waiting for channels to close. Don't open any new ones. */

	  SSH_CONNECTION_WRITE(connection,
		       format_open_failure(
			 info.remote_channel_number,
			 SSH_OPEN_ADMINISTRATIVELY_PROHIBITED,
			 "Waiting for channels to close.", ""));
	}
      else
	{
	  if (info.type)
	    {
	      CAST_SUBTYPE(channel_open, o,
			   ALIST_GET(connection->channel_types,
				     info.type));
	      open = o;
	    }
	  if (!open)
	    {
	      werror("handle_channel_open: Unknown channel type `%ps'\n",
		     info.type_length, info.type_data);
	      SSH_CONNECTION_WRITE(connection,
			   format_open_failure(
			     info.remote_channel_number,
			     SSH_OPEN_UNKNOWN_CHANNEL_TYPE,
			     "Unknown channel type", ""));
	    }
	  else
	    {
	      int local_number
		= ssh_connection_alloc_channel(connection,
					       CHANNEL_ALLOC_RECEIVED_OPEN);

	      if (local_number < 0)
		{
		  SSH_CONNECTION_WRITE(connection,
			       format_open_failure(
				 info.remote_channel_number,
				 SSH_OPEN_RESOURCE_SHORTAGE,
				 "Channel limit exceeded.", ""));
		  return;
		}
	      
	      CHANNEL_OPEN(open, connection,
			   &info,
			   buffer,
			   make_channel_open_continuation(connection,
							  local_number,
							  info.remote_channel_number,
							  info.send_window_size,
							  info.send_max_packet),
			   make_exc_channel_open_handler(connection,
							 local_number,
							 info.remote_channel_number,
							 HANDLER_CONTEXT));

	    }
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid SSH_MSG_CHANNEL_OPEN message.");
}     

static void
handle_adjust_window(struct ssh_connection *connection,
		     struct simple_buffer *buffer)
{
  uint32_t channel_number;
  uint32_t size;

  if (parse_uint32(buffer, &channel_number)
      && parse_uint32(buffer, &size)
      && parse_eod(buffer))
    {
      struct ssh_channel *channel
	= ssh_connection_lookup_channel(connection,
					channel_number,
					CHANNEL_ALLOC_ACTIVE);

      if (channel
	  && !(channel->flags & CHANNEL_RECEIVED_CLOSE))
	{
	  if (! (channel->flags & (CHANNEL_SENT_CLOSE | CHANNEL_SENT_EOF)))
	    {
	      channel->send_window_size += size;

	      if (channel->send_window_size && channel->send_adjust)
		CHANNEL_SEND_ADJUST(channel, size);
	    }
	}
      else
	{
	  werror("SSH_MSG_CHANNEL_WINDOW_ADJUST on nonexistant or closed "
		 "channel %i\n", channel_number);
	  SSH_CONNECTION_ERROR(connection, "Unexpected CHANNEL_WINDOW_ADJUST");
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_WINDOW_ADJUST message.");
}

/* Common processing for ordinary and "extended" data. */
static int
receive_data_common(struct ssh_channel *channel,
		    int type, uint32_t length, const uint8_t *data)
{
  if (channel->receive
      && !(channel->flags & (CHANNEL_RECEIVED_EOF
			     | CHANNEL_RECEIVED_CLOSE)))
    {
      if (channel->flags & CHANNEL_SENT_CLOSE)
	{
	  werror("Ignoring data on channel which is closing\n");
	  return 1;
	}
      else
	{
	  if (length > channel->rec_max_packet)
	    {
	      werror("Channel data larger than rec_max_packet. Extra data ignored.\n");
	      length = channel->rec_max_packet;
	    }

	  if (length > channel->rec_window_size)
	    {
	      /* Truncate data to fit window */
	      werror("Channel data overflow. Extra data ignored.\n");
	      debug("   (type = %i, data->length=%i, rec_window_size=%i).\n",
		    type, length, channel->rec_window_size);

	      length = channel->rec_window_size;
	    }

	  if (!length)
	    {
	      /* Ignore data packet */
	      return 1;
	    }
	  channel->rec_window_size -= length;

	  CHANNEL_RECEIVE(channel, type, length, data);
	}
      return 1;
    }
  else
    return 0;
}

static void
handle_channel_data(struct ssh_connection *connection,
		    struct simple_buffer *buffer)
{
  uint32_t channel_number;
  uint32_t length;
  const uint8_t *data;
  
  if (parse_uint32(buffer, &channel_number)
      && parse_string(buffer, &length, &data)
      && parse_eod(buffer))
    {
      struct ssh_channel *channel
	= ssh_connection_lookup_channel(connection,
					channel_number,
					CHANNEL_ALLOC_ACTIVE);

      if (channel)
	{
	  if (!receive_data_common(channel, CHANNEL_DATA,
				   length, data))
	    werror("Data on closed channel %i\n", channel_number);
	}
      else
	werror("Data on non-existant channel %i\n", channel_number);
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_DATA message.");
}

static void
handle_channel_extended_data(struct ssh_connection *connection,
			     struct simple_buffer *buffer)
{
  uint32_t channel_number;
  uint32_t type;
  uint32_t length;
  const uint8_t *data;
  
  if (parse_uint32(buffer, &channel_number)
      && parse_uint32(buffer, &type)
      && parse_string(buffer, &length, &data)
      && parse_eod(buffer))
    {
      struct ssh_channel *channel
	= ssh_connection_lookup_channel(connection,
					channel_number,
					CHANNEL_ALLOC_ACTIVE);
      
      if (channel)
	{
	  if (type != SSH_EXTENDED_DATA_STDERR)
	    werror("Unknown type %i of extended data.\n", type);
	    
	  else if (!receive_data_common(channel, CHANNEL_STDERR_DATA,
					length, data))
	    werror("Extended data on closed channel %i\n", channel_number);
	}      
      else
	werror("Extended data on non-existant channel %i\n", channel_number);
    }
  else
    SSH_CONNECTION_ERROR(connection,
			 "Invalid CHANNEL_EXTENDED_DATA message.");
}

static void
handle_channel_eof(struct ssh_connection *connection,
		   struct simple_buffer *buffer)
{
  uint32_t channel_number;
  
  if (parse_uint32(buffer, &channel_number)
      && parse_eod(buffer))
    {
      struct ssh_channel *channel
	= ssh_connection_lookup_channel(connection,
					channel_number,
					CHANNEL_ALLOC_ACTIVE);

      if (channel)
	{
	  if (channel->flags & (CHANNEL_RECEIVED_EOF | CHANNEL_RECEIVED_CLOSE))
	    {
	      werror("Receiving EOF on channel on closed channel.\n");
	      SSH_CONNECTION_ERROR(connection,
			     "Received EOF on channel on closed channel.");
	    }
	  else
	    {
	      verbose("Receiving EOF on channel %i (local %i)\n",
		      channel->remote_channel_number, channel_number);
	      
	      channel->flags |= CHANNEL_RECEIVED_EOF;
	      
	      CHANNEL_EVENT(channel, CHANNEL_EVENT_EOF);

	      /* Should we close the channel now? */
	      channel_maybe_close(channel);	      
	    }
	}
      else
	{
	  werror("EOF on non-existant channel %i\n",
		 channel_number);
	  SSH_CONNECTION_ERROR(connection, "EOF on non-existant channel");
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_EOF message");
}

static void
handle_channel_close(struct ssh_connection *connection,
		     struct simple_buffer *buffer)
{
  uint32_t channel_number;
  
  if (parse_uint32(buffer, &channel_number)
      && parse_eod(buffer))
    {
      struct ssh_channel *channel
	= ssh_connection_lookup_channel(connection,
					channel_number,
					CHANNEL_ALLOC_ACTIVE);

      if (channel)
	{
	  verbose("Receiving CLOSE on channel %i (local %i)\n",
		  channel->remote_channel_number, channel_number);
	      
	  if (channel->flags & CHANNEL_RECEIVED_CLOSE)
	    {
	      werror("Receiving multiple CLOSE on channel.\n");
	      SSH_CONNECTION_ERROR(connection, "Receiving multiple CLOSE on channel.");
	    }
	  else
	    {
	      channel->flags |= CHANNEL_RECEIVED_CLOSE;
	  
	      if (! (channel->flags & (CHANNEL_RECEIVED_EOF | CHANNEL_NO_WAIT_FOR_EOF
				       | CHANNEL_SENT_CLOSE)))
		{
		  werror("Unexpected channel CLOSE.\n");
		}
	      CHANNEL_EVENT(channel, CHANNEL_EVENT_CLOSE);

	      if (channel->flags & CHANNEL_SENT_CLOSE)
		channel_finished(channel);
	      else
		channel_close(channel);
	    }
	}
      else
	{
	  werror("CLOSE on non-existant channel %i\n",
		 channel_number);
	  SSH_CONNECTION_ERROR(connection, "CLOSE on non-existant channel");
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_CLOSE message");
}

static void
handle_open_confirm(struct ssh_connection *connection,
		    struct simple_buffer *buffer)
{
  uint32_t local_channel_number;
  uint32_t remote_channel_number;  
  uint32_t window_size;
  uint32_t max_packet;
  
  if (parse_uint32(buffer, &local_channel_number)
      && parse_uint32(buffer, &remote_channel_number)
      && parse_uint32(buffer, &window_size)
      && parse_uint32(buffer, &max_packet)
      && parse_eod(buffer))
    {
      struct ssh_channel *channel
	= ssh_connection_lookup_channel(connection,
					local_channel_number,
					CHANNEL_ALLOC_SENT_OPEN);

      if (channel) 
	{
	  channel->remote_channel_number = remote_channel_number;
	  channel->send_window_size = window_size;
	  channel->send_max_packet = max_packet;

	  ssh_connection_activate_channel(connection, local_channel_number);
	  CHANNEL_EVENT(channel, CHANNEL_EVENT_CONFIRM);
	}
      else
	{
	  werror("Unexpected SSH_MSG_CHANNEL_OPEN_CONFIRMATION on channel %i\n",
		 local_channel_number);
	  SSH_CONNECTION_ERROR(connection, "Unexpected CHANNEL_OPEN_CONFIRMATION.");
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_OPEN_CONFIRMATION message.");
}

static void
handle_open_failure(struct ssh_connection *connection,
		    struct simple_buffer *buffer)
{
  uint32_t channel_number;
  uint32_t reason;

  const uint8_t *msg;
  uint32_t length;

  const uint8_t *language;
  uint32_t language_length;
  
  if (parse_uint32(buffer, &channel_number)
      && parse_uint32(buffer, &reason)
      && parse_string(buffer, &length, &msg)
      && parse_string(buffer, &language_length, &language)
      && parse_eod(buffer))
    {
      struct ssh_channel *channel =
	ssh_connection_lookup_channel(connection,
				      channel_number,
				      CHANNEL_ALLOC_SENT_OPEN);

      if (channel)
	{
	  /* FIXME: It would be nice to pass the message on. */
	  werror("Channel open for channel %i failed: %ps\n", channel_number, length, msg);

	  CHANNEL_EVENT(channel, CHANNEL_EVENT_DENY);
	  channel_finished(channel);
	}
      else
	werror("Unexpected SSH_MSG_CHANNEL_OPEN_FAILURE on channel %i\n",
	       channel_number);
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_OPEN_FAILURE message.");
}

static void
handle_channel_success(struct ssh_connection *connection,
		       struct simple_buffer *buffer)
{
  uint32_t channel_number;
  struct ssh_channel *channel;
      
  if (parse_uint32(buffer, &channel_number)
      && parse_eod(buffer)
      && (channel = ssh_connection_lookup_channel(connection,
						  channel_number,
						  CHANNEL_ALLOC_ACTIVE)))
    {
      if (channel->request_methods)
	{
	  channel->request_methods->success(channel);
	  return;
	}
      
      if (object_queue_is_empty(&channel->pending_requests))
	{
	  werror("do_channel_success: Unexpected message. Ignoring.\n");
	}
      else
	{
	  struct lsh_object *o
	    = object_queue_remove_head(&channel->pending_requests);
#if 0
	  CAST_SUBTYPE(command_context, ctx, o);
	  
	  COMMAND_RETURN(ctx->c, channel);
#endif
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_SUCCESS message");
}

static void
handle_channel_failure(struct ssh_connection *connection,
		       struct simple_buffer *buffer)
{
  uint32_t channel_number;
  struct ssh_channel *channel;
  
  if (parse_uint32(buffer, &channel_number)
      && parse_eod(buffer)
      && (channel = ssh_connection_lookup_channel(connection,
						  channel_number,
						  CHANNEL_ALLOC_ACTIVE)))
    {
      if (channel->request_methods)
	{
	  channel->request_methods->failure(channel);
	  return;
	}

      if (object_queue_is_empty(&channel->pending_requests))
	{
	  werror("do_channel_failure: No handler. Ignoring.\n");
	}
      else
	{
	  struct lsh_object *o
	    = object_queue_remove_head(&channel->pending_requests);

	  werror("Channel request failed. Closing channel.\n");

	  channel_close(channel);
#if 0
	  static const struct exception channel_request_exception =
	    STATIC_EXCEPTION(EXC_CHANNEL_REQUEST, 0, "Channel request failed");

	  CAST_SUBTYPE(command_context, ctx, o);
	  
	  EXCEPTION_RAISE(ctx->e, &channel_request_exception);
#endif
	}
    }
  else
    SSH_CONNECTION_ERROR(connection, "Invalid CHANNEL_FAILURE message.");
}

void
channel_close(struct ssh_channel *channel)
{
  if (! (channel->flags & CHANNEL_SENT_CLOSE))
    {
      verbose("Sending CLOSE on channel %i\n", channel->remote_channel_number);

      channel->flags |= CHANNEL_SENT_CLOSE;
      
      SSH_CONNECTION_WRITE(channel->connection, format_channel_close(channel));

      if (channel->flags & CHANNEL_RECEIVED_CLOSE)
	channel_finished(channel);
    }
}

/* Implement the close logic */
void
channel_maybe_close(struct ssh_channel *channel)
{
  trace("channel_maybe_close: flags = %xi, sources = %i, sinks = %i.\n",
	channel->flags, channel->sources, channel->sinks);

  /* We need not check channel->sources; that's done by the code that
     sends CHANNEL_EOF and sets the corresponding flag. We should
     check channel->sinks, unless CHANNEL_NO_WAIT_FOR_EOF is set. */
  if (!(channel->flags & CHANNEL_SENT_CLOSE)
      && (channel->flags & CHANNEL_SENT_EOF)
      && ((channel->flags & CHANNEL_NO_WAIT_FOR_EOF)
	  || ((channel->flags & CHANNEL_RECEIVED_EOF)
	      && !channel->sinks)))
    channel_close(channel);      
}

void
channel_eof(struct ssh_channel *channel)
{
  if (! (channel->flags &
	 (CHANNEL_SENT_EOF | CHANNEL_SENT_CLOSE | CHANNEL_RECEIVED_CLOSE)))
    {
      verbose("Sending EOF on channel %i\n", channel->remote_channel_number);

      channel->flags |= CHANNEL_SENT_EOF;
      SSH_CONNECTION_WRITE(channel->connection,
			   format_channel_eof(channel->remote_channel_number));

      channel_maybe_close(channel);
    }
}

void
init_channel(struct ssh_channel *channel,
	     void (*kill)(struct resource *),
	     void (*event)(struct ssh_channel *, enum channel_event))
{
  init_resource(&channel->super, kill);

  channel->connection = NULL;
  
  channel->flags = 0;
  channel->sources = 0;
  channel->sinks = 0;

  channel->request_methods = NULL;
  channel->request_types = NULL;
  
  channel->receive = NULL;
  channel->send_adjust = NULL;

  channel->event = event;

  object_queue_init(&channel->pending_requests);
  object_queue_init(&channel->active_requests);
}

/* Returns zero if message type is unimplemented */
int
channel_packet_handler(struct ssh_connection *connection,
		       uint32_t length, const uint8_t *packet)
{
  struct simple_buffer buffer;

  simple_buffer_init(&buffer, length, packet);
  unsigned msg;

  if (!parse_uint8(&buffer, &msg))
    fatal("Internal error.\n");
  
  trace("channel_packet_handler, received %T (%i)\n", msg, msg);
  debug("packet contents: %xs\n", length, packet);

  switch (msg)
    {
    default:
      return 0;
    case SSH_MSG_GLOBAL_REQUEST:
      handle_global_request(connection, &buffer);
      break;
    case SSH_MSG_REQUEST_SUCCESS:
      handle_global_success(connection, &buffer);
      break;
    case SSH_MSG_REQUEST_FAILURE:
      handle_global_failure(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_OPEN:
      handle_channel_open(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_OPEN_CONFIRMATION:
      handle_open_confirm(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_OPEN_FAILURE:
      handle_open_failure(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_WINDOW_ADJUST:
      handle_adjust_window(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_DATA:
      handle_channel_data(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_EXTENDED_DATA:
      handle_channel_extended_data(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_EOF:
      handle_channel_eof(connection, &buffer);
      break;
    case SSH_MSG_CHANNEL_CLOSE:
      handle_channel_close(connection, &buffer);       
      break;
    case SSH_MSG_CHANNEL_REQUEST:
      handle_channel_request(connection, &buffer); 
      break;
    case SSH_MSG_CHANNEL_SUCCESS:
      handle_channel_success(connection, &buffer); 
      break;
    case SSH_MSG_CHANNEL_FAILURE:
      handle_channel_failure(connection, &buffer); 
      break;
    }
  return 1;
}

void
channel_transmit_data(struct ssh_channel *channel,
		      uint32_t length, const uint8_t *data)
{
  assert(length <= channel->send_window_size);
  assert(length <= channel->send_max_packet);
  channel->send_window_size -= length;

  SSH_CONNECTION_WRITE(channel->connection,
		       format_channel_data(channel->remote_channel_number,
					   length, data));
}

void
channel_transmit_extended(struct ssh_channel *channel,
			  uint32_t type,
			  uint32_t length, const uint8_t *data)
{
  assert(length <= channel->send_window_size);
  assert(length <= channel->send_max_packet);
  channel->send_window_size -= length;

  SSH_CONNECTION_WRITE(channel->connection,
		       format_channel_extended_data(
			 channel->remote_channel_number,
			 type, length, data));
}

int
channel_open_new_v(struct ssh_connection *connection,
		   struct ssh_channel *channel,
		   uint32_t type_length, const uint8_t *type,
		   const char *format, va_list args)
{
  struct lsh_string *request;
  uint32_t l1, l2;
  va_list args_copy;

  int index
    = ssh_connection_alloc_channel(connection, CHANNEL_ALLOC_SENT_OPEN);
  if (index < 0)
    {
      /* We have run out of channel numbers. */
      werror("channel_open_new: ssh_connection_alloc_channel failed\n");
      return 0;
    }

  ssh_connection_register_channel(connection, index, channel);
  
  check_rec_max_packet(channel);
  
#define OPEN_FORMAT "%c%s%i%i%i"
#define OPEN_ARGS SSH_MSG_CHANNEL_OPEN, type_length, type, \
  channel->local_channel_number, \
  channel->rec_window_size, channel->rec_max_packet

  va_copy(args_copy, args);

  l1 = ssh_format_length(OPEN_FORMAT, OPEN_ARGS);
  
  l2 = ssh_vformat_length(format, args);

  request = lsh_string_alloc(l1 + l2);

  ssh_format_write(OPEN_FORMAT, request, 0, OPEN_ARGS);

  ssh_vformat_write(format, request, l1, args_copy);
  va_end(args_copy);

#undef OPEN_FORMAT
#undef OPEN_ARGS
  
  SSH_CONNECTION_WRITE(connection, request);
  
  return 1;
}

int
channel_open_new_type(struct ssh_connection *connection,
		      struct ssh_channel *channel,
		      uint32_t type_length, const uint8_t *type,
		      const char *format, ...)
{
  va_list args;
  int res;
  
  va_start(args, format);
  res = channel_open_new_v(connection, channel,
			   type_length, type,
			   format, args);
  va_end(args);
  return res;
}

#if 0
struct lsh_string *
format_channel_request_i(struct channel_request_info *info,
			 struct ssh_channel *channel,
			 uint32_t args_length, const uint8_t *args_data)
{
  return ssh_format("%c%i%s%c%ls", SSH_MSG_CHANNEL_REQUEST,
		    channel->remote_channel_number,
		    info->type_length, info->type_data,
		    info->want_reply,
		    args_length, args_data);
}
#endif

void
channel_send_request(struct ssh_channel *channel, int type,
		     int close_on_error,
		     const char *format, ...)
{
  va_list args;
  uint32_t l1, l2;
  struct lsh_string *packet;

#define REQUEST_FORMAT "%c%i%a%c"
#define REQUEST_ARGS SSH_MSG_CHANNEL_REQUEST, channel->remote_channel_number, \
  type, close_on_error

  l1 = ssh_format_length(REQUEST_FORMAT, REQUEST_ARGS);
  
  va_start(args, format);
  l2 = ssh_vformat_length(format, args);
  va_end(args);

  packet = lsh_string_alloc(l1 + l2);

  ssh_format_write(REQUEST_FORMAT, packet, 0, REQUEST_ARGS);

  va_start(args, format);
  ssh_vformat_write(format, packet, l1, args);
  va_end(args);

#undef REQUEST_FORMAT
#undef REQUEST_ARGS

  SSH_CONNECTION_WRITE(channel->connection, packet);

  /* FIXME: What context do we really need? So far, for all requests
     we send, we want to either ignore the result (and use want_reply
     = 0), or we close the channel if the request fails. pty request
     will be the first user of more context. */
  
  if (close_on_error)
    {
      /* For now, we use the channel itself as a placeholder. */
      object_queue_add_tail(&channel->pending_requests,
			    &channel->super.super);
    }
}

void
channel_send_global_request(struct ssh_connection *connection, int type,
			    struct command_context *ctx,
			    const char *format, ...)
{
  va_list args;
  uint32_t l1, l2;
  struct lsh_string *packet;
  uint8_t want_reply;

#define REQUEST_FORMAT "%c%a%c"
#define REQUEST_ARGS SSH_MSG_GLOBAL_REQUEST, type, want_reply

  want_reply = (ctx != NULL);

  l1 = ssh_format_length(REQUEST_FORMAT, REQUEST_ARGS);
  
  va_start(args, format);
  l2 = ssh_vformat_length(format, args);
  va_end(args);

  packet = lsh_string_alloc(l1 + l2);

  ssh_format_write(REQUEST_FORMAT, packet, 0, REQUEST_ARGS);

  va_start(args, format);
  ssh_vformat_write(format, packet, l1, args);
  va_end(args);

#undef REQUEST_FORMAT
#undef REQUEST_ARGS
  
  SSH_CONNECTION_WRITE(connection, packet);
  if (want_reply)
    {
      assert(ctx);
      object_queue_add_tail(&connection->pending_global_requests,
			    &ctx->super);
    }      
}
