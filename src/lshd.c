/* lshd.c
 *
 * main server program.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "algorithms.h"
#include "alist.h"
#include "atoms.h"
#include "channel.h"
#include "channel_commands.h"
#include "charset.h"
#include "compress.h"
#include "connection_commands.h"
#include "crypto.h"
#include "format.h"
#include "io.h"
#include "io_commands.h"
#include "lookup_verifier.h"
#include "randomness.h"
#include "reaper.h"
#include "server.h"
#include "server_authorization.h"
#include "server_keyexchange.h"
#include "server_pty.h"
#include "server_session.h"
#include "server_userauth.h"
#include "sexp.h"
#include "ssh.h"
#include "tcpforward.h"
#include "tcpforward_commands.h"
#include "tcpforward_commands.h"
#include "userauth.h"
#include "werror.h"
#include "xalloc.h"

#include "lsh_argp.h"

#include "lshd.c.x"

#include <assert.h>

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Block size for stdout and stderr buffers */
#define BLOCK_SIZE 32768


/* Option parsing */

#define OPT_SSH1_FALLBACK 0x200
#define OPT_INTERFACE 0x201
#define OPT_TCPIP_FORWARD 0x202
#define OPT_NO_TCPIP_FORWARD 0x203

/* GABA:
   (class
     (name lshd_options)
     (super algorithms_options)
     (vars
       (style . sexp_argp_state)
       (interface . "char *")
       (port . "char *")
       (hostkey . "char *")
       (local object address_info)
       (with_tcpip_forward . int)
       (sshd1 object ssh1_fallback)))
*/

static struct lshd_options *
make_lshd_options(struct alist *algorithms)
{
  NEW(lshd_options, self);

  init_algorithms_options(&self->super, algorithms);
  
  self->style = SEXP_TRANSPORT;
  self->interface = NULL;
  self->port = "ssh";
  /* FIXME: this should perhaps use sysconfdir */  
  self->hostkey = "/etc/lsh_host_key";
  self->local = NULL;

  self->with_tcpip_forward = 1;
  
  self->sshd1 = NULL;
  
  return self;
}

static const struct argp_option
main_options[] =
{
  /* Name, key, arg-name, flags, doc, group */
  { "interface", OPT_INTERFACE, "interface", 0,
    "Listen on this network interface", 0 }, 
  { "port", 'p', "Port", 0, "Listen on this port.", 0 },
  { "host-key", 'h', "Key file", 0, "Location of the server's private key.", 0},

#if WITH_SSH1_FALLBACK
  { "ssh1-fallback", OPT_SSH1_FALLBACK, "File name", OPTION_ARG_OPTIONAL,
    "Location of the sshd1 program, for falling back to version 1 of the Secure Shell protocol.", 0 },
#endif /* WITH_SSH1_FALLBACK */

#if WITH_TCP_FORWARD
  { "tcp-forward", OPT_TCPIP_FORWARD, NULL, 0, "Enable tcpip forwarding (default).", 0 },
  { "no-tcp-forward", OPT_NO_TCPIP_FORWARD, NULL, 0, "Disable tcpip forwarding.", 0 },
#endif /* WITH_TCP_FORWARD */

  { NULL, 0, NULL, 0, NULL, 0 }
};

static const struct argp_child
main_argp_children[] =
{
  { &sexp_input_argp, 0, "", 0 },
  { &algorithms_argp, 0, "", 0 },
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
      state->child_inputs[0] = &self->style;
      state->child_inputs[1] = &self->super;
      state->child_inputs[2] = NULL;
      break;
    case ARGP_KEY_ARG:
      argp_error(state, "Spurious arguments.");
      break;
      
    case ARGP_KEY_END:
      self->local = make_address_info_c(self->interface, self->port);
      if (!self->local)
	argp_error(state, "Invalid interface, port or service, %s:%s'.",
		   self->interface ? self->interface : "ANY",
		   self->port);
      break;
      
    case 'p':
      self->port = arg;
      break;

    case 'h':
      self->hostkey = arg;
      break;

    case OPT_INTERFACE:
      self->interface = arg;
      break;
 
#if WITH_SSH1_FALLBACK
    case OPT_SSH1_FALLBACK:
      self->sshd1 = make_ssh1_fallback(arg ? arg : SSHD1);
      break;
#endif

    case OPT_TCPIP_FORWARD:
      self->with_tcpip_forward = 1;
      break;

    case OPT_NO_TCPIP_FORWARD:
      self->with_tcpip_forward = 0;
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

/* FIXME: We should have some more general functions for reading
 * private keys. */

/* GABA:
   (class
     (name read_key)
     (super command_continuation)
     (vars
       (random object randomness)
       ;; Maps hostkey algorithm to a keyinfo structure
       (keys object alist)))
*/

static void do_read_key(struct command_continuation *s,
			struct lsh_object *a)
{
  CAST(read_key, closure, s);
  CAST_SUBTYPE(sexp, private, a);
  
  struct sexp_iterator *i;
  struct sexp *e;
  mpz_t p, q, g, y, x;
  
  if (!sexp_check_type(private, "private-key", &i))
    {
      werror("lshd: Host key file does not contain a private key.");
      return;
    }

  e = SEXP_GET(i);
  if (! (e && sexp_check_type(e, "dsa", &i)))
    {
      werror("lshd: Unknown key type (only dsa is supported)\n");
      return;
    }

  mpz_init(p);
  mpz_init(q);
  mpz_init(g);
  mpz_init(y);
  mpz_init(x);

  if (sexp_get_un(i, "p", p)
      && sexp_get_un(i, "q", q)
      && sexp_get_un(i, "g", g)
      && sexp_get_un(i, "y", y)
      && sexp_get_un(i, "x", x)
      && !SEXP_GET(i))
    {
      /* Test key */
      mpz_t tmp;
      struct lsh_string *s;
      
      mpz_init_set(tmp, g);
      mpz_powm(tmp, tmp, x, p);
      if (mpz_cmp(tmp, y))
	{
	  werror("lshd: Host key doesn't work.\n");
	  mpz_clear(tmp);
	}
      else
	{
	  struct lsh_string *public
	    = ssh_format("%a%n%n%n%n", ATOM_SSH_DSS, p, q, g, y);
	  struct signer *private;
	  	  
	  s = ssh_format("%n", x);
	  
	  private = MAKE_SIGNER(make_dsa_algorithm(closure->random),
				public->length, public->data,
				s->length, s->data);
	  assert(private);
	  lsh_string_free(s);
	  
	  /* FIXME: Check if we already have a key for this algorithm,
	   * and warn about multiple keys. */
	  ALIST_SET(closure->keys, ATOM_SSH_DSS,
		    make_keypair(public, private));

#if DATAFELLOWS_WORKAROUNDS
	  ALIST_SET(closure->keys, ATOM_SSH_DSS_KLUDGE,
		    make_keypair(public,
				      make_dsa_signer_kludge(private)));
#endif /* DATAFELLOWS_WORKAROUNDS */
	  
	  debug("lshd: Using (public) hostkey:\n"
		"  p=%xn\n"
		"  q=%xn\n"
		"  g=%xn\n"
		"  y=%xn\n",
		p, q, g, y);
	}
    }

  /* Cleanup */
  mpz_clear(p);
  mpz_clear(q);
  mpz_clear(g);
  mpz_clear(y);
  mpz_clear(x);
}

static int read_host_key(const char *name,
			 struct alist *keys,
			 struct randomness *r)
{
  int fd = open(name, O_RDONLY);
  if (fd < 0)
    {
      werror("lshd: Could not open %z (errno = %i): %z\n",
	     name, errno, STRERROR(errno));
      return 0;
    }
  else
    {
      int res;
      
      NEW(read_key, handler);
      handler->super.c = do_read_key;

      handler->random = r;
      handler->keys = keys;
      
      res = blocking_read(fd,
			  make_read_sexp(SEXP_TRANSPORT, 1,
					 &handler->super, &ignore_exception_handler));
      close(fd);

      KILL(handler);
      
      return 1;
    }
}

/* GABA:
   (expr
     (name lshd_listen)
     (globals
       (log "&io_log_peer_command.super.super"))
     (params
       (listen object command)
       (handshake object command)
       (services object command) )
     (expr (lambda (port)
             (services (handshake (log (listen port)))))))
*/

/* Invoked when the client requests the userauth service. */
/* GABA:
   (expr
     (name lshd_services)
     (params 
       (userauth object command))
     (expr
       (lambda (connection)
         ((userauth connection) connection))))
*/

/* Invoked when starting the ssh-connection service */
/* GABA:
   (expr
     (name lshd_connection_service)
     (globals
       (progn "&progn_command.super.super")
       (init "&connection_service.super"))
     (params
       (login object command)     
       (hooks object object_list))
     (expr
       (lambda (user connection)
         ((progn hooks) (login user
	                       ; We have to initialize the connection
			       ; before logging in.
	                       (init connection))))))
*/

int main(int argc, char **argv)
{
  struct lshd_options *options;
  
  struct alist *keys;
  
  struct reap *reaper;
  
  struct randomness *r;
  struct diffie_hellman_method *dh;
  struct keyexchange_algorithm *kex;
  struct alist *algorithms;
  struct make_kexinit *make_kexinit;
  struct alist *authorization_lookup;
  
  /* FIXME: Why not allocate backend statically? */
  NEW(io_backend, backend);
  init_backend(backend);

  /* For filtering messages. Could perhaps also be used when converting
   * strings to and from UTF8. */
  setlocale(LC_CTYPE, "");
  /* FIXME: Choose character set depending on the locale */
  set_local_charset(CHARSET_LATIN1);

  r = make_reasonably_random();
  dh = make_dh1(r);
  
  algorithms = many_algorithms(1,
			       ATOM_SSH_DSS, make_dsa_algorithm(r),
			       -1);
  options = make_lshd_options(algorithms);
  
  argp_parse(&main_argp, argc, argv, 0, NULL, options);

  /* Read the hostkey */
  keys = make_alist(0, -1);
  if (!read_host_key(options->hostkey, keys, r))
    {
      werror("lshd: Could not read hostkey.\n");
      return EXIT_FAILURE;
    }
  /* FIXME: We should check that we have at aleast one host key.
   * We should also extract the host-key algorithms for which we have keys,
   * instead of hardcoding ssh-dss below. */
  
#if 0
#if HAVE_SYSLOG
  {
    int option = LOG_PID | LOG_CONS;
    if (foreground_flag)
      {
	option |= LOG_PERROR;
      }
    openlog("lshd", option, LOG_DAEMON);
    syslog_flag = 1;
  }
#endif /* HAVE_SYSLOG */
#endif
 
  reaper = make_reaper();

  kex = make_dh_server(dh, keys);

  authorization_lookup
    = make_alist(1
#if DATAFELLOWS_WORKAROUNDS
		 +1,
		 ATOM_SSH_DSS_KLUDGE, make_authorization_db(ssh_format("keys_md5"), 
							    make_dsa_kludge_algorithm(NULL),
							    &md5_algorithm)
#endif
				    
		 ,ATOM_SSH_DSS, make_authorization_db(ssh_format("keys_md5"),
						      make_dsa_algorithm(NULL),
						      &md5_algorithm),
		 
		 -1);

  
  ALIST_SET(algorithms, ATOM_DIFFIE_HELLMAN_GROUP1_SHA1, kex);

  make_kexinit
    = make_simple_kexinit(r,
			  make_int_list(1, ATOM_DIFFIE_HELLMAN_GROUP1_SHA1,
					-1),
			  make_int_list(1, ATOM_SSH_DSS, -1),
			  options->super.crypto_algorithms,
			  options->super.mac_algorithms,
			  options->super.compression_algorithms,
			  make_int_list(0, -1));
  
  {
    /* Commands to be invoked on the connection */
    struct object_list *connection_hooks;

#if WITH_TCP_FORWARD
    if (options->with_tcpip_forward)
      connection_hooks = make_object_list
	(3,
	 make_tcpip_forward_hook(backend),
	 make_install_fix_global_request_handler
	 (ATOM_CANCEL_TCPIP_FORWARD, &tcpip_cancel_forward),
	 make_direct_tcpip_hook(backend),
	 -1);
    else
#endif
      connection_hooks = make_object_list(0, -1);
    {
      struct lsh_object *o = lshd_listen
	(make_simple_listen(backend, NULL),
	 make_handshake_command(CONNECTION_SERVER,
				"lsh - a free ssh",
				SSH_MAX_PACKET,
				r,
				algorithms,
				make_kexinit,
				options->sshd1),
	 make_offer_service
	 (make_alist
	  (1, ATOM_SSH_USERAUTH,
	   lshd_services(make_userauth_service
			 (make_int_list(2,
					ATOM_PASSWORD,
					ATOM_PUBLICKEY, -1),
			  make_alist(2,
				     ATOM_PASSWORD, &unix_userauth.super,
				     ATOM_PUBLICKEY, make_userauth_publickey(authorization_lookup),
				     -1),
			  make_alist(1, ATOM_SSH_CONNECTION,
				     lshd_connection_service
				     (make_server_connection_service
				      (make_alist
				       (1
#if WITH_PTY_SUPPORT
					+1, ATOM_PTY_REQ, make_pty_handler()
#endif /* WITH_PTY_SUPPORT */
					, ATOM_SHELL,
					make_shell_handler(backend,
							   reaper),
					-1),
				       backend),
				      connection_hooks),
				     -1))),
	   -1)));
    
      CAST_SUBTYPE(command, server_listen, o);
    
      COMMAND_CALL(server_listen, options->local,
		   &discard_continuation,
		   make_report_exception_handler(EXC_IO, EXC_IO, "lshd: ",
						 &default_exception_handler,
						 HANDLER_CONTEXT));
    }
  }
  
  reaper_run(reaper, backend);

  return 0;
}
