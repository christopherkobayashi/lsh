/* channel_forward.h
 *
 * General channel type for forwarding data to an fd
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998, 2001 Bal�zs Scheidler, Niels M�ller
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
#include <errno.h>
#include <string.h>

#include "channel_forward.h"

#include "io.h"
#include "lsh_string.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#define GABA_DEFINE
#include "channel_forward.h.x"
#undef GABA_DEFINE

/* We don't use channel_read_state_close and channel_write_state_close. */
static void
do_kill_channel_forward(struct resource *s)
{  
  CAST_SUBTYPE(channel_forward, self, s);
  if (self->super.super.alive)
    {
      trace("do_kill_channel_forward\n");
      
      self->super.super.alive = 0;
      io_close_fd(self->read.fd);
    }
}

void
channel_forward_shutdown(struct channel_forward *self)
{
  if (shutdown (self->write.fd, SHUT_WR) < 0)
    werror("close_fd_write, shutdown failed, %e\n", errno);

  assert(self->super.sinks);
  self->super.sinks--;
  channel_maybe_close(&self->super);  
}

static void *
oop_write_socket(oop_source *source UNUSED,
		 int fd, oop_event event, void *state)
{
  CAST_SUBTYPE(channel_forward, self, (struct lsh_object *) state);
  
  assert(event == OOP_WRITE);
  assert(fd == self->write.fd);

  if (channel_io_flush(&self->super, &self->write) == CHANNEL_IO_EOF)
    channel_forward_shutdown(self);

  return OOP_CONTINUE;
}

void
channel_forward_write(struct channel_forward *self,
		      uint32_t length, const uint8_t *data)
{
  if (channel_io_write(&self->super, &self->write, oop_write_socket,
		       length, data) == CHANNEL_IO_EOF)
    channel_forward_shutdown(self);
}

static void
do_channel_forward_receive(struct ssh_channel *s, int type,
			   uint32_t length, const uint8_t *data)
{
  CAST_SUBTYPE(channel_forward, self, s);

  if (type != CHANNEL_DATA)
    werror("Ignoring unexpected extended data on forwarded channel.\n");
  else
    channel_forward_write(self, length, data);
}

static void *
oop_read_socket(oop_source *source UNUSED,
		int fd, oop_event event, void *state)
{
  CAST_SUBTYPE(channel_forward, self, (struct lsh_object *) state);
  uint32_t done;
  
  assert(fd == self->read.fd);
  assert(event == OOP_READ);

  if (channel_io_read(&self->super, &self->read, &done) == CHANNEL_IO_OK
      && done > 0)
    channel_transmit_data(&self->super,
			  done, lsh_string_data(self->read.buffer));

  return OOP_CONTINUE;
}

/* We may send more data */
static void
do_channel_forward_send_adjust(struct ssh_channel *s,
			       uint32_t i UNUSED)
{
  CAST_SUBTYPE(channel_forward, self, s);

  channel_io_start_read(&self->super, &self->read, oop_read_socket);
}

void
channel_forward_start_read(struct channel_forward *self)
{
  if (self->super.send_window_size)
    channel_io_start_read(&self->super, &self->read, oop_read_socket);
}

/* NOTE: Because this function is called by
 * do_open_forwarded_tcpip_continuation, the same restrictions apply.
 * I.e we can not assume that the channel is completely initialized
 * (channel_open_continuation has not yet done its work), and we can't
 * send any packets. */
void
channel_forward_start_io(struct channel_forward *self)
{
  self->super.receive = do_channel_forward_receive;
  self->super.send_adjust = do_channel_forward_send_adjust;

  self->super.sources++;
  self->super.sinks ++;

  channel_forward_start_read(self);
}

static void
do_channel_forward_event(struct ssh_channel *s, enum channel_event event)
{
  CAST_SUBTYPE(channel_forward, self, s);

  switch(event)
    {
    case CHANNEL_EVENT_CONFIRM:
      channel_forward_start_io(self);
      break;
    case CHANNEL_EVENT_DENY:
    case CHANNEL_EVENT_CLOSE:
      /* Do nothing */
      break;      
    case CHANNEL_EVENT_EOF:
      if (!self->write.state->length)
	channel_forward_shutdown(self);
      break;
    case CHANNEL_EVENT_STOP:
      channel_io_stop_read(&self->read);
      break;
    case CHANNEL_EVENT_START:
      channel_forward_start_read(self);
      break;
    }
}

#define FORWARD_READ_BUFFER_SIZE 0x4000

/* NOTE: It's the caller's responsibility to call io_register_fd. */
void
init_channel_forward(struct channel_forward *self,
		     int socket, uint32_t initial_window,
		     void (*event)(struct ssh_channel *, enum channel_event))
{
  if (!event)
    event = do_channel_forward_event;

  init_channel(&self->super,
	       do_kill_channel_forward, event);

  /* The rest of the callbacks are not set up until
   * channel_forward_start_io. */

  self->super.rec_window_size = initial_window;

  /* FIXME: Make maximum packet size configurable. */
  self->super.rec_max_packet = SSH_MAX_PACKET;

  init_channel_read_state(&self->read, socket, FORWARD_READ_BUFFER_SIZE);
  init_channel_write_state(&self->write, socket, initial_window);
}

/* NOTE: It's the caller's responsibility to call io_register_fd. */
struct channel_forward *
make_channel_forward(int socket, uint32_t initial_window)
{
  NEW(channel_forward, self);
  init_channel_forward(self, socket, initial_window, NULL);
  
  return self;
}

/* Used by the party requesting tcp forwarding, i.e. when a socket is
 * already open, and we have asked the other end to forward it. Takes
 * a channel as argument, and connects it to the socket. Returns the
 * channel. */

DEFINE_COMMAND(forward_start_io_command)
     (struct command *s UNUSED,
      struct lsh_object *x,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST_SUBTYPE(channel_forward, channel, x);

  assert(channel);
  
  channel_forward_start_io(channel);

  COMMAND_RETURN(c, channel);  
}
