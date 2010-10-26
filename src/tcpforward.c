/* tcpforward.c
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998, 2005, 2008 Bal�zs Scheidler, Niels M�ller
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

#include "tcpforward.h"

#include "channel_forward.h"
#include "format.h"
#include "io.h"
#include "lsh_string.h"
#include "ssh.h"

#include "werror.h"


#define GABA_DEFINE
#include "tcpforward.h.x"
#undef GABA_DEFINE

#include "tcpforward.c.x"

/* Structures used to keep track of forwarded ports */

struct forwarded_port *
tcpforward_lookup(struct object_queue *q,
		  uint32_t length, const uint8_t *ip, uint32_t port)
{
  FOR_OBJECT_QUEUE(q, n)
    {
      CAST_SUBTYPE(forwarded_port, f, n);
      
      if ( (port == f->address->port)
	   && lsh_string_eq_l(f->address->ip, length, ip) )
	return f;
    }
  return NULL;
}

struct forwarded_port *
tcpforward_remove(struct object_queue *q,
		  uint32_t length, const uint8_t *ip, uint32_t port)
{
  FOR_OBJECT_QUEUE(q, n)
    {
      CAST_SUBTYPE(forwarded_port, p, n);

      if ( (port == p->address->port)
	   && lsh_string_eq_l(p->address->ip, length, ip) )
	{
	  FOR_OBJECT_QUEUE_REMOVE(q, n);
	  return p;
	}
    }
  return NULL;
}

int
tcpforward_remove_port(struct object_queue *q, struct forwarded_port *port)
{
  FOR_OBJECT_QUEUE(q, n)
    {
      CAST_SUBTYPE(forwarded_port, f, n);
      
      if (port == f)
	{
	  FOR_OBJECT_QUEUE_REMOVE(q, n);
	  return 1;
	}
    }
  return 0;
}

/* GABA:
   (class
     (name tcpforward_listen_port)
     (super io_listen_port)
     (vars
       (type . int)
       (connection object ssh_connection)
       (forward const object address_info)))       
*/

static void
do_tcpforward_listen_port_accept(struct io_listen_port *s,
				 int fd,
				 socklen_t addr_length,
				 const struct sockaddr *addr)  
{
  CAST(tcpforward_listen_port, self, s);

  struct channel_forward *channel;
  struct address_info *peer = sockaddr2info(addr_length, addr);
  trace("forward_local_port\n");

  io_register_fd(fd, "forwarded socket");
  channel = make_channel_forward(fd, TCPIP_WINDOW_SIZE);

  if (!channel_open_new_type(self->connection, &channel->super,
			     ATOM_LD(self->type),
			     "%S%i%S%i",
			     self->forward->ip, self->forward->port,
			     peer->ip, peer->port))
    {
      werror("tcpforward_listen_port: Allocating a local channel number failed.");
      KILL_RESOURCE(&channel->super.super);
    }
}

struct io_listen_port *
make_tcpforward_listen_port(struct ssh_connection *connection,
			    int type,
			    const struct address_info *local,
			    const struct address_info *forward)
{
  struct sockaddr *addr;
  socklen_t addr_length;
  int fd;

  trace("make_tcpforward_listen_port: Local port: %S:%i, target port: %S:%i\n",
	local->ip, local->port, forward->ip, forward->port);

  addr = io_make_sockaddr(&addr_length,
			  lsh_get_cstring(local->ip), local->port);
  if (!addr)
    return NULL;

  fd = io_bind_sockaddr((struct sockaddr *) addr, addr_length);
  if (fd < 0)
    return NULL;

  {
    NEW(tcpforward_listen_port, self);
    init_io_listen_port(&self->super, fd, do_tcpforward_listen_port_accept);

    self->connection = connection;
    self->type = type;
    self->forward = forward;

    return &self->super;
  }
}

/* GABA:
   (class
     (name tcpforward_connect_state)
     (super io_connect_state)
     (vars
       (info const object channel_open_info)))
*/

static void
tcpforward_connect_done(struct io_connect_state *s, int fd)
{
  CAST(tcpforward_connect_state, self, s);

  struct channel_forward *channel
    = make_channel_forward(fd, TCPIP_WINDOW_SIZE);
  
  channel_open_confirm(self->info, &channel->super);
  channel_forward_start_io(channel);  
}

static void
tcpforward_connect_error(struct io_connect_state *s, int error)
{
  CAST(tcpforward_connect_state, self, s);
  
  werror("Connection failed: %s\n", STRERROR(error));
  channel_open_deny(self->info,
		    SSH_OPEN_CONNECT_FAILED, "Connection failed");
}

struct resource *
tcpforward_connect(const struct address_info *a,
		   const struct channel_open_info *info)
{
  struct sockaddr *addr;
  socklen_t addr_length;

  addr = io_make_sockaddr(&addr_length, lsh_get_cstring(a->ip), a->port);
  if (!addr)
    {
      channel_open_deny(info, SSH_OPEN_CONNECT_FAILED, "Invalid address");
      return NULL;
    }

  {
    NEW(tcpforward_connect_state, self);
    init_io_connect_state(&self->super,
			  tcpforward_connect_done,
			  tcpforward_connect_error);
    int res;
    
    self->info = info;

    res = io_connect(&self->super, addr_length, addr);
    lsh_space_free(addr);

    if (!res)
      {
	channel_open_deny(info, SSH_OPEN_CONNECT_FAILED, STRERROR(res));
	return NULL;
      }
    return &self->super.super.super;
  }
}
