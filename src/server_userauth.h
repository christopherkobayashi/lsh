/* server_userauth.h
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef LSH_SERVER_USERAUTH_H_INCLUDED
#define LSH_SERVER_USERAUTH_H_INCLUDED

#include "alist.h"

/* For uid_t and gid_t */
#include <unistd.h>

#define GABA_DECLARE
#include "server_userauth.h.x"
#undef GABA_DECLARE

/* GABA:
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

struct unix_user *lookup_user(struct lsh_string *name, int free);
int verify_password(struct unix_user *user,
		    struct lsh_string *password, int free);

int change_uid(struct unix_user *user);
int change_dir(struct unix_user *user);

/* authentication methods */
extern struct userauth unix_userauth;
struct userauth *make_userauth_publickey(struct alist *verifiers);

#endif /* LSH_SERVER_USERAUTH_H_INCLUDED */
