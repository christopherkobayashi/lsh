/* server_userauth.h
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
#include "command.h"
#include "connection.h"
#include "parse.h"
#include "userauth.h"

/* For uid_t and gid_t */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#define GABA_DECLARE
#include "server_userauth.h.x"
#undef GABA_DECLARE

/* FIXME: Could abstract some of this information into methods, for example
 *
 * o  verifying a password
 * o  reading files in ~/.lsh
 * o  cd:ing to the home directory
 */

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

/* GABA:
   (class
     (name user_db)
     (vars
       (lookup method "struct unix_user *"
                      "struct lsh_string *name" "int free")))
*/

struct unix_user *lookup_user(struct lsh_string *name, int free);
int verify_password(struct unix_user *user,
		    struct lsh_string *password, int free);

int change_uid(struct unix_user *user);
int change_dir(struct unix_user *user);

/* GABA:
   (class
     (name userauth)
     (vars
       (authenticate method void
                     "struct ssh_connection *connection"
		     ; The name is consumed by this function
		     "struct lsh_string *username"
		     "UINT32 service"
		     "struct simple_buffer *args"
		     "struct command_continuation *c"
		     "struct exception_handler *e")))
*/

#define AUTHENTICATE(s, n, u, v, a, c, e) \
((s)->authenticate((s), (n), (u), (v), (a), (c), (e)))

/* NOTE: This class struct is used also by proxy_userauth.c. */

/* GABA:
   (class
     (name userauth_service)
     (super command)
     (vars
       (advertised_methods object int_list)
       (methods object alist)
       (services object alist)))
*/

struct lsh_string *
format_userauth_failure(struct int_list *methods,
			int partial);
struct lsh_string *
format_userauth_success(void);

struct packet_handler *
make_userauth_handler(struct alist *methods,
                      struct alist *services,
                      struct command_continuation *c,
                      struct exception_handler *e);


/* authentication methods */
extern struct userauth unix_userauth;
struct userauth *make_userauth_publickey(struct alist *verifiers);

struct command *make_userauth_service(struct int_list *advertised_methods,
				      struct alist *methods,
				      struct alist *services);

#endif /* LSH_SERVER_USERAUTH_H_INCLUDED */
