/* password.h
 *
 * System dependant password related functions.
 *
 * $Id$
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

#ifndef LSH_PASSWORD_H_INCLUDED
#define LSH_PASSWORD_H_INCLUDED

#include "lsh_types.h"

#include "alist.h"

#include <sys/types.h>
#include <unistd.h>

struct lsh_string *
read_password(int max_length, struct lsh_string *prompt, int free);

/* CLASS:
   (class
     (name unix_user)
     (vars
       (uid simple uid_t)
       (gid simple gid_t)
       
       ; These strings include a terminating NUL-character, for
       ; compatibility with library and system calls.
       (name string)
       (passwd string)  ; Crypted passwd
       (home string)
       (shell string)))
       
*/

#if 0
struct unix_user
{
  struct lsh_object header;
  
  uid_t uid;
  gid_t gid;
  
  /* These strings include a terminating NUL-character,
   * for compatibility with library and system calls. */
  struct lsh_string *name;
  struct lsh_string *passwd; /* Crypted passwd */
  struct lsh_string *home;
  struct lsh_string *shell; 
};
#endif

struct unix_user *lookup_user(struct lsh_string *name, int free);
int verify_password(struct unix_user *user,
		    struct lsh_string *password, int free);

struct userauth *make_password_userauth(void);

/* CLASS:
   (class
     (name unix_service)
     (vars
       (login method (object ssh_service) "struct unix_user *user")))
*/

#if 0
struct unix_service
{
  struct lsh_object header;

  struct ssh_service * (*login)(struct unix_service *closure,
                                struct unix_user *user);
};
#endif

#define LOGIN(s, u) ((s)->login((s), (u)))

struct userauth *make_unix_userauth(struct alist *services);

int change_uid(struct unix_user *user);
int change_dir(struct unix_user *user);

#if 0
struct login_method
{
  struct lsh_object header;

  struct ssh_service * (*login)(struct login_method *closure,
				struct unix_user *user,
				struct ssh_service *service);
};

#define LOGIN(m, u, s) ((m)->login((m), (u), (s)))

struct userauth *make_unix_userauth(struct login_method *login,
				    struct alist *services);
struct login_method *make_unix_login(void);
#endif

#endif /* LSH_PASSWORD_H_INCLUDED */
