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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "password.h"

#include "charset.h"
#include "format.h"
#include "parse.h"
#include "userauth.h"
#include "werror.h"
#include "xalloc.h"

#include <string.h>
#include <errno.h>

#include <pwd.h>
#include <grp.h>

static struct lsh_string *format_cstring(char *s)
{
  return s ? ssh_format("%lz", s) : NULL;
}

static struct lsh_string *make_cstring(struct lsh_string *s, int free)
{
  struct lsh_string *res;
  
  if (memchr(s->data, '\0', s->length))
    {
      if (free)
	lsh_string_free(s);
      return 0;
    }

  res = ssh_format("%lS", s);
  if (free)
    lsh_string_free(s);
  return res;
}
    
/* NOTE: Calls function using the *disgusting* convention of returning
 * pointers to static buffers. */
struct unix_user *lookup_user(struct lsh_string *name, int free)
{
  struct passwd *passwd;
  struct unix_user *res;

  name = make_cstring(name, free);

  if (!name)
    return 0;
  
  if (!(passwd = getpwnam(name->data)))
    return 0;
  
  NEW(res);
  res->uid = passwd->pw_uid;
  res->gid = passwd->pw_gid;
  res->username = name;
  res->passwd = format_cstring(passwd->pw_passwd);
  res->home = format_cstring(passwd->pw_dir);

  return res;
}

/* NOTE: Calls function using the *disgusting* convention of returning
 * pointers to static buffers. */
int verify_password(struct unix_user *user,
		    struct lsh_string *password, int free)
{
  char *salt;
  
  /* Convert password to a NULL-terminated string */
  password = make_cstring(password, free);

  if (!user->passwd || (user->passwd->length < 2) )
    {
      /* FIXME: How are accounts without passwords handled? */
      lsh_string_free(password);
      return 0;
    }

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

struct unix_authentication
{
  struct userauth super;

  struct login_method *login;
  struct alist *services;  /* Services allowed */
};

static int do_authenticate(struct userauth *c,
			   struct lsh_string *username,
			   int requested_service,
			   struct simple_buffer *args,
			   struct ssh_service **result)
{
  struct unix_authentication *closure = (struct unix_authentication *) c;
  struct lsh_string * password = NULL;
  struct ssh_service *service;
  
  MDEBUG(closure);

  if (!( (service = ALIST_GET(closure->services, requested_service))))
    {
      lsh_string_free(username);
      return LSH_AUTH_FAILED;
    }

  username = utf8_to_local(username, 1);
  if (!username)
    return 0;
  
  if ( (password = parse_string_copy(args))
       && parse_eod(args))
    {
      struct unix_user *user;
      int access;

      password = utf8_to_local(password, 1);

      if (!password)
	{
	  lsh_string_free(username);
	  return LSH_AUTH_FAILED;
	}
       
      user = lookup_user(username, 1);

      if (!user)
	{
	  lsh_string_free(password);
	  return LSH_AUTH_FAILED;
	}

      access = verify_password(user, password, 1);

      if (access)
	{
	  *result = LOGIN(closure->login, user, service);
	  return LSH_OK | LSH_GOON;
	}
      else
	{
	  /* FIXME: Free user struct */
	  return LSH_AUTH_FAILED;
	}
    }
  lsh_string_free(username);

  if (password)
    lsh_string_free(password);
  return 0;
}
  
struct userauth *make_unix_userauth(struct login_method *login,
				    struct alist *services)
{
  struct unix_authentication *closure;

  NEW(closure);
  closure->super.authenticate = do_authenticate;
  closure->login = login;
  closure->services = services;

  return &closure->super;
}

struct setuid_service
{
  struct ssh_service super;

  struct unix_user *user;
  /* Service to start once we have changed to the correct uid. */
  struct ssh_service *service;
};

static int do_setuid(struct ssh_service *c,
		     struct ssh_connection *connection)
{
  struct setuid_service *closure  = (struct setuid_service *) c;  
  uid_t server_uid = getuid();
  int res = 0;

  MDEBUG(closure);
  
  if (server_uid != closure->user->uid)
    {
      if (server_uid)
	/* Not root */
	return LSH_AUTH_FAILED;

      switch(fork())
	{
	case -1:
	  /* Error */
	  werror("fork failed: %s\n", strerror(errno));
	  return LSH_FAIL | LSH_DIE;
	case 0:
	  /* Child */
	  
	  /* NOTE: Error handling is crucial here. If we do something
	   * wrong, the server will think that the user is logged in
	   * under his or her user id, while in fact the process is
	   * still running as root. */
	  if (initgroups(closure->user->username->data, closure->user->gid) < 0)
	    {
	      werror("initgroups failed: %s\n", strerror(errno));
	      return LSH_FAIL | LSH_DIE | LSH_KILL_OTHERS;
	    }
	  if (setgid(closure->user->gid) < 0)
	    {
	      werror("setgid failed: %s\n", strerror(errno));
	      return LSH_FAIL | LSH_DIE | LSH_KILL_OTHERS;
	    }
	  if (setuid(closure->user->uid) < 0)
	    {
	      werror("setuid failed: %s\n", strerror(errno));
	      return LSH_FAIL | LSH_DIE | LSH_KILL_OTHERS;
	    }
	  
	  res |= LSH_KILL_OTHERS;
	  break;
	default:
	  /* Parent */
	  return LSH_OK | LSH_DIE;
	}
    }

  /* Change to user's home directory. FIXME: If the server is running
   * as the same user, perhaps it's better to use $HOME? */
  if (!closure->user->home)
    {
      if (chdir("/") < 0)
	fatal("Strange: home directory was NULL, and chdir(\"/\") failed: %s\n",
	      strerror(errno));
    }
  else
    if (chdir(closure->user->home->data) < 0)
      {
	werror("chdir to %s failed (using / instead): %s\n",
	       closure->user->home ? (char *) closure->user->home->data : "none",
	       strerror(errno));
	if (chdir("/") < 0)
	  fatal("chdir(\"/\") failed: %s\n", strerror(errno));
      }

  /* Initialize environment, somehow. In particular, the HOME and
   * LOGNAME variables */

  /* If closure->user is not needed anymore, deallocate it (and set
   * the pointer to NULL, to tell gc about that). */

  return res | SERVICE_INIT(closure->service, connection);
}

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
