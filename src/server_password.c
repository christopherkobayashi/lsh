/* server_password.c
 *
 * System dependant password related functions.
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

#include "server_password.h"

#include "charset.h"
#include "format.h"
#include "parse.h"
#include "userauth.h"
#include "werror.h"
#include "xalloc.h"

#include <string.h>
#include <errno.h>

#if HAVE_CRYPT_H
# include <crypt.h>
#endif
#include <pwd.h>
#include <grp.h>

#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#define GABA_DEFINE
#include "server_password.h.x" 
#undef GABA_DEFINE

/* NOTE: Calls functions using the disgusting convention of returning
 * pointers to static buffers. */
struct unix_user *lookup_user(struct lsh_string *name, int free)
{
  struct passwd *passwd;

  NEW(unix_user, res);

  name = make_cstring(name, free);

  if (!name)
    {
      KILL(res);
      return 0;
    }
  
  if (!(passwd = getpwnam(name->data)))
    {
      lsh_string_free(name);
      KILL(res);
      return 0;
    }

  res->uid = passwd->pw_uid;
  res->gid = passwd->pw_gid;
  res->name = name;
  
#if HAVE_GETSPNAM
  /* FIXME: What's the most portable way to test for shadow passwords?
   * A single character in the passwd field should cover most variants. */
  if (passwd->pw_passwd && (strlen(passwd->pw_passwd) == 1))
  {
    struct spwd *shadowpwd;
    
    if (!(shadowpwd = getspnam(name->data)))
    {
      KILL(res);
      return 0;
    }
    res->passwd = format_cstring(shadowpwd->sp_pwdp);
  }
  else
#endif /* HAVE_GETSPNAM */
    res->passwd = format_cstring(passwd->pw_passwd);

  res->home = format_cstring(passwd->pw_dir);
  res->shell = format_cstring(passwd->pw_shell);
  
  return res;
}

/* NOTE: Calls functions using the *disgusting* convention of returning
 * pointers to static buffers. */
int verify_password(struct unix_user *user,
		    struct lsh_string *password, int free)
{
  char *salt;
  
  if (!user->passwd || (user->passwd->length < 2) )
    {
      /* FIXME: How are accounts without passwords handled? */
      lsh_string_free(password);
      return 0;
    }

  /* Convert password to a NULL-terminated string */
  password = make_cstring(password, free);

  if (!password)
    return 0;
  
  salt = user->passwd->data;

  if (strcmp(crypt(password->data, salt), user->passwd->data))
    {
      /* Passwd doesn't match */
      lsh_string_free(password);
      return 0;
    }

  lsh_string_free(password);
  return 1;
}


static int do_authenticate(struct userauth *ignored UNUSED,
			   struct lsh_string *username,
			   struct simple_buffer *args,
			   struct lsh_object **result)
{
  struct lsh_string *password = NULL;
  /* struct unix_service *service; */
  int change_passwd;

#if 0
  if (!( (service = ALIST_GET(closure->services, requested_service))))
    {
      lsh_string_free(username);
      return LSH_AUTH_FAILED;
    }
#endif
  
  username = utf8_to_local(username, 1, 1);
  if (!username)
    return LSH_FAIL | LSH_DIE;

  if (parse_boolean(args, &change_passwd))
    {
      if (change_passwd)
	{
	  /* Password changeing is not implemented. */
	  lsh_string_free(username);
	  return LSH_AUTH_FAILED;
	}
      if ( (password = parse_string_copy(args))
	   && parse_eod(args))
	{
	  struct unix_user *user;

	  password = utf8_to_local(password, 1, 1);

	  if (!password)
	    {
	      lsh_string_free(username);
	      return LSH_FAIL | LSH_DIE;
	    }
       
	  user = lookup_user(username, 1);

	  if (!user)
	    {
	      lsh_string_free(password);
	      return LSH_AUTH_FAILED;
	    }

	  if (verify_password(user, password, 1))
	    {
	      *result = &user->super;
	      return LSH_OK | LSH_GOON;
	    }
	  else
	    {
	      KILL(user);
	      return LSH_AUTH_FAILED;
	    }
	}
    }
  
  /* Request was invalid */
  lsh_string_free(username);

  if (password)
    lsh_string_free(password);
  
  return LSH_FAIL | LSH_DIE;
}

struct userauth unix_userauth =
{ STATIC_HEADER, do_authenticate };


int change_uid(struct unix_user *user)
{
  /* NOTE: Error handling is crucial here. If we do something
   * wrong, the server will think that the user is logged in
   * under his or her user id, while in fact the process is
   * still running as root. */
  if (initgroups(user->name->data, user->gid) < 0)
    {
      werror("initgroups failed: %z\n", strerror(errno));
      return 0;
    }
  if (setgid(user->gid) < 0)
    {
      werror("setgid failed: %z\n", strerror(errno));
      return 0;
    }
  if (setuid(user->uid) < 0)
    {
      werror("setuid failed: %z\n", strerror(errno));
      return 0;
    }
  return 1;
}

int change_dir(struct unix_user *user)
{
  /* Change to user's home directory. FIXME: If the server is running
   * as the same user, perhaps it's better to use $HOME? */
  if (!user->home)
    {
      if (chdir("/") < 0)
	{
	  werror("Strange: home directory was NULL, and chdir(\"/\") failed: %z\n",
		 strerror(errno));
	  return 0;
	}
    }
  else if (chdir(user->home->data) < 0)
    {
      werror("chdir to %z failed (using / instead): %z\n",
	     user->home ? (char *) user->home->data : "none",
	     strerror(errno));
      if (chdir("/") < 0)
	{
	  werror("chdir(\"/\") failed: %z\n", strerror(errno));
	  return 0;
	}
    }
  return 1;  
}

#if 0

struct setuid_service
{
  struct ssh_service super;

  struct unix_user *user;
  /* Service to start once we have changed to the correct uid. */
  struct ssh_service *service;
};

/* NOTE: This is used only if early forking (i.e., for directly after
 * user autentication) is enabled. */
static int do_setuid(struct ssh_service *c,
		     struct ssh_connection *connection)
{
  CAST(setuid_service, closure, c);  
  uid_t server_uid = getuid();
  int res = 0;

  if (server_uid != closure->user->uid)
    {
      if (server_uid)
	/* Not root */
	return LSH_AUTH_FAILED;

      switch(fork())
	{
	case -1:
	  /* Error */
	  werror("fork failed: %z\n", strerror(errno));
	  return LSH_FAIL | LSH_DIE;
	case 0:
	  /* Child */

	  if (!change_uid(closure->user))
	    return  LSH_FAIL | LSH_DIE | LSH_KILL_OTHERS;;

	  res |= LSH_KILL_OTHERS;
	  break;
	default:
	  /* Parent */
	  return LSH_OK | LSH_DIE;
	}
    }

  /* Change to user's home directory. FIXME: If the server is running
   * as the same user, perhaps it's better to use $HOME? */
  if (!change_dir(closure->user))
    fatal("can't chdir: giving up\n");

  /* Initialize environment, somehow. In particular, the HOME and
   * LOGNAME variables */

  return res | LOGIN(closure->service, closure->user);		     connection);
}

/* FIXME: This function is not quite adequate, as it does not pass the
 * user struct on to the started service. */

static struct ssh_service *do_login(struct login_method *closure,
				    struct unix_user *user,
				    struct ssh_service *service)
{
  struct setuid_service *res;

  MDEBUG(closure);
  
  NEW(res);
  res->super.init = do_setuid;
  res->user = user;
  res->service = service;

  return &res->super;
}

struct login_method *make_unix_login(void)
{
  struct login_method *self;

  NEW(self);
  self->login = do_login;

  return self;
}
#endif
