/* client_keyexchange.h
 *
 * Client specific key exchange handling
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LSH_CLIENT_KEYEXCHANGE_H_INCLUDED
#define LSH_CLIENT_KEYEXCHANGE_H_INCLUDED

#include "keyexchange.h"
#include "publickey_crypto.h"

/* Maps a key blob to a signature verifier, using some signature
 * algorithm and some method to determine the authenticity of the key.
 * Returns NULL If the key is invalid or not trusted. */

/* FIXME: This function needs the hostname we are connecting to. */
struct lookup_verifier
{
  struct verifier * (*lookup)(struct lookup_verifier *closure,
			      struct lsh_string *key);
};

#define LOOKUP_VERIFIER(l, key) ((l)->lookup((l), (key)))

struct dh_client_exchange
{
  struct keyexchange_algorithm super;
  struct diffie_hellman_method *dh;
  struct lookup_verifier *verifier;
};

/* Handler for the kex_dh_reply message */
struct dh_client
{
  struct packet_handler super;
  struct diffie_hellman_instance dh;
  struct lookup_verifier *verifier;
  struct install_keys *install;
  struct packet_handler *saved_kexinit_handler;
};

struct keyexchange_algorithm *
make_dh_client(struct diffie_hellman_method *dh,
	       struct lookup_verifier *verifier);

#endif /* LSH_CLIENT_KEYEXCHANGE_H_INCLUDED */
