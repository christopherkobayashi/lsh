/* io_commands.c
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
#include <errno.h>

/* For STDIN_FILENO */
#include <unistd.h>

#include "io_commands.h"

#include "command.h"
#include "werror.h"
#include "xalloc.h"

#include "io_commands.c.x"

/* (listen_tcp_command address callback)

   Returns a resource. The callback gets a listen-value as argument.
*/

/* GABA:
   (class
     (name io_port)
     (super resource)
     (vars
       (fd . int)
       (callback object command)))
*/

static void
kill_io_port(struct resource *s)
{
  CAST(io_port, self, s);
  if (self->super.alive)
    {
      self->super.alive = 0;
      io_close_fd(self->fd);
      self->fd = -1;
    }
};

static struct io_port *
make_io_port(int fd, struct command *callback)
{
  NEW(io_port, self);
  init_resource(&self->super, kill_io_port);

  io_register_fd(fd, "listen port");

  self->fd = fd;
  self->callback = callback;

  return self;
}

static void *
oop_io_port_accept(oop_source *source UNUSED,
		   int fd, oop_event event, void *state)
{
  CAST(io_port, self, (struct lsh_object *) state);

#if WITH_IPV6
  struct sockaddr_storage peer;
#else
  struct sockaddr_in peer;
#endif

  socklen_t peer_length = sizeof(peer);
  int s;
  
  assert(event == OOP_READ);
  assert(self->fd == fd);

  s = accept(self->fd, (struct sockaddr *) &peer, &peer_length);
  if (s < 0)
    {
      werror("accept failed, fd = %i: %e\n", self->fd, errno);
    }
  else
    COMMAND_CALL(self->callback,
		 make_listen_value(s, sockaddr2info(peer_length,
						    (struct sockaddr *)&peer)),
		 &discard_continuation, &ignore_exception_handler);

  return OOP_CONTINUE;  
}

DEFINE_COMMAND2(listen_tcp_command)
     (struct command_2 *self UNUSED,
      struct lsh_object *a1,
      struct lsh_object *a2,
      struct command_continuation *c,
      struct exception_handler *e)
{
  CAST(address_info, a, a1);
  CAST_SUBTYPE(command, callback, a2);
  struct sockaddr *addr;
  socklen_t addr_length;
  struct io_port *port;
  int yes = 1;
  int fd;

  /* FIXME: Use something simpler than address_info2sockaddr, when we
     want to handle numerical addresses only. */
  addr = address_info2sockaddr(&addr_length, a, NULL, 0);
  if (!addr)
    {
      EXCEPTION_RAISE(e, make_exception(EXC_RESOLVE, 0, "invalid address"));
      return;
    }

  fd = socket(addr->sa_family, SOCK_STREAM, 0);
  
  if (fd < 0)
    {
      EXCEPTION_RAISE(e, make_exception(EXC_IO_ERROR, errno, "socket failed"));
      lsh_space_free(addr);
      return;
    }

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0)
    werror("setsockopt failed: %e\n", errno);

  if (bind(fd, addr, addr_length) < 0)
    {
      EXCEPTION_RAISE(e, make_exception(EXC_IO_ERROR, errno, "bind failed"));
      lsh_space_free(addr);
      close(fd);
      return;
    }

  lsh_space_free(addr);

  if (listen(fd, 256) < 0)
    {
      EXCEPTION_RAISE(e, make_exception(EXC_IO_ERROR, errno, "listen failed"));
      close(fd);
      return;
    }    

  port = make_io_port(fd, callback);
  global_oop_source->on_fd(global_oop_source, fd, OOP_READ,
			   oop_io_port_accept, port);

  COMMAND_RETURN(c, port);
}


#if 0

#include "connection.h"
/* For lsh_get_cstring */
#include "format.h"
#include "io.h"
#include "queue.h"
#include "werror.h"
#include "xalloc.h"


#if WITH_TCPWRAPPERS
#include <tcpd.h> 

/* This seems to be necessary on some systems */
#include <syslog.h>

int allow_severity = LOG_INFO;
int deny_severity = LOG_INFO;

#endif /* WITH_TCPWRAPPERS */

#include "io_commands.c.x"

/* (listen callback fd) */
DEFINE_COMMAND2(listen_command)
     (struct command_2 *s UNUSED,
      struct lsh_object *a1,
      struct lsh_object *a2,
      struct command_continuation *c,
      struct exception_handler *e)
{
  CAST_SUBTYPE(command, callback, a1);
  CAST(lsh_fd, fd, a2);

  if (io_listen(fd, make_listen_callback(callback, e)))
    COMMAND_RETURN(c, fd);
  else
    EXCEPTION_RAISE(e, make_io_exception(EXC_IO_LISTEN,
					 NULL, errno, NULL));
}

static struct exception resolve_exception =
STATIC_EXCEPTION(EXC_RESOLVE, "address could not be resolved");

DEFINE_COMMAND(bind_address_command)
     (struct command *self UNUSED,
      struct lsh_object *x,
      struct command_continuation *c,
      struct exception_handler *e)
{
  CAST(address_info, a, x);
  struct sockaddr *addr;
  socklen_t addr_length;
  
  struct lsh_fd *fd;

  addr = address_info2sockaddr(&addr_length, a, NULL, 0);
  if (!addr)
    {
      EXCEPTION_RAISE(e, &resolve_exception);
      return;
    }

  fd = io_bind_sockaddr(addr, addr_length, e);
  lsh_space_free(addr);
  
  if (fd)
    COMMAND_RETURN(c, fd);
  else
    /* FIXME: Do we need an EXC_IO_BIND ? */
    EXCEPTION_RAISE(e, make_io_exception(EXC_IO_LISTEN,
					 NULL, errno, NULL));
}

DEFINE_COMMAND(bind_local_command)
     (struct command *self UNUSED,
      struct lsh_object *x,
      struct command_continuation *c,
      struct exception_handler *e)
{
  CAST(local_info, info, x);
  struct lsh_fd *fd = io_bind_local(info, e);

  if (fd)
    COMMAND_RETURN(c, fd);
  else
    /* FIXME: Do we need an EXC_IO_BIND ? */
    EXCEPTION_RAISE(e, make_io_exception(EXC_IO_LISTEN,
					 NULL, errno, NULL));
}


/* GABA:
   (class
     (name connect_continuation)
     (super command_continuation)
     (vars
       (target object address_info)
       (up object command_continuation)))
*/

static void
do_connect_continuation(struct command_continuation *c,
			struct lsh_object *x)
{
  CAST(connect_continuation, self, c);
  CAST(lsh_fd, fd, x);

  COMMAND_RETURN(self->up, make_listen_value(fd, self->target, fd2info(fd,0)));
}

/* FIXME: Return an io_callback instead */
static struct command_continuation *
make_connect_continuation(struct address_info *target,
			  struct command_continuation *up)
{
  NEW(connect_continuation, self);
  self->super.c = do_connect_continuation;
  self->target = target;
  self->up = up;

  return &self->super;
}
     
static void
do_connect(struct address_info *a,
	   struct resource_list *resources,
	   struct command_continuation *c,
	   struct exception_handler *e)
{
  struct sockaddr *addr;
  socklen_t addr_length;
  struct lsh_fd *fd;

  /* Address must specify a host */
  assert(a->ip);

  /* Performs dns lookups */
  addr = address_info2sockaddr(&addr_length, a, NULL, 1);
  if (!addr)
    {
      EXCEPTION_RAISE(e, &resolve_exception);
      return;
    }

  /* If the name is canonicalized in any way, we should pass the
   * canonical name to make_connect_continuation .*/
  fd = io_connect(addr, addr_length, 
		  make_connect_callback(make_connect_continuation(a, c)),
		  e);
  lsh_space_free(addr);

  if (!fd)
    {
      EXCEPTION_RAISE(e, make_io_exception(EXC_IO_CONNECT, NULL, errno, NULL));
      return;
    }

  if (resources)
    remember_resource(resources,
		      &fd->super);
}


/* Connect variant, taking a connection object as argument (used for
 * rememembering the connected fd).
 *
 * (connect port connection) -> fd */

/* GABA:
   (class
     (name connect_port)
     (super command)
     (vars
       (target object address_info)))
*/

static void
do_connect_port(struct command *s,
		struct lsh_object *x,
		struct command_continuation *c,
		struct exception_handler *e)
{
  CAST(connect_port, self, s);
  CAST(ssh_connection, connection, x);
  
  do_connect(self->target, connection->resources, c, e);
}


struct command *
make_connect_port(struct address_info *target)
{
  NEW(connect_port, self);
  self->super.call = do_connect_port;
  self->target = target;

  return &self->super;
}

/* (connect address) */
DEFINE_COMMAND(connect_simple_command)
     (struct command *self UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e)
{
  CAST(address_info, address, a);

  do_connect(address, NULL, c, e);
}

/* (connect addresses) */
DEFINE_COMMAND(connect_list_command)
     (struct command *self UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e)
{
  CAST(connect_list_state, addresses, a);

  io_connect_list(addresses,
		  /* FIXME: Fix handshake_command to take a plain fd
		     as argument, not a listen value. */
		  make_connect_continuation(NULL, c), e);
}

/* (connect_connection connection port) */
DEFINE_COMMAND2(connect_connection_command)
     (struct command_2 *self UNUSED,
      struct lsh_object *a1,
      struct lsh_object *a2,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST(ssh_connection, connection, a1);
  CAST(address_info, address, a2);

  do_connect(address, connection->resources, c, e);
}


DEFINE_COMMAND(connect_local_command)
     (struct command *s UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST(local_info, info, a);
  
  static struct exception gateway_exception =
    STATIC_EXCEPTION(EXC_IO_CONNECT, "no usable gateway socket found");
  
  struct lsh_fd *fd = io_connect_local(info,
				       make_connect_continuation(NULL, c),
				       e);
  
  if (!fd)
    EXCEPTION_RAISE(e, &gateway_exception);
}



/* Takes a listen_value as argument, logs the peer address, and
 * returns the fd object. */

DEFINE_COMMAND(io_log_peer_command)
     (struct command *s UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST(listen_value, lv, a);

  verbose("Accepting connection from %S, port %i\n",
	  lv->peer->ip, lv->peer->port);

  COMMAND_RETURN(c, lv);
}






/* GABA:
   (class
     (name tcp_wrapper)
     (super command)
     (vars
       (name string)
       (msg string)))
*/


/* TCP wrapper function, replaces io_log_peer if used */

static void
do_tcp_wrapper(struct command *s UNUSED,
	       struct lsh_object *a,
	       struct command_continuation *c,
	       struct exception_handler *e UNUSED)
{
  CAST(listen_value, lv, a);

#if WITH_TCPWRAPPERS  

  CAST(tcp_wrapper, self, s);

  struct request_info res;

  request_init(&res,
	       RQ_DAEMON, lsh_get_cstring(self->name), /* Service name */
	       RQ_FILE, lv->fd->fd,   /* connection fd */
	       0);                /* No more arguments */
  
  fromhost(&res); /* Lookup information before */
  
  if (!hosts_access(&res)) /* Connection OK? */
    { 
      /* FIXME: Should we say anything to the other side? */

      verbose("Denying access for %z@%z (%z)\n",
	      eval_user(&res),
	      eval_hostname(res.client),
	      eval_hostaddr(res.client)
	      );


      
      io_write(lv->fd, 1024, NULL);
      A_WRITE(&lv->fd->write_buffer->super, 
	      lsh_string_dup(self->msg));

      close_fd_nicely(lv->fd);

      return;
    }

#endif /* WITH_TCPWRAPPERS */
  
  verbose("Accepting connection from %S, port %i\n",
	  lv->peer->ip, lv->peer->port);
  
  COMMAND_RETURN(c, lv);
}



struct command *
make_tcp_wrapper(struct lsh_string *name, struct lsh_string *msg )
{
  NEW(tcp_wrapper, self);
  self->super.call = do_tcp_wrapper;
  self->name = name;
  self->msg = msg;

  return &self->super;
}

/* ***
 *
 * (lambda (backend connection port)
     (listen backend connection port
             (lambda (peer)
                (start-io peer (request-forwarded-tcpip connection peer)))))
 */
#endif
