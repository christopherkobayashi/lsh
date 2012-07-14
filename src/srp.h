/* srp.h
 *
 * Declarations for Thomas Wu's Secure Remote Password Protocol
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2000 Niels Möller
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301  USA
 */

#ifndef LSH_SRP_H_INCLUDED
#define LSH_SRP_H_INCLUDED

#include "publickey_crypto.h"

#define GABA_DECLARE
#include "srp.h.x"
#undef GABA_DECLARE

/* GABA:
   (class
     (name srp_entry)
     (vars
       (name string)
       (salt string)
       (verifier bignum)))
*/

/* Copies the name, rather than consuming it. */
struct srp_entry *
make_srp_entry(const struct lsh_string *name,
	       const struct lsh_string *expr);

/* Consumes the salt */
struct lsh_string *
srp_make_verifier(const struct zn_group *G,
		  const struct nettle_hash *H,
		  struct lsh_string *salt,
		  struct lsh_string *name,
		  struct lsh_string *passwd);

void
srp_hash_password(mpz_t x,
		  const struct nettle_hash *H,
		  struct lsh_string *salt,
		  struct lsh_string *name,
		  struct lsh_string *passwd);

struct lsh_string *
srp_make_init_msg(struct dh_instance *dh, struct lsh_string *name);

/* Returns the name */
struct lsh_string *
srp_process_init_msg(struct dh_instance *self, struct lsh_string *packet);

struct lsh_string *
srp_make_reply_msg(struct dh_instance *dh, struct srp_entry *entry);

/* Returns the salt */
struct lsh_string *
srp_process_reply_msg(struct dh_instance *dh, struct lsh_string *packet);

struct lsh_string *
srp_make_client_proof(struct dh_instance *dh,
		      struct lsh_string **m2,
		      mpz_t x);

struct lsh_string *
srp_process_client_proof(struct dh_instance *dh, struct lsh_string *packet);

int
srp_process_server_proof(struct lsh_string *m2,
			 struct lsh_string *packet);

struct dh_method *
make_srp1(struct randomness *r);

#endif /* LSH_SRP_H_INCLUDED */
