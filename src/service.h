/* service.h
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

#ifndef LSH_SERVICE_H_INCLUDED
#define LSH_SERVICE_H_INCLUDED

#include "alist.h"
#include "connection.h"

/* Used for both proper services (i.e. services that can be requested
 * in a SSH_MSG_SERVICE_REQUEST or SSH_MSG_USERAUTH_REQUEST) and for
 * any other stuff that needs initialization at some later time. */

#if 0
#define GABA_DECLARE
#include "service.h.x"
#undef GABA_DECLARE
#endif

/* GABA:
   (class
     (name ssh_service)
     (vars
       (init method int "struct ssh_connection *c")))
*/

#define SERVICE_INIT(s, c) ((s)->init((s), (c)))

#if 0
/* services is an alist mapping names to service objects */
struct packet_handler *
make_service_request_handler(struct alist *services,
			     struct command_continuation *c);
#endif

#if 0
int request_service(int name, struct ssh_service * service);
#endif

struct lsh_string *format_service_request(int name);
struct lsh_string *format_service_accept(int name);

struct ssh_service *make_meta_service(struct alist *services);

#endif /* LSH_SERVICE_H_INCLUDED */
