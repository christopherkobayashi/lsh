/* lshd.c
 *
 * Main server program.
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005 Niels M�ller
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

#include <signal.h>

#include <unistd.h>
#include <netinet/in.h>

#include <oop.h>

#include "nettle/macros.h"

#include "algorithms.h"
#include "crypto.h"
#include "environ.h"
#include "daemon.h"
#include "format.h"
#include "io.h"
#include "keyexchange.h"
#include "lsh_string.h"
#include "parse.h"
#include "randomness.h"
#include "server.h"
#include "service.h"
#include "ssh.h"
#include "transport_forward.h"
#include "version.h"
#include "werror.h"
#include "xalloc.h"

#include "lshd.c.x"

#define SERVICE_WRITE_THRESHOLD 1000
#define SERVICE_WRITE_BUFFER_SIZE (3 * SSH_MAX_PACKET)

/* Information shared by several connections */
/* GABA:
   (class
     (name configuration)
     (super transport_context)
     (vars
       (keys object alist)
       ; For now, a list { name, program, name, program, NULL }       
       (services . "const char **")))
*/

/* Connection */
static void
kill_lshd_connection(struct resource *s)
{
  CAST(transport_forward, self, s);
  if (self->super.super.alive)
    {
      transport_forward_kill(self);
      self->super.super.alive = 0;
    }
}

static int
lshd_packet_handler(struct transport_connection *connection,
		    uint32_t seqno, uint32_t length, const uint8_t *packet);

/* Used only until the service is started. */
static int
lshd_event_handler(struct transport_connection *connection,
		   enum transport_event event)
{
  switch (event)
    {
    case TRANSPORT_EVENT_KEYEXCHANGE_COMPLETE:
      connection->packet_handler = lshd_packet_handler;
      break;

    case TRANSPORT_EVENT_START_APPLICATION:
    case TRANSPORT_EVENT_STOP_APPLICATION:
    case TRANSPORT_EVENT_CLOSE:
    case TRANSPORT_EVENT_PUSH:
      /* Do nothing */
      break;
    }
  return 0;
}

static struct transport_forward *
make_lshd_connection(struct configuration *config, int input, int output)
{
  struct transport_forward *self
    = make_transport_forward(kill_lshd_connection,
			     &config->super,
			     input, output,
			     lshd_event_handler);
  return self;
};

static void
lshd_line_handler(struct transport_connection *connection,
		  uint32_t length, const uint8_t *line)
{
  verbose("Client version string: %ps\n", length, line);

  /* Line must start with "SSH-2.0-" (we may need to allow "SSH-1.99"
     as well). */
  if (length < 8 || 0 != memcmp(line, "SSH-2.0-", 4))
    {
      transport_disconnect(connection, 0, "Bad version string.");
      return;
    }

  connection->kex.version[0] = ssh_format("%ls", length, line);
  connection->line_handler = NULL;
}

/* FIXME: Duplicates server_session.c: lookup_subsystem. */
static const char *
lookup_service(const char **services,
	       uint32_t length, const uint8_t *name)
{
  unsigned i;
  if (memchr(name, 0, length))
    return NULL;

  for (i = 0; services[i]; i+=2)
    {
      assert(services[i+1]);
      if ((length == strlen(services[i]))
	  && !memcmp(name, services[i], length))
	return services[i+1];
    }
  return NULL;
}

static struct lsh_string *
format_service_accept(uint32_t name_length, const uint8_t *name)
{
  return ssh_format("%c%s", SSH_MSG_SERVICE_ACCEPT, name_length, name);
};

static void
lshd_service_request_handler(struct transport_forward *self,
			     uint32_t length, const uint8_t *packet)
{
  struct simple_buffer buffer;
  unsigned msg_number;

  const uint8_t *name;
  uint32_t name_length;

  simple_buffer_init(&buffer, length, packet);

  if (parse_uint8(&buffer, &msg_number)
      && (msg_number == SSH_MSG_SERVICE_REQUEST)
      && parse_string(&buffer, &name_length, &name)
      && parse_eod(&buffer))
    {
      CAST(configuration, config, self->super.ctx);
      const char *program = lookup_service(config->services,
					   name_length, name);

      if (program)
	{
	  int pipe[2];
	  pid_t child;

	  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe) < 0)
	    {
	      werror("lshd_service_request_handler: socketpair failed: %e\n",
		     errno);
	      transport_disconnect(&self->super,
				   SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
				   "Service could not be started");
	      return;
	    }
	  child = fork();
	  if (child < 0)
	    {
	      werror("lshd_service_request_handler: fork failed: %e\n",
		     errno);
	      close(pipe[0]);
	      close(pipe[1]);
	      transport_disconnect(&self->super,
				   SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
				   "Service could not be started");
	      return;
	    }
	  if (child)
	    {
	      /* Parent process */
	      close(pipe[1]);

	      transport_send_packet(&self->super, TRANSPORT_WRITE_FLAG_PUSH,
				    format_service_accept(name_length, name));

	      /* Setup forwarding. Replaces event_handler and packet_handler. */
	      transport_forward_setup(self, pipe[0], pipe[0]);
	    }
	  else
	    {
	      /* Child process */
	      struct lsh_string *hex;

	      close(pipe[0]);
	      dup2(pipe[1], STDIN_FILENO);
	      dup2(pipe[1], STDOUT_FILENO);
	      close(pipe[1]);

	      hex = ssh_format("%lxS", self->super.session_id);

	      /* FIXME: Pass sufficient information so that
		 $SSH_CLIENT can be set properly. */
	      execl(program, program, "--session-id", lsh_string_data(hex), NULL);

	      werror("lshd_service_request_handler: exec failed: %e\n", errno);
	      _exit(EXIT_FAILURE);
	    }
	}
      else
	transport_disconnect(&self->super,
			     SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
			      "Service not available");
    }
  else
    transport_protocol_error(&self->super, "Invalid SERVICE_REQUEST");
}

/* Handles decrypted packets above the ssh transport layer. Replaced
   after the service exchange is complete. */
static int
lshd_packet_handler(struct transport_connection *connection,
		    uint32_t seqno, uint32_t length, const uint8_t *packet)
{
  CAST(transport_forward, self, connection);

  uint8_t msg;

  werror("Received packet: %xs\n", length, packet);
  assert(length > 0);

  msg = packet[0];

  if (msg == SSH_MSG_SERVICE_REQUEST)
    {
      lshd_service_request_handler(self, length, packet);
    }
  else
    {
      /* FIXME: If for example userauth packets are received before
	 the corresponding service is started, we reply with
	 UNIMPLEMENTED, not DISCONNECT. */
	 
      transport_send_packet(connection, TRANSPORT_WRITE_FLAG_PUSH,
			    format_unimplemented(seqno));
    }
  
  return 1;
}

/* GABA:
   (class
     (name lshd_port)
     (super resource)
     (vars
       (config object configuration)
       (fd . int)))
*/

static void
kill_port(struct resource *s)
{
  CAST(lshd_port, self, s);
  if (self->super.alive)
    {
      self->super.alive = 0;
      io_close_fd(self->fd);
      self->fd = -1;
    }
};

static struct lshd_port *
make_lshd_port(struct configuration *config, int fd)
{
  NEW(lshd_port, self);
  init_resource(&self->super, kill_port);

  io_register_fd(fd, "lshd listen port");
  self->config = config;
  self->fd = fd;

  return self;
}

static void *
lshd_port_accept(oop_source *source UNUSED,
		 int fd, oop_event event, void *state)
{
  CAST(lshd_port, self, (struct lsh_object *) state);
  struct transport_forward *connection;
  struct sockaddr_in peer;
  socklen_t peer_length = sizeof(peer);
  int s;

  assert(event == OOP_READ);
  assert(self->fd == fd);

  s = accept(self->fd, (struct sockaddr *) &peer, &peer_length);
  if (s < 0)
    {
      werror("accept failed: %e\n", errno);
      return OOP_CONTINUE;
    }

  connection = make_lshd_connection(self->config, s, s);
  gc_global(&connection->super.super);

  transport_handshake(&connection->super, make_string("SSH-2.0-lshd-ng"),
		      lshd_line_handler);

  return OOP_CONTINUE;
}

/* FIXME: Should use getaddrinfo, and should allow multiple ports and
   services. At the UI, given interfaces can include explicit port,
   and the -p options gives one or more default ports. */
static int
open_ports(struct configuration *config, struct resource_list *resources,
	   const char *portid)
{
  struct sockaddr_in sin;
  struct lshd_port *port;
  oop_source *source;
  int yes = 1;
  int s;
  /* In network byte order */
  short port_number;

  if (!portid)
    {
      struct servent *se = getservbyname("ssh", "tcp");
      port_number = se ? se->s_port : htons(SSH_DEFAULT_PORT);
    }
  else
    {
      char *end;
      unsigned long n;

      if (!portid[0])
	{
	  werror("Port number is empty.\n");
	  return 0;
	}
      n = strtoul(portid, &end, 10);
      if (end[0] == '\0')
	port_number = htons(n);
      else
	{
	  struct servent *se = getservbyname(portid, "tcp");
	  if (!se)
	    {
	      werror("Port `%s' not found.\n", portid);
	      return 0;
	    }

	  port_number = se->s_port;
	}
    }

  source = config->super.oop;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    {
      werror("socket failed: %e\n", errno);
      return 0;
    }

  io_set_nonblocking(s);
  io_set_close_on_exec(s);

  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (yes)) <0)
    werror("setsockopt SO_REUSEADDR failed: %e\n", errno);

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sin.sin_port = port_number;

  if (bind(s, &sin, sizeof(sin)) < 0)
    {
      werror("bind failed: %e\n", errno);
      return 0;
    }

  if (listen(s, 256) < 0)
    {
      werror("listen failed: %e\n", errno);
      return 0;
    }
  port = make_lshd_port(config, s);
  remember_resource(resources, &port->super);

  source->on_fd(source, s, OOP_READ, lshd_port_accept, port);

  return 1;
}

static struct configuration *
make_configuration(const char *hostkey, oop_source *source)
{
  NEW(configuration, self);
  static const char *services[3];

  services[0] = "ssh-userauth";
  GET_FILE_ENV(services[1], LSHD_USERAUTH);
  services[2] = NULL;

  self->super.is_server = 1;

  self->super.random = make_system_random();

  if (!self->super.random)
    {
      werror("No randomness generator available.\n");
      exit(EXIT_FAILURE);
    }

  self->super.algorithms = all_symmetric_algorithms();
  self->super.oop = source;

  self->keys = make_alist(0, -1);
  if (!read_host_key(hostkey,
		     all_signature_algorithms(self->super.random),
		     self->keys))
    werror("No host key.\n");

  ALIST_SET(self->super.algorithms, ATOM_DIFFIE_HELLMAN_GROUP14_SHA1,
	    &make_server_dh_exchange(make_dh_group14(&crypto_sha1_algorithm),
				     self->keys)->super);

  self->super.kexinit
    = make_simple_kexinit(
      make_int_list(1, ATOM_DIFFIE_HELLMAN_GROUP14_SHA1, -1),
      filter_algorithms(self->keys, default_hostkey_algorithms()),
      default_crypto_algorithms(self->super.algorithms),
      default_mac_algorithms(self->super.algorithms),
      default_compression_algorithms(self->super.algorithms),
      make_int_list(0, -1));

  self->services = services;

  return self;
}


/* GABA:
   (class
     (name pid_file_resource)
     (super resource)
     (vars
       (file . "const char *")))
*/

static void
do_kill_pid_file(struct resource *s)
{
  CAST(pid_file_resource, self, s);
  if (self->super.alive)
    {
      self->super.alive = 0;
      if (unlink(self->file) < 0)
	werror("Unlinking pidfile failed %e\n", errno);
    }
}

static struct resource *
make_pid_file_resource(const char *file)
{
  NEW(pid_file_resource, self);
  init_resource(&self->super, do_kill_pid_file);
  self->file = file;

  return &self->super;
}

/* GABA:
   (class
     (name sighup_close_callback)
     (super lsh_callback)
     (vars
       (resource object resource)))
*/

static void
do_sighup_close_callback(struct lsh_callback *s)
{
  CAST(sighup_close_callback, self, s);
  
  werror("SIGHUP received.\n");
  KILL_RESOURCE(self->resource);
}

static struct lsh_callback *
make_sighup_close_callback(struct resource *resource)
{
  NEW(sighup_close_callback, self);
  self->super.f = do_sighup_close_callback;
  self->resource = resource;

  return &self->super;
}

/* GABA:
   (class
     (name lshd_options)
     (vars
       (port . "char *")
       (hostkey . "char *")

       (daemonic . int)
       (background . int)
       (corefile . int)
       (pid_file . "const char *")
       ; -1 means use pid file iff we're in daemonic mode
       (use_pid_file . int)))
*/

static struct lshd_options *
make_lshd_options(void)
{
  NEW(lshd_options, self);

  /* Default behaviour is to lookup the "ssh" service, and fall back
   * to port 22 if that fails. */
  self->port = NULL;

  /* FIXME: This should use sysconfdir */  
  self->hostkey = "/etc/lsh_host_key";

  self->daemonic = 0;
  self->background = 0;

  /* FIXME: Make the default a configure time option? */
  self->pid_file = "/var/run/lshd.pid";
  self->use_pid_file = -1;
  self->corefile = 0;
  
  return self;
}

/* Option parsing */

const char *argp_program_version
= "lshd (lsh-" VERSION "), secsh protocol version " SERVER_PROTOCOL_VERSION;

const char *argp_program_bug_address = BUG_ADDRESS;

enum {
  OPT_NO = 0x400,
  OPT_INTERFACE = 0x201,

  OPT_DAEMONIC = 0x205,
  OPT_PIDFILE = 0x206,
  OPT_NO_PIDFILE = (OPT_PIDFILE | OPT_NO),
  OPT_CORE = 0x207,
  OPT_BACKGROUND = 0x208,
};

static const struct argp_option
main_options[] =
{
  /* Name, key, arg-name, flags, doc, group */
  { "interface", OPT_INTERFACE, "interface", 0,
    "Listen on this network interface.", 0 }, 
  { "port", 'p', "Port", 0, "Listen on this port.", 0 },
  { "host-key", 'h', "Key file", 0, "Location of the server's private key.", 0},

  { NULL, 0, NULL, 0, "Daemonic behaviour", 0 },
  { "daemonic", OPT_DAEMONIC, NULL, 0, "Run in the background, and redirect stdio to /dev/null, chdir to /, and use syslog.", 0 },
  { "background", OPT_BACKGROUND, NULL, 0, "Run in the background.", 0 },
  { "pid-file", OPT_PIDFILE, "file name", 0, "Create a pid file. When running in daemonic mode, "
    "the default is /var/run/lshd.pid.", 0 },
  { "no-pid-file", OPT_NO_PIDFILE, NULL, 0, "Don't use any pid file. Default in non-daemonic mode.", 0 },
  { "enable-core", OPT_CORE, NULL, 0, "Dump core on fatal errors (disabled by default).", 0 },
  
  { NULL, 0, NULL, 0, NULL, 0 }
};

static const struct argp_child
main_argp_children[] =
{
  { &werror_argp, 0, "", 0 },
  { NULL, 0, NULL, 0}
};

static error_t
main_argp_parser(int key, char *arg, struct argp_state *state)
{
  CAST(lshd_options, self, state->input);

  switch(key)
    {
    default:
      return ARGP_ERR_UNKNOWN;
    case ARGP_KEY_INIT:
      state->child_inputs[0] = NULL;
      break;

    case ARGP_KEY_END:
      if (self->daemonic && self->background)
	argp_error(state, "The options --background and --daemonic are "
		   "mutually exclusive.");
      break;

    case OPT_INTERFACE:
      werror("Ignoring --interface option; using localhost only.\n");
      break;

    case 'p':
      /* FIXME: Interpret multiple -p:s as a request to listen on
       * several ports. */
      self->port = arg;
      break;

    case 'h':
      self->hostkey = arg;
      break;

    case OPT_DAEMONIC:
      self->daemonic = 1;
      break;

    case OPT_BACKGROUND:
      self->background = 1;
      break;

    case OPT_PIDFILE:
      self->pid_file = arg;
      self->use_pid_file = 1;
      break;

    case OPT_NO_PIDFILE:
      self->use_pid_file = 0;
      break;

    case OPT_CORE:
      self->corefile = 1;
      break;      
    }
  return 0;
}
      
static const struct argp
main_argp =
{ main_options, main_argp_parser,
  NULL,
  "Server for the ssh-2 protocol.",
  main_argp_children,
  NULL, NULL
};

int
main(int argc, char **argv)
{
  struct lshd_options *options = make_lshd_options();
  struct configuration *configuration;
  struct resource_list *resources = make_resource_list();;

  argp_parse(&main_argp, argc, argv, 0, NULL, options);
  
  if (!options->corefile && !daemon_disable_core())
    {
      werror("Disabling of core dumps failed.\n");
      return EXIT_FAILURE;
    }

  if (options->daemonic)
    {
      werror("--daemonic not yet implemented.\n");
      return EXIT_FAILURE;
    }

  io_init();
  
  configuration = make_configuration(options->hostkey, global_oop_source);
  
  if (!open_ports(configuration, resources, options->port))
    return EXIT_FAILURE;

  if (options->background)
    {
      /* Just put process into the background. */
      switch (fork())
	{
	case 0:
	  /* Child */
	  trace("forked into background. New pid: %i.\n", getpid());
	  break;
              
	case -1:
	  /* Error */
	  werror("fork failed %e\n", errno);
	  break;

	default:
	  /* Parent */
	  /* FIXME: Ideally, we should wait until the child has
	     created its pid-file. */
	  _exit(EXIT_SUCCESS);
	}
    }
  
  if (options->use_pid_file)
    {
      if (daemon_pidfile(options->pid_file))
	remember_resource(resources, 
			  make_pid_file_resource(options->pid_file));
      else
	{
	  werror("lshd seems to be running already.\n");
	  return EXIT_FAILURE;
	}
    }

  /* FIXME: We need a mechanism to exit when we have closed our ports
     in response to SIGHUP, and all connections have died. */

  io_signal_handler(SIGHUP,
		    make_sighup_close_callback(&resources->super));
  
  /* Ignore status from child processes */
  signal(SIGCHLD, SIG_IGN);

  io_run();

  return EXIT_SUCCESS;
}
