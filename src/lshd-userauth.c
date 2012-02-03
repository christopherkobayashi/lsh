/* lshd-userauth.c
 *
 * Main program for the ssh-userauth service.
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2005 Niels Möller
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
#include <stdarg.h>
#include <string.h>

#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#if HAVE_CRYPT_H
# include <crypt.h>
#endif

#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "nettle/base16.h"
#include "nettle/macros.h"

#include "charset.h"
#include "crypto.h"
#include "environ.h"
#include "list.h"
#include "lsh_string.h"
#include "format.h"
#include "io.h"
#include "parse.h"
#include "server.h"
#include "ssh.h"
#include "version.h"
#include "werror.h"
#include "xalloc.h"

#include "lsh_argp.h"

#include "lshd-userauth.c.x"

#define HEADER_SIZE 8

static struct lsh_string *
format_userauth_failure(struct int_list *methods,
			int partial)
{
  return ssh_format("%c%A%c", SSH_MSG_USERAUTH_FAILURE, methods, partial);
}

static struct lsh_string *
format_userauth_success(void)
{
  return ssh_format("%c", SSH_MSG_USERAUTH_SUCCESS);
}

static struct lsh_string *
format_userauth_pk_ok(int algorithm,
		      uint32_t key_length, const uint8_t *key)
{
  return ssh_format("%c%a%s", SSH_MSG_USERAUTH_PK_OK, 
		    algorithm, key_length, key);
}

/* We use blocking i/o through out. */
static struct lsh_string *
read_packet(uint32_t *seqno)
{
  uint8_t header[HEADER_SIZE];
  uint32_t length;
  struct lsh_string *packet;
  uint32_t done;

  for (done = 0; done < HEADER_SIZE; )
    {
      int res;
      do
	res = read(STDIN_FILENO, header + done, HEADER_SIZE - done);
      while (res < 0 && errno == EINTR);
      if (res <= 0)
	{
	  if (res == 0)
	    {
	      if (done > 0)
		die("read_packet: End of file after %i header octets.\n",
		    done);
	      else
		exit(EXIT_SUCCESS);
	    }
	  else
	    die("read_packet: read failed after %i header octets: %e.\n",
		done, errno);
	}
      done += res;
    }
  
  *seqno = READ_UINT32(header);
  length = READ_UINT32(header + 4);

  if (length > SSH_MAX_PACKET)
    die("Too large packet.\n");

  packet = lsh_string_alloc(length);

  for (done = 0; done < length; )
    {
      int res;

      res = lsh_string_read(packet, done, STDIN_FILENO, length - done);

      if (res <= 0)
	{
	  if (res == 0)
	    die("read_packet: End of file after %i data octets.\n",
		done);
	  else
	    die("read_packet: read failed after %i data octets: %e.\n",
		done, errno);
	}
      done += res;
    }
  return packet;
}

static void
write_packet(struct lsh_string *packet)
{
  /* Sequence number not supported */  
  packet = ssh_format("%i%fS", 0, packet);

  if (!write_raw(STDOUT_FILENO, STRING_LD(packet)))
    die("write_packet: write failed: %e.\n", errno);

  lsh_string_free(packet);
}

static void
protocol_error(const char *msg) NORETURN;

static void
protocol_error(const char *msg)
{
  write_packet(format_disconnect(SSH_DISCONNECT_PROTOCOL_ERROR,
				 msg, ""));
  die("Protocol error: %z.\n", msg);
}

static void
service_error(const char *msg) NORETURN;

static void
service_error(const char *msg)
{
  write_packet(format_disconnect(SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
				 msg, ""));
  die("Service not available: %z.\n", msg);
  exit(EXIT_FAILURE);  
}

/* Some or all of these pointers are returned by getpwnam, and hence
   destroyed after the next lookup. Can we trust the libc to never
   call getpwnam and friends behind our back? */
   
struct lshd_user
{
  /* Name, in local charset */
  struct lsh_string *name;
  uid_t uid;
  gid_t gid;
  const char *crypted;
  const char *home;
  const char *shell;
};

static void
lshd_user_init(struct lshd_user *self)
{
  self->name = NULL;
}  

static void
lshd_user_clear(struct lshd_user *self)
{
  lsh_string_free(self->name);
  self->name = NULL;
}

/* It's somewhat tricky to determine when accounts are disabled. To be
 * safe, it is recommended that all disabled accounts have a harmless
 * login-shell, like /bin/false.
 *
 * We return NULL for disabled accounts, according to the following
 * rules:
 *
 * If our uid is non-zero, i.e. we're not running as root, then an
 * account is considered valid if and only if it's uid matches the
 * server's, and the password field is non-empty. We never try
 * checking the shadow record.
 *
 * If we're running as root, first check the passwd record.
 *
 * o If the uid is zero, consider the account disabled. --root-login
 *   omits this check.
 *
 * o If the passwd equals "x", look up the shadow record, check
 *   expiration etc, and replace the passwd value with the one from the
 *   shadow record. If there's no shadow record, consider the account
 *   disabled.
 *
 * o If the passwd field is empty, consider the account disabled (we
 *   usually don't want remote logins on password-less accounts). We may
 *   need to make this check optional, though.
 *
 * o If the passwd entry starts with a "*" and is longer than one
 *   character, consider the account disabled. (Other bogus values like
 *   "NP" means that the account is enabled, only password login is
 *   disabled)
 *
 * o Otherwise, the account is active, and a user record is returned.
 *
 * FIXME: What about systems that uses a single "*" to disable
 * accounts?
 *
 * FIXME: Check for /etc/nologin ?
 */

static int
lookup_user(struct lshd_user *user, uint32_t name_length,
	    const uint8_t *name_utf8, int allow_root_login)
{
  struct passwd *passwd;
  const char *cname;
  uid_t me;

  if (memchr(name_utf8, 0, name_length))
    return 0;

  lshd_user_clear(user);
  user->name = low_utf8_to_local(name_length, name_utf8, utf8_paranoid);

  cname = lsh_get_cstring(user->name);
  
  /* NUL:s should be filtered out during the utf8 conversion. */
  assert(cname);

  passwd = getpwnam(cname);
  if (!passwd)
    return 0;

  user->crypted = passwd->pw_passwd;
  if (!user->crypted || !*user->crypted)
    /* Ignore accounts with empty passwords. */
    return 0;

  user->shell = passwd->pw_shell;
  /* If there's no shell, there's no reasonable default we can
     substitute, so consider the account disabled. */
  if (!user->shell || !*user->shell)
    return 0;

  user->uid = passwd->pw_uid;
  user->gid = passwd->pw_gid;
  user->home = passwd->pw_dir;
  
  me = getuid();
  if (me)
    {
      const char *home;
      
      /* Not root. We can't login anybody but ourselves. */
      if (user->uid != me)
	return 0;

      /* Skip all other checks, a non-root user running lshd
	 presumably wants to be able to login to his or her
	 account. */

      /* NOTE: If we are running as the uid of the user, it seems like
       * a good idea to let the HOME environment variable override the
       * passwd-database. I think the testsuite depends on this. */
      home = getenv(ENV_HOME);
      if (home)
	user->home = home;

      /* FIXME: Do the same with $SHELL? */
    }
  else
    {
      /* No root login, unless explicitly enabled.  */
      if (!allow_root_login && !user->uid)
	return 0;

#if HAVE_GETSPNAM && HAVE_SHADOW_H

      /* FIXME: What's the most portable way to test for shadow
       * passwords? For now, we look up shadow database if and only if
       * the passwd field equals "x". If there's no shadow record, we
       * just keep the value from the passwd-database, the user may be
       * able to login using a publickey, or the password helper. */
      if (strcmp(user->crypted, "x") == 0)
	{
	  /* Current day number since January 1, 1970.
	   *
	   * FIXME: Which timezone is used in the /etc/shadow file? */
	  long now = time(NULL) / (3600 * 24);
	  struct spwd *shadowpwd = getspnam(cname);
	  
	  if (!shadowpwd)
	    return 0;

          /* sp_expire == -1 means there is no account expiration date.
           * although chage(1) claims that sp_expire == 0 does this */
	  if ( (shadowpwd->sp_expire >= 0)
	       && (now > shadowpwd->sp_expire))
	    {
	      werror("Access denied for user '%pz', account expired.\n", cname); 
	      return 0;
	    }
	  		     
          /* sp_inact == -1 means expired password doesn't disable account.
	   *
	   * During the time
	   *
	   *   sp_lstchg + sp_max < now < sp_lstchg + sp_max + sp_inact
	   *
	   * the user is allowed to log in only by changing her
	   * password. As we don't support password change, this
	   * means that access is denied. */

          if ( (shadowpwd->sp_inact >= 0) &&
	       (now > (shadowpwd->sp_lstchg + shadowpwd->sp_max)))
            {
	      werror("Access denied for user '%pz', password too old.\n", cname);
	      return 0;
	    }

	  /* FIXME: We could look at sp_warn and figure out if it is
	   * appropriate to send a warning about passwords about to
	   * expire, and possibly also a
	   * SSH_MSG_USERAUTH_PASSWD_CHANGEREQ message.
	   *
	   * A warning is appropriate when
	   *
	   *   sp_lstchg + sp_max - sp_warn < now < sp_lstchg + sp_max
	   *
	   */

	  user->crypted = shadowpwd->sp_pwdp;

	  /* Check again for empty passwd field. */
	  if (!user->crypted || !*user->crypted)
	    return 0;
	}
#endif /* HAVE_GETSPNAM */

      /* A passwd field of more than one character, which starts with a star,
	 indicates a disabled account. */
      if (user->crypted[0] == '*' && user->crypted[1])
	return 0;
    }

  if (!user->home || !*user->home)
    user->home = "/";

  return 1;
}

static gid_t
lookup_group(const char *name, gid_t default_gid)
{
  struct group *group = getgrnam(name);
  return group ? group->gr_gid : default_gid;
}

#define AUTHORIZATION_DIR "authorized_keys_sha1"

static struct verifier *
get_verifier(struct lshd_user *user, enum lsh_atom algorithm,
	     uint32_t key_length, const uint8_t *key)
{
  struct verifier *v;
  struct lsh_string *file;
  const char *s;
  struct stat sbuf;
  
  switch(algorithm)
    {
    case ATOM_SSH_DSS:
      v = make_ssh_dss_verifier(key_length, key);
      break;
    case ATOM_SSH_DSA_SHA256_LOCAL:
      v = make_ssh_dsa_sha256_verifier(key_length, key);
      break;
    case ATOM_SSH_RSA:
      v = make_ssh_rsa_verifier(key_length, key);
      break;
    default:
      werror("Unknown publickey algorithm %a\n", algorithm);
      return NULL;
    }
  if (!v)
    return NULL;

  /* FIXME: We should have proper spki support */
  file = ssh_format("%lz/.lsh/" AUTHORIZATION_DIR "/%lxfS",
		    user->home,
		    hash_string(&nettle_sha1,
				PUBLIC_SPKI_KEY(v, 0),
				1));
  s = lsh_get_cstring(file);
  assert(s);

  /* FIXME: Use seteuid around the stat call? */
  if (stat(s, &sbuf) < 0)
    v = NULL;
  
  lsh_string_free(file);
  return v;
}

/* Returns 1 on success, 0 on failure, and -1 if we have sent an
   USERAUTH_PK_OK reply. */
static int
handle_publickey(struct simple_buffer *buffer,
		 struct lshd_user *user,
		 const struct lshd_userauth_config *config)
{
  int check_key;
  enum lsh_atom algorithm;
  uint32_t key_length;
  const uint8_t *key;
  uint32_t signature_start;
  uint32_t signature_length;
  const uint8_t *signature;
  struct lsh_string *signed_data;
  int res;
  
  struct verifier *v;

  if (!config->allow_publickey)
    return 0;

  if (! (parse_boolean(buffer, &check_key)
	 && parse_atom(buffer, &algorithm)
	 && parse_string(buffer, &key_length, &key)))
    protocol_error("Invalid USERAUTH_REQUEST \"publickey\"");

  signature_start = buffer->pos;

  if (check_key)
    {
      if (!parse_string(buffer, &signature_length, &signature))
	protocol_error("Invalid USERAUTH_REQUEST \"publickey\"");
    }
  
  if (!parse_eod(buffer))
    protocol_error("Invalid USERAUTH_REQUEST \"publickey\"");

  v = get_verifier(user, algorithm, key_length, key);
  if (!v)
    return 0;

  if (!check_key)
    {
      write_packet(format_userauth_pk_ok(algorithm, key_length, key));
      return -1;
    }

  /* The signature is on the session id, followed by the userauth
     request up to the actual signature. To avoid collisions, the
     length field for the session id is included. */
  signed_data = ssh_format("%S%ls", config->session_id, 
			   signature_start, buffer->data);

  res = VERIFY(v, algorithm,
	       lsh_string_length(signed_data), lsh_string_data(signed_data),
	       signature_length, signature);
  lsh_string_free(signed_data);
  KILL(v);  

  return res;
}

/* Returns 1 on success, 0 on failure. */
static int
handle_password(struct simple_buffer *buffer,
		struct lshd_user *user,
		const struct lshd_userauth_config *config)
{
  int change_passwd;
  uint32_t password_length;
  const uint8_t *password_data;
  const struct lsh_string *password;
  const char *cpassword;

  if (!config->allow_password)
    return 0;

  if (!(parse_boolean(buffer, &change_passwd)
	&& parse_string(buffer, &password_length, &password_data)))
      protocol_error("Invalid USERAUTH_REQUEST \"password\"");

  if (change_passwd)
    /* Not supported. */
    return 0;

  if (!parse_eod (buffer))
    protocol_error("Invalid USERAUTH_REQUEST \"password\"");

  password = low_utf8_to_local (password_length, password_data, 0);
  if (!password)
    return 0;

  cpassword = lsh_get_cstring (password);
  if (!cpassword)
    {
    fail:
      lsh_string_free (password);
      return 0;
    }

  /* FIXME: Currently no support for PAM, kerberos passwords, or
     password helper process. */

  /* NOTE: Check for accounts with empty passwords, or generally short
   * passwd fields like "NP" or "x". */
  if (!user->crypted || (strlen(user->crypted) < 5) )
    goto fail;

  if (strcmp(crypt(cpassword, user->crypted),
	     user->crypted))
    goto fail;

  /* Unix style password authentication succeded. */  
  lsh_string_free (password);
  return 1;  
}

#define MAX_ATTEMPTS 10

static const struct service_entry *
handle_userauth(struct lshd_user *user,
		const struct lshd_userauth_config *config)
{
  struct int_list *methods;
  unsigned attempt;
  unsigned i;

  methods = alloc_int_list(config->allow_password + config->allow_publickey);
  i = 0;

  if (config->allow_password)
    LIST(methods)[i++] = ATOM_PASSWORD;
  if (config->allow_publickey)
    LIST(methods)[i++] = ATOM_PUBLICKEY;

  /* FIXME: Don't count publickey requests without signature as failed
     attempts. */
  for (attempt = 0; attempt < MAX_ATTEMPTS; attempt++)
    {
      struct lsh_string *packet;
      struct simple_buffer buffer;
      unsigned msg_number;
      uint32_t user_length;
      const uint8_t *user_utf8;
      uint32_t seqno;

      uint32_t service_length;
      const uint8_t *service_name;
      const struct service_entry *service;

      enum lsh_atom method;
      int res;

      packet = read_packet(&seqno);
      
      trace("handle_userauth: Received packet.\n");
      simple_buffer_init(&buffer, STRING_LD(packet));

      if (!parse_uint8(&buffer, &msg_number))
	protocol_error("Received empty packet.\n");

      /* All supported methods use a single USERAUTH_REQUEST */
      if (msg_number != SSH_MSG_USERAUTH_REQUEST)
	{
	  write_packet(format_unimplemented(seqno));
	  lsh_string_free(packet);
	  continue;
	}

      if (! (parse_string(&buffer, &user_length, &user_utf8)
	     && parse_string(&buffer, &service_length, &service_name)
	     && parse_atom(&buffer, &method)))
	protocol_error("Invalid USERAUTH_REQUEST message");

      service = service_config_lookup(config->service_config,
				      service_length, service_name);
      
      if (!service)
	{
	  verbose("Request for unknown service %ps.\n",
		  service_length, service_name);
	fail:
	  write_packet(format_userauth_failure(methods, 0));
	  lsh_string_free(packet);
	  lshd_user_clear(user);
	  continue;
	}

      if (!lookup_user(user, user_length, user_utf8, config->allow_root_login))
	{
	  verbose("User lookup failed for user %pus.\n", user_length, user_utf8);
	  goto fail;
	}
      /* NOTE: Each called function have to check if the memthod is
	 enabled. */
      switch (method)
	{
	default:
	  verbose("Unsupported authentication method.\n");
	  goto fail;
	case ATOM_PUBLICKEY:
	  verbose("Processing publickey request.\n");
	  res = handle_publickey(&buffer, user, config);
	  break;
	case ATOM_PASSWORD:
	  verbose("Processing password request.\n");
	  res = handle_password(&buffer, user, config);
	  break;
	}
      switch (res)
	{
	default:
	  fatal("Internal error!\n");
	case 0:
	  goto fail;
	case -1:
	  lsh_string_free(packet);
	  break;
	case 1:
	  write_packet(format_userauth_success());
	  lsh_string_free(packet);
	  return service;
	}
    }
  return NULL;
}

static int
spawn_helper(const char *program, uid_t uid, gid_t gid)
{
  pid_t child;
  /* pipe[0] for the child, pipe[1] for the parent */ 
  int pipe[2];

  int type;

#ifdef SO_RECVUCRED
  /* Solaris' ucred passing works with SOCK_DGRAM sockets only */
  type = SOCK_DGRAM;
#else
  type = SOCK_STREAM;
#endif

  if (socketpair(AF_UNIX, type, 0, pipe) < 0)
    {
      werror("socketpair failed: %e.\n", errno);
      return -1;
    }

  child = fork();

  if (child < 0)
    {
      werror("fork failed: %e.\n", errno);
      close(pipe[0]);
      close(pipe[1]);
      return -1;
    }
  else if (child != 0)
    {
      /* Parent */
      close(pipe[0]);
      return pipe[1];
    }
  else
    {
      if (getuid() != uid)
	{
	  if (setgroups(0, NULL) < 0)
	    {
	      werror("setgroups failed: %e.\n", errno);
	      _exit(EXIT_FAILURE);
	    }
	  if (setgid(gid) < 0)
	    {
	      werror("setgid failed: %e.\n", errno);
	      _exit(EXIT_FAILURE);
	    }
	  if (setuid(uid) < 0)
	    {
	      werror("setuid failed: %e.\n", errno);
	      _exit(EXIT_FAILURE);
	    }
	}

      assert(getuid() == uid);
      
      if (dup2(pipe[0], STDIN_FILENO) < 0)
	{
	  werror("dup2 to stdin failed: %e.\n", errno);
	  _exit(EXIT_FAILURE);
	}
      if (dup2(pipe[0], STDOUT_FILENO) < 0)
	{
	  werror("dup2 to stdin failed: %e.\n", errno);
	  _exit(EXIT_FAILURE);
	}
      
      close(pipe[0]);
      close(pipe[1]);

      execl(program, program, NULL);
      werror("execl failed: %e.\n", errno);
      _exit(EXIT_FAILURE);
    }
}

static const char *
format_env_pair(const char *name, const char *value)
{
  return lsh_get_cstring(ssh_format("%lz=%lz", name, value));
}

/* Change persona, set up new environment, change directory, and exec
   the service process. */
static void
start_service(struct lshd_user *user, const char *program, const char **argv)
{
  /* We need place for SHELL, HOME, USER, LOGNAME, TZ, PATH,
     LSHD_CONFIG_DIR, LSHD_LIBEXEC_DIR, LSHD_CONNECTION_CONF and a
     terminating NULL */

  /* FIXME: Is it really the right way to propagate lshd-specific
     environment variables, such as LSHD_CONFIG_DIR, here? If we
     support providing command line options with the service, that
     might be a cleaner alternative. */

#define ENV_MAX 10

  const char *env[ENV_MAX];
  const char *tz = getenv(ENV_TZ);
  const char *cname = lsh_get_cstring(user->name);  
  const char *config_dir = getenv(ENV_LSHD_CONFIG_DIR);
  const char *libexec_dir = getenv(ENV_LSHD_LIBEXEC_DIR);
  const char *lshd_connection_conf = getenv(ENV_LSHD_CONNECTION_CONF);
  
  unsigned i;
  
  assert(cname);

  i = 0;
  env[i++] = format_env_pair(ENV_SHELL, user->shell);
  env[i++] = format_env_pair(ENV_HOME, user->home);
  env[i++] = format_env_pair(ENV_USER, cname);
  env[i++] = format_env_pair(ENV_LOGNAME, cname);
  env[i++] = ENV_PATH "=/bin:/usr/bin";
  if (tz)
    env[i++] = format_env_pair(ENV_TZ, tz);

  if (config_dir)
    env[i++] = format_env_pair(ENV_LSHD_CONFIG_DIR, config_dir);
  if (libexec_dir)
    env[i++] = format_env_pair(ENV_LSHD_LIBEXEC_DIR, libexec_dir);
  if (lshd_connection_conf)
    env[i++] = format_env_pair(ENV_LSHD_CONNECTION_CONF, lshd_connection_conf);
  env[i++] = NULL;

  assert(i <= ENV_MAX);
  
  if (user->uid != getuid())
    {
      if (initgroups(cname, user->gid) < 0)
	{
	  werror("start_service: initgroups failed: %e.\n", errno);
	  service_error("Failed to start service process");
	}
      if (setgid(user->gid) < 0)
	{
	  werror("start_service: setgid failed: %e.\n", errno);
	  service_error("Failed to start service process");
	}

      /* FIXME: On obscure systems, notably UNICOS, it's not enough to
	 change our uid, we must also explicitly lower our
	 privileges. */

      if (setuid(user->uid) < 0)
	{
	  werror("start_service: setuid failed: %e.\n", errno);
	  service_error("Failed to start service process");
	}
    }
  assert(user->uid == getuid());

  if (!user->home)
    goto cd_root;

  if (chdir(user->home) < 0)
    {
      werror("chdir to home directory `%z' failed: %e.\n", user->home, errno);

    cd_root:
      if (chdir("/") < 0)
	{
	  werror("chdir to `/' failed: %e.\n", errno);
	  _exit(EXIT_FAILURE);
	}
    }
    
  /* FIXME: We should use the user's login shell.

       $SHELL -c 'argv[0] "$@"' argv[0] argv[1] ...
       
     should work. Or perhaps even

       $SHELL -c '"$0" "$@"' argv[0] argv[1] ...

     Can we require that the login-shell is posix-style?
  */
  /* NOTE: Any relative PATH in argv[0] will be interpreted relative
     to the user's home directory. */
  execve(program, (char **) argv, (char **) env);

  werror("start_service: exec failed: %e.\n", errno);
}


/* Option parsing */

enum {
  OPT_SESSION_ID = 0x200,
  OPT_PASSWORD,
  OPT_NO_PASSWORD,
  OPT_PUBLICKEY,
  OPT_NO_PUBLICKEY,
  OPT_ROOT_LOGIN,
  OPT_NO_ROOT_LOGIN,
  OPT_SERVICE,
  OPT_ADD_SERVICE,
};

const char *argp_program_version
= "lshd-userauth (" PACKAGE_STRING ")";

const char *argp_program_bug_address = BUG_ADDRESS;

static const struct argp_child
main_argp_children[] =
{
  { &server_argp, 0, "", 0 },
  { NULL, 0, NULL, 0}
};

static const struct argp_option
main_options[] =
{
  /* Name, key, arg-name, flags, doc, group */
  { "session-id", OPT_SESSION_ID, "Session id", 0,
    "Session id from the transport layer.", 0 },

  { "service", OPT_SERVICE, "NAME { COMMAND LINE }", 0,
    "Service to offer.", 0 },
  { "add-service", OPT_ADD_SERVICE, "NAME { COMMAND LINE }", 0,
    "Service to offer. Unlike --service does not override other services "
    "listed in the config file.", 0 },

  { NULL, 0, NULL, 0, "Authentication options:", 0 },  
  { "allow-password", OPT_PASSWORD, NULL, 0,
    "Enable password user authentication.", 0},
  { "deny-password", OPT_NO_PASSWORD, NULL, 0,
    "Disable password user authentication (default).", 0},

  { "allow-publickey", OPT_PUBLICKEY, NULL, 0,
    "Enable publickey user authentication.", 0},
  { "deny-publickey", OPT_NO_PUBLICKEY, NULL, 0,
    "Disable publickey user authentication (default).", 0},

  { "allow-root-login", OPT_ROOT_LOGIN, NULL, 0,
    "Allow root user to login.", 0 },
  { "deny-root-login", OPT_NO_ROOT_LOGIN, NULL, 0,
    "Don't allow root user to login (default).", 0 },
  
  { NULL, 0, NULL, 0, NULL, 0 }
};

/* GABA:
   (class
     (name lshd_userauth_config)
     (super server_config)
     (vars
       (service_config object service_config)
       (session_id string)
       (allow_password . int)
       (allow_publickey . int)
       (allow_root_login . int)))
*/

static const struct config_parser
lshd_userauth_config_parser;

static struct lshd_userauth_config *
make_lshd_userauth_config(void)
{
  NEW(lshd_userauth_config, self);
  init_server_config(&self->super,
		     &lshd_userauth_config_parser,
		     FILE_LSHD_USERAUTH_CONF,
		     ENV_LSHD_USERAUTH_CONF);

  self->service_config = make_service_config();
  self->session_id = NULL;
  self->allow_password = -1;
  self->allow_publickey = -1;
  self->allow_root_login = -1;

  return self;
}

#define CASE_FLAG(opt, flag)			\
  case OPT_##opt:				\
    self->flag = 1;				\
    break;					\
  case OPT_NO_##opt:				\
    self->flag = 0;				\
    break

static error_t
main_argp_parser(int key, char *arg, struct argp_state *state)
{
  CAST(lshd_userauth_config, self, state->input);

  switch(key)
    {
    default:
      return ARGP_ERR_UNKNOWN;
    case ARGP_KEY_INIT:
      state->child_inputs[0] = &self->super;
      break;

    case ARGP_KEY_END:
      if (!self->session_id)
	argp_error(state, "Mandatory option --session-id is missing.");

      if (object_queue_is_empty(&self->service_config->services))
	{
	  struct service_entry *entry
	    = make_service_entry("ssh-connection", NULL);

	  arglist_push (&entry->args, FILE_LSHD_CONNECTION);

	  arglist_push (&entry->args, "--helper-fd");
	  arglist_push (&entry->args, "$(helper_fd)");
	  
	  /* Propagate werror-related options. */
	  if (self->super.super.verbose > 0)
	    arglist_push (&entry->args, "-v");
	  if (self->super.super.quiet > 0)
	    arglist_push (&entry->args, "-q");
	  if (self->super.super.debug > 0)
	    arglist_push (&entry->args, "--debug");
	  if (self->super.super.trace > 0)
	    arglist_push (&entry->args, "--trace");

	  assert (entry->args.argc > 0);
	  object_queue_add_head (&self->service_config->services,
				 &entry->super);
	}
      /* Defaults should have been setup at the end of the config
	 file parsing. */
      assert (self->allow_password >= 0);
      assert (self->allow_publickey >= 0);
      assert (self->allow_root_login >= 0);

      if (!(self->allow_password || self->allow_publickey))
	argp_error(state, "All user authentication methods disabled.\n");

      break;

    case OPT_SESSION_ID:
      self->session_id = lsh_string_hex_decode(strlen(arg), arg);
      if (!self->session_id)
	argp_error(state, "Invalid argument for --session-id.");
      break;

    case OPT_SERVICE:
      self->service_config->override_config_file = 1;
      /* Fall through */
    case OPT_ADD_SERVICE:
      service_config_argp(self->service_config,
			  state, "service", arg);      
      break;

    CASE_FLAG(PASSWORD, allow_password);
    CASE_FLAG(PUBLICKEY, allow_publickey);
    CASE_FLAG(ROOT_LOGIN, allow_root_login);
    }
  return 0;
}

static const struct argp
main_argp =
{ main_options, main_argp_parser, 
  NULL,
  "Handles the ssh-userauth service.\v"
  "Intended to be invoked by lshd.",
  main_argp_children,
  NULL, NULL
};

static const struct config_option
lshd_userauth_config_options[] = {
  { OPT_SERVICE, "service", CONFIG_TYPE_LIST, "Service to offer.",
    "ssh-connection = { lshd-connection --helper-fd $(helper_fd) "
    "--use-example-config }" },
  { OPT_PASSWORD, "allow-password", CONFIG_TYPE_BOOL,
    "Support for password user authentication", "no" },
  { OPT_PUBLICKEY, "allow-publickey", CONFIG_TYPE_BOOL,
    "Support for publickey user authentication", "yes" },
  { OPT_ROOT_LOGIN, "allow-root-login", CONFIG_TYPE_BOOL,
    "Allow the root user to login", "no" },

  { 0, NULL, 0, NULL, NULL }
};

static int
lshd_userauth_config_handler(int key, uint32_t value, const uint8_t *data UNUSED,
			     struct config_parser_state *state)
{
  CAST_SUBTYPE(lshd_userauth_config, self, state->input);
  switch (key)
    {
    case CONFIG_PARSE_KEY_INIT:
      state->child_inputs[0] = &self->super.super;
      break;
    case CONFIG_PARSE_KEY_END:
      /* Set up defaults, for values specified neither in the
	 configuration file nor on the command line. */
      if (self->allow_password < 0)
	self->allow_password = 0;
      if (self->allow_publickey < 0)
	self->allow_publickey = 0;
      if (self->allow_root_login < 0)
	self->allow_root_login = 0;

      break;

    case OPT_SERVICE:
      if (!service_config_option(self->service_config,
				 "service", value, data))
	return EINVAL;
      break;
    case OPT_PASSWORD:
      if (self->allow_password < 0)
	self->allow_password = value;
      break;
    case OPT_PUBLICKEY:
      if (self->allow_publickey < 0)
	self->allow_publickey = value;
      break;
    case OPT_ROOT_LOGIN:
      if (self->allow_root_login < 0)
	self->allow_root_login = value;
      break;
    }
  return 0;
}

static const struct config_parser *
lshd_userauth_config_children[] = {
  &werror_config_parser,
  NULL
};

static const struct config_parser
lshd_userauth_config_parser = {
  lshd_userauth_config_options,
  lshd_userauth_config_handler,
  lshd_userauth_config_children
};

int
main(int argc, char **argv)
{
  struct lshd_user user;
  struct lshd_userauth_config *config = make_lshd_userauth_config();
  const struct service_entry *service;
  int helper_fd;
  struct arglist args;
  const char *program;
  unsigned i;

  argp_parse(&main_argp, argc, argv, 0, NULL, config);

  trace("Started userauth service.\n");

  lshd_user_init(&user);

  service = handle_userauth(&user, config);

  if (!service)
    {
      write_packet(format_disconnect(SSH_DISCONNECT_SERVICE_NOT_AVAILABLE,
				     "Access denied", ""));
      exit(EXIT_FAILURE);      
    }

  helper_fd = -1;
  arglist_init (&args);

  program = service->args.argv[0];
  arglist_push (&args, program);

  /* If not absolute, interpret it relative to libexec_dir. */
  if (program[0] != '/')
    program = lsh_get_cstring(ssh_format("%lz/%lz",
					 config->service_config->libexec_dir,
					 program));
  
  for (i = 1; i < service->args.argc; i++)
    {
      const char *arg = service->args.argv[i];
      if (arg[0] == '$')
	{
	  if (!strcmp(arg+1, "(helper_fd)"))
	    {
	      /* With UNIX98-style ptys, utmp is sufficient privileges
		 for the helper program. */

	      /* FIXME: Make sure that the user can't attach a
		 debugger to this process. How? */
	      if (helper_fd < 0)
		{
		  /* FIXME: Make configurable? */
		  /* Interpreted relative to libexec_dir. */
		  const char *helper_program
		    = lsh_get_cstring(ssh_format("%lz/%lz",
						 config->service_config->libexec_dir,
						 FILE_LSHD_PTY_HELPER));
		  
		  helper_fd = spawn_helper(helper_program, user.uid,
					   lookup_group("utmp", user.gid));
		  if (helper_fd < 0)
		    die("Could not spawn helper process.\n");		  
		}
	      arg = lsh_get_cstring(ssh_format("%di", helper_fd));
	    }
	}
      arglist_push (&args, arg);
    }

  debug("exec of service %s, program %z. Argument list:\n",
	service->name_length, service->name, program);
  for (i = 0; i < args.argc; i++)
    debug("  %z\n", args.argv[i]);

  start_service(&user, program, args.argv);

  service_error("Failed to start service process");
}
