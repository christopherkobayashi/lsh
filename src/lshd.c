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
#include "daemon.h"
#include "dsa.h"
#include "format.h"
#include "handshake.h"
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
#include "sexp.h"
#include "spki.h"
#include "srp.h"
#include "ssh.h"
#include "tcpforward.h"
#include "tcpforward_commands.h"
#include "tcpforward_commands.h"
#include "server_userauth.h"
#include "version.h"
#include "werror.h"
#include "xalloc.h"

#include "lsh_argp.h"

/* Forward declarations */
struct command options2local;
#define OPTIONS2LOCAL (&options2local.super)

struct command options2keys;
#define OPTIONS2KEYS (&options2keys.super)

struct command options2tcp_wrapper;
#define OPTIONS2TCP_WRAPPER (&options2tcp_wrapper.super)

struct command_2 close_on_sighup;
#define CLOSE_ON_SIGHUP (&close_on_sighup.super.super)

#include "lshd.c.x"

#include <assert.h>

#include <errno.h>
#include <locale.h>
#include <stdio.h>
/* #include <string.h> */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif


/* Option parsing */

const char *argp_program_version
= "lshd-" VERSION ", secsh protocol version " SERVER_PROTOCOL_VERSION;

const char *argp_program_bug_address = BUG_ADDRESS;

/* The definition of SBINDIR is currently broken */
#if 0
# define KERBEROS_HELPER SBINDIR "/lsh-krb-checkpw"
#else
# define KERBEROS_HELPER PREFIX "/sbin/lsh-krb-checkpw"
#endif

#define OPT_NO 0x400
#define OPT_SSH1_FALLBACK 0x200
#define OPT_INTERFACE 0x201

#define OPT_TCPIP_FORWARD 0x202
#define OPT_NO_TCPIP_FORWARD (OPT_TCPIP_FORWARD | OPT_NO)
#define OPT_PTY 0x203
#define OPT_NO_PTY (OPT_PTY | OPT_NO)
#define OPT_SUBSYSTEMS 0x204
#define OPT_NO_SUBSYSTEMS (OPT_SUBSYSTEMS | OPT_NO)

#define OPT_DAEMONIC 0x205
#define OPT_NO_DAEMONIC (OPT_DAEMONIC | OPT_NO)
#define OPT_PIDFILE 0x206
#define OPT_NO_PIDFILE (OPT_PIDFILE | OPT_NO)
#define OPT_CORE 0x207
#define OPT_SYSLOG 0x208
#define OPT_NO_SYSLOG (OPT_SYSLOG | OPT_NO)
#define OPT_X11_FORWARD 0x209
#define OPT_NO_X11_FORWARD (OPT_X11_FORWARD |OPT_NO)

#define OPT_SRP 0x210
#define OPT_NO_SRP (OPT_SRP | OPT_NO)
#define OPT_DH 0x211
#define OPT_NO_DH (OPT_DH | OPT_NO)

#define OPT_PUBLICKEY 0x220
#define OPT_NO_PUBLICKEY (OPT_PUBLICKEY | OPT_NO)
#define OPT_PASSWORD 0x221
#define OPT_NO_PASSWORD (OPT_PASSWORD | OPT_NO)

#define OPT_ROOT_LOGIN 0x222
#define OPT_NO_ROOT_LOGIN (OPT_ROOT_LOGIN | OPT_NO)

#define OPT_KERBEROS_PASSWD 0x223
#define OPT_NO_KERBEROS_PASSWD (OPT_KERBEROS_PASSWD | OPT_NO)

#define OPT_PASSWORD_HELPER 0x224

#define OPT_LOGIN_SHELL 0x225

#define OPT_TCPWRAPPERS 0x226
#define OPT_NO_TCPWRAPPERS 0x227

#define OPT_TCPWRAP_GOAWAY_MSG 0x228

/* GABA:
   (class
     (name lshd_options)
     (super algorithms_options)
     (vars
       (e object exception_handler)
       
       (reaper object reaper)
       (random object randomness)
       
       (signature_algorithms object alist)
       (style . sexp_argp_state)
       (interface . "char *")
       (port . "char *")
       (hostkey . "char *")
       (local object address_info)
       (tcp_wrapper_name . "char *")
       (tcp_wrapper_message . "char *")

       (with_srp_keyexchange . int)
       (with_dh_keyexchange . int)

       ;; (kexinit object make_kexinit)
       (kex_algorithms object int_list)
       
       (with_publickey . int)
       (with_password . int)
       (allow_root . int)
       (pw_helper . "const char *")
       (login_shell . "const char *")
       
       (with_tcpip_forward . int)
       (with_x11_forward . int)
       (with_pty . int)
       (subsystems . "const char **")
       
       (userauth_methods object int_list)
       (userauth_algorithms object alist)
       
       (sshd1 object ssh1_fallback)
       (daemonic . int)
       (no_syslog . int)
       (corefile . int)
       (pid_file . "const char *")
       ; -1 means use pid file iff we're in daemonic mode
       (use_pid_file . int)
       ; Resources that should be killed when SIGHUP is received,
       ; or when the program exits.
       (resources object resource_list)))
*/

static void
do_exc_lshd_handler(struct exception_handler *s,
		    const struct exception *e)
{
  switch(e->type)
    {
    case EXC_RESOLVE:
    case EXC_SEXP_SYNTAX:
    case EXC_SPKI_TYPE:
    case EXC_RANDOMNESS_LOW_ENTROPY:
      werror("lshd: %z\n", e->msg);
      exit(EXIT_FAILURE);
    default:
      EXCEPTION_RAISE(s->parent, e);
    }
}

static struct exception_handler *
make_lshd_exception_handler(struct exception_handler *parent,
			    const char *context)
{
  return make_exception_handler(do_exc_lshd_handler, parent, context);
}

static struct lshd_options *
make_lshd_options(void)
{
  NEW(lshd_options, self);

  init_algorithms_options(&self->super, all_symmetric_algorithms());

  self->e = make_lshd_exception_handler(&default_exception_handler,
					HANDLER_CONTEXT);
  self->reaper = make_reaper();
  self->random = make_system_random();

  self->signature_algorithms = all_signature_algorithms(self->random); /* OK to initialize with NULL */

  self->style = SEXP_TRANSPORT;
  self->interface = NULL;

  /* Default behaviour is to lookup the "ssh" service, and fall back
   * to port 22 if that fails. */
  self->port = NULL;
  
  /* FIXME: this should perhaps use sysconfdir */  
  self->hostkey = "/etc/lsh_host_key";
  self->local = NULL;

  self->with_dh_keyexchange = 1;
  self->with_srp_keyexchange = 0;

  self->kex_algorithms = NULL;
  
  self->with_publickey = 1;
  self->with_password = 1;
  self->with_tcpip_forward = 1;
  /* Experimental, so disabled by default. */
  self->with_x11_forward = 0;
  self->with_pty = 1;
  self->subsystems = NULL;
  
  self->tcp_wrapper_name = "lshd";
  self->tcp_wrapper_message = NULL;

  self->allow_root = 0;
  self->pw_helper = NULL;
  self->login_shell = NULL;
  
  self->userauth_methods = NULL;
  self->userauth_algorithms = NULL;
  
  self->sshd1 = NULL;
  self->daemonic = 0;
  self->no_syslog = 0;
  
  /* FIXME: Make the default a configure time option? */
  self->pid_file = "/var/run/lshd.pid";
  self->use_pid_file = -1;
  self->corefile = 0;

  self->resources = make_resource_list();
  /* Not strictly needed for gc, but makes sure the
   * resource list is killed properly by gc_final. */
  gc_global(&self->resources->super);

  return self;
}

/* Port to listen on */
DEFINE_COMMAND(options2local)
     (struct command *s UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST(lshd_options, options, a);
  /* FIXME: Call bind already here? */
  COMMAND_RETURN(c, options->local);
}

/* alist of signature algorithms */
DEFINE_COMMAND(options2signature_algorithms)
     (struct command *s UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST(lshd_options, options, a);
  COMMAND_RETURN(c, options->signature_algorithms);
}


/* FIXME: Call read_host_key directly from main instead. */
DEFINE_COMMAND(options2keys)
     (struct command *ignored UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST(lshd_options, options, a);

  struct alist *keys = make_alist(0, -1);
  read_host_key(options->hostkey, options->signature_algorithms, keys);
  COMMAND_RETURN(c, keys);
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
       (resources object resource_list)))
*/

static void
do_sighup_close_callback(struct lsh_callback *s)
{
  CAST(sighup_close_callback, self, s);
  unsigned nfiles;
  
  werror("SIGHUP received.\n");
  KILL_RESOURCE_LIST(self->resources);
  
  nfiles = io_nfiles();

  if (nfiles)
    werror("Waiting for active connections to terminate, "
	   "%i files still open.\n", nfiles);
}

static struct lsh_callback *
make_sighup_close_callback(struct lshd_options *options)
{
  NEW(sighup_close_callback, self);
  self->super.f = do_sighup_close_callback;
  self->resources = options->resources;

  return &self->super;
}

/* (close_on_sighup options file) */
DEFINE_COMMAND2(close_on_sighup)
     (struct command_2 *ignored UNUSED,
      struct lsh_object *a1,
      struct lsh_object *a2,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
  CAST(lshd_options, options, a1);
  CAST(lsh_fd, fd, a2);

  remember_resource(options->resources, &fd->super);

  COMMAND_RETURN(c, a2);
}


DEFINE_COMMAND(options2tcp_wrapper)
     (struct command *s UNUSED,
      struct lsh_object *a,
      struct command_continuation *c,
      struct exception_handler *e UNUSED)
{
#if WITH_TCPWRAPPERS
  CAST(lshd_options, options, a);

  if (options->tcp_wrapper_name) 
    COMMAND_RETURN(c, 
		   make_tcp_wrapper(
				    make_string(options->tcp_wrapper_name),
				    options->tcp_wrapper_message ? 
				    ssh_format("%lz\n", options->tcp_wrapper_message ) :
				    ssh_format("")
				    )
		   ); 
  else
#endif /* WITH_TCPWRAPPERS */
    COMMAND_RETURN(c, &io_log_peer_command);
}


static const struct argp_option
main_options[] =
{
  /* Name, key, arg-name, flags, doc, group */
  { "interface", OPT_INTERFACE, "interface", 0,
    "Listen on this network interface.", 0 }, 
  { "port", 'p', "Port", 0, "Listen on this port.", 0 },
  { "host-key", 'h', "Key file", 0, "Location of the server's private key.", 0},
#if WITH_SSH1_FALLBACK
  { "ssh1-fallback", OPT_SSH1_FALLBACK, "File name", OPTION_ARG_OPTIONAL,
    "Location of the sshd1 program, for falling back to version 1 of the Secure Shell protocol.", 0 },
#endif /* WITH_SSH1_FALLBACK */

#if WITH_TCPWRAPPERS
  { NULL, 0, NULL, 0, "Connection filtering:", 0 },
  { "tcpwrappers", OPT_TCPWRAPPERS, "name", 0, "Set service name for tcp wrappers (default lshd)", 0 },
  { "no-tcpwrappers", OPT_NO_TCPWRAPPERS, NULL, 0, "Disable wrappers", 0 },
  { "tcpwrappers-msg", OPT_TCPWRAP_GOAWAY_MSG, "'Message'", 0, "Message sent to clients " 
    "who aren't allowed to connect. A newline will be added.", 0 },
#endif /* WITH_TCPWRAPPERS */

  { NULL, 0, NULL, 0, "Keyexchange options:", 0 },
#if WITH_SRP
  { "srp-keyexchange", OPT_SRP, NULL, 0, "Enable experimental SRP support.", 0 },
  { "no-srp-keyexchange", OPT_NO_SRP, NULL, 0, "Disable experimental SRP support (default).", 0 },
#endif /* WITH_SRP */

  { "dh-keyexchange", OPT_DH, NULL, 0, "Enable DH support (default).", 0 },
  { "no-dh-keyexchange", OPT_NO_DH, NULL, 0, "Disable DH support.", 0 },
  
  { NULL, 0, NULL, 0, "User authentication options:", 0 },

  { "password", OPT_PASSWORD, NULL, 0,
    "Enable password user authentication (default).", 0},
  { "no-password", OPT_NO_PASSWORD, NULL, 0,
    "Disable password user authentication.", 0},

  { "publickey", OPT_PUBLICKEY, NULL, 0,
    "Enable publickey user authentication (default).", 0},
  { "no-publickey", OPT_NO_PUBLICKEY, NULL, 0,
    "Disable publickey user authentication.", 0},

  { "root-login", OPT_ROOT_LOGIN, NULL, 0,
    "Allow root to login.", 0 },
  { "no-root-login", OPT_NO_ROOT_LOGIN, NULL, 0,
    "Don't allow root to login (default).", 0 },

  { "login-shell", OPT_LOGIN_SHELL, "Program", 0,
    "Use this program as the login shell for all users. "
    "(Experimental)", 0 },
  
  { "kerberos-passwords", OPT_KERBEROS_PASSWD, NULL, 0,
    "Recognize kerberos passwords, using the helper program "
    "\"" KERBEROS_HELPER "\". This option is experimental.", 0 },
  { "no-kerberos-passwords", OPT_NO_KERBEROS_PASSWD, NULL, 0,
    "Don't recognize kerberos passwords (default behaviour).", 0 },

  { "password-helper", OPT_PASSWORD_HELPER, "Program", 0,
    "Use the named helper program for password verification. "
    "(Experimental).", 0 },

  { NULL, 0, NULL, 0, "Offered services:", 0 },

#if WITH_PTY_SUPPORT
  { "pty-support", OPT_PTY, NULL, 0, "Enable pty allocation (default).", 0 },
  { "no-pty-support", OPT_NO_PTY, NULL, 0, "Disable pty allocation.", 0 },
#endif /* WITH_PTY_SUPPORT */
#if WITH_TCP_FORWARD
  { "tcpip-forward", OPT_TCPIP_FORWARD, NULL, 0,
    "Enable tcpip forwarding (default).", 0 },
  { "no-tcpip-forward", OPT_NO_TCPIP_FORWARD, NULL, 0,
    "Disable tcpip forwarding.", 0 },
#endif /* WITH_TCP_FORWARD */
#if WITH_X11_FORWARD
  { "x11-forward", OPT_X11_FORWARD, NULL, 0,
    "Enable x11 forwarding.", 0 },
  { "no-x11-forward", OPT_NO_X11_FORWARD, NULL, 0,
    "Disable x11 forwarding (default).", 0 },
#endif /* WITH_X11_FORWARD */
  
  { "subsystems", OPT_SUBSYSTEMS, "List of subsystem names and programs", 0,
    "For example `sftp=/usr/sbin/sftp-server,foosystem=/usr/bin/foo' "
    "(experimental).", 0},
  
  { NULL, 0, NULL, 0, "Daemonic behaviour", 0 },
  { "daemonic", OPT_DAEMONIC, NULL, 0, "Run in the background, redirect stdio to /dev/null, and chdir to /.", 0 },
  { "no-daemonic", OPT_NO_DAEMONIC, NULL, 0, "Run in the foreground, with messages to stderr (default).", 0 },
  { "pid-file", OPT_PIDFILE, "file name", 0, "Create a pid file. When running in daemonic mode, "
    "the default is /var/run/lshd.pid.", 0 },
  { "no-pid-file", OPT_NO_PIDFILE, NULL, 0, "Don't use any pid file. Default in non-daemonic mode.", 0 },
  { "enable-core", OPT_CORE, NULL, 0, "Dump core on fatal errors (disabled by default).", 0 },
  { "no-syslog", OPT_NO_SYSLOG, NULL, 0, "Don't use syslog (by default, syslog is used "
    "when running in daemonic mode).", 0 },
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

/* NOTE: Modifies the argument string. */
static const char **
parse_subsystem_list(char *arg)
{
  const char **subsystems;
  char *separator;
  unsigned length;
  unsigned i;
  
  /* First count the number of elements. */
  for (length = 1, i = 0; arg[i]; i++)
    if (arg[i] == ',')
      length++;

  subsystems = lsh_space_alloc((length * 2 + 1) * sizeof(*subsystems));

  for (i = 0; ; i++)
    {
      subsystems[2*i] = arg;

      separator = strchr(arg, '=');

      if (!separator)
	goto fail;

      *separator = '\0';

      subsystems[2*i+1] = arg = separator + 1;
      
      separator = strchr(arg, ',');

      if (i == (length - 1))
	break;
      
      if (!separator)
	goto fail;

      *separator = '\0';
      arg = separator + 1;
    }
  if (separator)
    {
    fail:
      lsh_space_free(subsystems);
      return NULL;
    }
  return subsystems;
}

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
    case ARGP_KEY_END:
      {
	struct user_db *user_db = NULL;
	
	if (!self->random)
	  argp_failure( state, EXIT_FAILURE, 0,  "No randomness generator available.");
	
       	if (self->with_password || self->with_publickey || self->with_srp_keyexchange)
	  user_db = make_unix_user_db(self->reaper,
				      self->pw_helper, self->login_shell,
				      self->allow_root);
	  
	if (self->with_dh_keyexchange || self->with_srp_keyexchange)
	  {
	    int i = 0;
	    self->kex_algorithms 
	      = alloc_int_list(self->with_dh_keyexchange + self->with_srp_keyexchange);
	    
	    if (self->with_dh_keyexchange)
	      {
		LIST(self->kex_algorithms)[i++] = ATOM_DIFFIE_HELLMAN_GROUP1_SHA1;
		ALIST_SET(self->super.algorithms,
			  ATOM_DIFFIE_HELLMAN_GROUP1_SHA1,
			  &make_dh_server(make_dh1(self->random))
			  ->super);
	      }
#if WITH_SRP	    
	    if (self->with_srp_keyexchange)
	      {
		assert(user_db);
		LIST(self->kex_algorithms)[i++] = ATOM_SRP_RING1_SHA1_LOCAL;
		ALIST_SET(self->super.algorithms,
			  ATOM_SRP_RING1_SHA1_LOCAL,
			  &make_srp_server(make_srp1(self->random),
					   user_db)
			  ->super);
	      }
#endif /* WITH_SRP */
	  }
	else
	  argp_error(state, "All keyexchange algorithms disabled.");

	if (self->port)
	  self->local = make_address_info_c(self->interface, self->port, 0);
	else
	  self->local = make_address_info_c(self->interface, "ssh", 22);
      
	if (!self->local)
	  argp_error(state, "Invalid interface, port or service, %s:%s'.",
		     self->interface ? self->interface : "ANY",
		     self->port);

	if (self->use_pid_file < 0)
	  self->use_pid_file = self->daemonic;

	if (self->with_password || self->with_publickey)
	  {
	    int i = 0;
	    
	    self->userauth_methods
	      = alloc_int_list(self->with_password + self->with_publickey);
	    self->userauth_algorithms = make_alist(0, -1);
	    
	    if (self->with_password)
	      {
		LIST(self->userauth_methods)[i++] = ATOM_PASSWORD;
		ALIST_SET(self->userauth_algorithms,
			  ATOM_PASSWORD,
			  &make_userauth_password(user_db)->super);
	      }
	    if (self->with_publickey)
	      {
		/* FIXME: Doesn't use spki */
		struct lookup_verifier *key_db
		  = make_authorization_db(ssh_format("authorized_keys_sha1"),
					  &crypto_sha1_algorithm);
		
		LIST(self->userauth_methods)[i++] = ATOM_PUBLICKEY;
		ALIST_SET(self->userauth_algorithms,
			  ATOM_PUBLICKEY,
			  &make_userauth_publickey
			  (user_db,
			   make_alist(2,
				      ATOM_SSH_DSS, key_db,
				      ATOM_SSH_RSA, key_db,
				      -1))
			  ->super);
	      }
	  }
        if (self->with_srp_keyexchange)
          ALIST_SET(self->userauth_algorithms,
                    ATOM_NONE,
                    &server_userauth_none.super);

        if (!self->userauth_algorithms->size)
	  argp_error(state, "All user authentication methods disabled.");

	break;
      }
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

    case OPT_SRP:
      self->with_srp_keyexchange = 1;
      break;

    case OPT_NO_SRP:
      self->with_srp_keyexchange = 0;
      break;
      
    case OPT_DH:
      self->with_dh_keyexchange = 1;
      break;

    case OPT_NO_DH:
      self->with_dh_keyexchange = 0;
      break;
      
    case OPT_PASSWORD:
      self->with_password = 1;
      break;
      
    case OPT_NO_PASSWORD:
      self->with_password = 0;
      break;

    case OPT_PUBLICKEY:
      self->with_publickey = 1;
      break;
      
    case OPT_NO_PUBLICKEY:
      self->with_publickey = 0;
      break;

    case OPT_ROOT_LOGIN:
      self->allow_root = 1;
      break;

    case OPT_KERBEROS_PASSWD:
      self->pw_helper = KERBEROS_HELPER;
      break;

    case OPT_NO_KERBEROS_PASSWD:
      self->pw_helper = NULL;
      break;

    case OPT_PASSWORD_HELPER:
      self->pw_helper = arg;
      break;

    case OPT_LOGIN_SHELL:
      self->login_shell = arg;
      break;
      
#if WITH_TCP_FORWARD
    case OPT_TCPIP_FORWARD:
      self->with_tcpip_forward = 1;
      break;

    case OPT_NO_TCPIP_FORWARD:
      self->with_tcpip_forward = 0;
      break;
#endif /* WITH_TCP_FORWARD */
#if WITH_X11_FORWARD
    case OPT_X11_FORWARD:
      self->with_x11_forward = 1;
      break;
    case OPT_NO_X11_FORWARD:
      self->with_x11_forward = 0;
      break;
#endif /* WITH_X11_FORWARD */
      
#if WITH_PTY_SUPPORT
    case OPT_PTY:
      self->with_pty = 1;
      break;
    case OPT_NO_PTY:
      self->with_pty = 0;
      break;
#endif /* WITH_PTY_SUPPORT */

#if WITH_TCPWRAPPERS
    case OPT_TCPWRAPPERS:
      self->tcp_wrapper_name = arg; /* Name given */
      break;
    case OPT_NO_TCPWRAPPERS:
      self->tcp_wrapper_name = NULL; /* Disable by giving name NULL */
      break;
      
    case OPT_TCPWRAP_GOAWAY_MSG:
      self->tcp_wrapper_message = arg;
      break;

#endif /* WITH_TCPWRAPPERS */

    case OPT_SUBSYSTEMS:
      self->subsystems = parse_subsystem_list(arg);
      if (!self->subsystems)
	argp_error(state, "Invalid subsystem list.");
      break;

    case OPT_NO_SUBSYSTEMS:
      self->subsystems = NULL;
      break;
      
    case OPT_DAEMONIC:
      self->daemonic = 1;
      break;
      
    case OPT_NO_DAEMONIC:
      self->daemonic = 0;
      break;

    case OPT_NO_SYSLOG:
      self->no_syslog = 1;
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


/* GABA:
   (expr
     (name make_lshd_listen)
     (params
       (handshake object handshake_info)
       (init object make_kexinit)
       (services object command) )
     (expr (lambda (options)
             (let ((keys (options2keys options)))
	       (close_on_sighup options
	         (listen
	           (lambda (lv)
    	             (services (connection_handshake
    	           		  handshake
    	           		  (kexinit_filter init keys)
    	           		  keys 
				  (options2tcp_wrapper options lv))))
	           (bind (options2local options)) ))))))
*/


/* Invoked when starting the ssh-connection service */
/* GABA:
   (expr
     (name make_lshd_connection_service)
     (params
       (hooks object object_list))
     (expr
       (lambda (connection)
         ((progn hooks)
	    ; We have to initialize the connection
	    ; before adding handlers.
	    (init_connection_service
	      ; Disconnect if connection->user is NULL
	      (connection_require_userauth connection)))))))
*/

static void
do_terminate_callback(struct lsh_callback *s UNUSED)
{
  io_final();

  /* If we're using GCOV, just call exit(). That way, profiling info
   * is written properly when the process is terminated. */
#if !WITH_GCOV
  kill(getpid(), SIGKILL);
#endif
  exit(0);
}

static struct lsh_callback
sigterm_handler = { STATIC_HEADER, do_terminate_callback };

static void
install_signal_handlers(struct lshd_options *options)
{
  io_signal_handler(SIGTERM, &sigterm_handler);
  io_signal_handler(SIGHUP,
		    make_sighup_close_callback(options));
}

int
main(int argc, char **argv)
{
  struct lshd_options *options;

  io_init();
  
  /* For filtering messages. Could perhaps also be used when converting
   * strings to and from UTF8. */
  setlocale(LC_CTYPE, "");

  /* FIXME: Choose character set depending on the locale */
  set_local_charset(CHARSET_LATIN1);

  options = make_lshd_options();

  if (!options)
    return EXIT_FAILURE;

  install_signal_handlers(options);
  
  trace("Parsing options...\n");
  argp_parse(&main_argp, argc, argv, 0, NULL, options);
  trace("Parsing options... done\n");  

  if (!options->corefile && !daemon_disable_core())
    {
      werror("Disabling of core dumps failed.\n");
      return EXIT_FAILURE;
    }

  if (!options->random) 
    {
      werror("Failed to initialize randomness generator.\n");
      return EXIT_FAILURE;
    }
  
  if (options->daemonic)
    {
      if (options->no_syslog)
        {
          /* Just put process into the background. --no-syslog is an
           * inappropriate name */
          switch (fork())
            {
            case 0:
              /* Child */
              /* FIXME: Should we create a new process group, close our tty
               * and stdio, etc? */
              trace("forked into background. New pid: %i.\n", getpid());
              break;
              
            case -1:
              /* Error */
              werror("background_process: fork failed %e\n", errno);
              break;
              
            default:
              /* Parent */
              _exit(EXIT_SUCCESS);
            }
        }
      else
        {
#if HAVE_SYSLOG
          set_error_syslog("lshd");
#else /* !HAVE_SYSLOG */
          werror("lshd: No syslog. Further messages will be directed to /dev/null.\n");
#endif /* !HAVE_SYSLOG */

          switch (daemon_init())
            {
            case 0:
              werror("lshd: Spawning into background failed.\n");
              return EXIT_FAILURE;
            case DAEMON_INETD:
              werror("lshd: spawning from inetd not yet supported.\n");
              return EXIT_FAILURE;
            case DAEMON_INIT:
            case DAEMON_NORMAL:
              break;
            default:
              fatal("Internal error\n");
            }
        }
    }
  
  if (options->use_pid_file)
    {
      if (daemon_pidfile(options->pid_file))
	remember_resource(options->resources, 
			  make_pid_file_resource(options->pid_file));
      else
	{
	  werror("lshd seems to be running already.\n");
	  return EXIT_FAILURE;
	}
    }
  {
    /* Commands to be invoked on the connection */
    /* FIXME: Use a queue instead. */
    struct object_list *connection_hooks;
    struct command *session_setup;
    
    /* Supported channel requests */
    struct alist *supported_channel_requests
      = make_alist(2,
		   ATOM_SHELL, &shell_request_handler,
		   ATOM_EXEC, &exec_request_handler,
		   -1);
    
#if WITH_PTY_SUPPORT
    if (options->with_pty)
      {
        ALIST_SET(supported_channel_requests,
                  ATOM_PTY_REQ, &pty_request_handler.super);
        ALIST_SET(supported_channel_requests,
                  ATOM_WINDOW_CHANGE, &window_change_request_handler.super);
      }
#endif /* WITH_PTY_SUPPORT */

#if WITH_X11_FORWARD
      if (options->with_x11_forward)
        ALIST_SET(supported_channel_requests,
		  ATOM_X11_REQ, &x11_req_handler.super);
#endif /* WITH_X11_FORWARD */

    if (options->subsystems)
      ALIST_SET(supported_channel_requests,
		ATOM_SUBSYSTEM,
		&make_subsystem_handler(options->subsystems)->super);
		
    session_setup = make_install_fix_channel_open_handler
      (ATOM_SESSION, make_open_session(supported_channel_requests));
    
#if WITH_TCP_FORWARD
    if (options->with_tcpip_forward)
      connection_hooks = make_object_list
	(4,
	 session_setup,
	 make_tcpip_forward_hook(),
	 make_install_fix_global_request_handler
	 (ATOM_CANCEL_TCPIP_FORWARD, &tcpip_cancel_forward),
	 make_direct_tcpip_hook(),
	 -1);
    else
#endif
      connection_hooks
	= make_object_list (1, session_setup, -1);
    {
      CAST_SUBTYPE(command, connection_service,
		   make_lshd_connection_service(connection_hooks));
      CAST_SUBTYPE(command, server_listen, 		   
		   make_lshd_listen
		   (make_handshake_info(CONNECTION_SERVER,
					"lsh - a free ssh",
					NULL,
					SSH_MAX_PACKET,
					options->random,
					options->super.algorithms,
					options->sshd1),
		    make_simple_kexinit
		    (options->random,
		     options->kex_algorithms,
		     options->super.hostkey_algorithms,
		     options->super.crypto_algorithms,
		     options->super.mac_algorithms,
		     options->super.compression_algorithms,
		     make_int_list(0, -1)),
		    make_offer_service
		    (make_alist
		     (1,
		      ATOM_SSH_USERAUTH,
		      make_userauth_service(options->userauth_methods,
					    options->userauth_algorithms,
					    make_alist(1, ATOM_SSH_CONNECTION,
						       connection_service,-1)),
		      -1))));

      static const struct report_exception_info report =
	STATIC_REPORT_EXCEPTION_INFO(EXC_IO, EXC_IO,
				     "lshd: ");
	    
      
      COMMAND_CALL(server_listen, options,
		   &discard_continuation,
		   make_report_exception_handler
		   (&report,
		    options->e,
		    HANDLER_CONTEXT));
    }
  }
  
  io_run();

  io_final();
  
  return 0;
}
