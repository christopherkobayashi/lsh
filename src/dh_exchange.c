/* dh_exchange.c
 *
 *
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

#include "publickey_crypto.h"

#include "connection.h"
#include "crypto.h"
#include "format.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

void
init_dh_instance(struct dh_method *m,
		 struct dh_instance *self,
		 struct ssh_connection *c)
{
  struct lsh_string *s;
  /* FIXME: The allocator could do this kind of initialization
   * automatically. */
  mpz_init(self->e);
  mpz_init(self->f);
  mpz_init(self->secret);
  mpz_init(self->K);
  
  self->method = m;
  self->hash = MAKE_HASH(m->H);
  self->exchange_hash = NULL;

  debug("init_dh_instance()\n"
	" V_C: %pS\n", c->versions[CONNECTION_CLIENT]);
  debug(" V_S: %pS\n", c->versions[CONNECTION_SERVER]);
  debug(" I_C: %xS\n", c->literal_kexinits[CONNECTION_CLIENT]);
  debug(" I_S: %xS\n", c->literal_kexinits[CONNECTION_SERVER]);

  s = ssh_format("%S%S%S%S",
		 c->versions[CONNECTION_CLIENT],
		 c->versions[CONNECTION_SERVER],
		 c->literal_kexinits[CONNECTION_CLIENT],
		 c->literal_kexinits[CONNECTION_SERVER]);
  HASH_UPDATE(self->hash, s->length, s->data);

  lsh_string_free(s);  

  /* We don't need the kexinit strings anymore. */
  lsh_string_free(c->literal_kexinits[CONNECTION_CLIENT]);
  lsh_string_free(c->literal_kexinits[CONNECTION_SERVER]);
  c->literal_kexinits[CONNECTION_CLIENT] = NULL;
  c->literal_kexinits[CONNECTION_SERVER] = NULL;
}

struct dh_method *
make_dh(struct abstract_group *G, struct hash_algorithm *H,
	struct randomness *r)
{
  NEW(dh_method, res);
  res->G = G;
  res->H = H;
  res->random = r;
  
  return res;
  
}

struct dh_method *
make_dh1(struct randomness *r)
{
  return make_dh(make_ssh_group1(), &sha1_algorithm, r);
}

/* R is set to a random, secret, exponent, and V set to is g^r */
void
dh_generate_secret(struct dh_method *self,
		   mpz_t r, mpz_t v)
{
  mpz_t tmp;

  /* Generate a random number, 1 < x <= p-1 = O(G) */
  mpz_init_set(tmp, self->G->order);  
  mpz_sub_ui(tmp, tmp, 1);
  bignum_random(r, self->random, tmp);
  mpz_add_ui(r, r, 1);
  mpz_clear(tmp);

  GROUP_POWER(self->G, v, self->G->generator, r);
}

struct lsh_string *
dh_make_client_msg(struct dh_instance *self)
{
  dh_generate_secret(self->method, self->secret, self->e);
  return ssh_format("%c%n", SSH_MSG_KEXDH_INIT, self->e);
}

int
dh_process_client_msg(struct dh_instance *self,
		      struct lsh_string *packet)
{
  struct simple_buffer buffer;
  unsigned msg_number;
  
  simple_buffer_init(&buffer, packet->length, packet->data);

  if (! (parse_uint8(&buffer, &msg_number)
	 && (msg_number == SSH_MSG_KEXDH_INIT)
	 && parse_bignum(&buffer, self->e)
	 && (mpz_cmp_ui(self->e, 1) > 0)
	 && GROUP_RANGE(self->method->G, self->e)
	 && parse_eod(&buffer) ))
    return 0;

  GROUP_POWER(self->method->G, self->K, self->e, self->secret);
  return 1;
}

void
dh_hash_update(struct dh_instance *self,
	       struct lsh_string *s, int free)
{
  debug("dh_hash_update: %xS\n", s);
  
  HASH_UPDATE(self->hash, s->length, s->data);
  if (free)
    lsh_string_free(s);
}

/* Hashes e, f, and the shared secret key */
void
dh_hash_digest(struct dh_instance *self)
{
  dh_hash_update(self, ssh_format("%n%n%n",
				  self->e, self->f,
				  self->K), 1);
  self->exchange_hash = lsh_string_alloc(self->hash->hash_size);
  HASH_DIGEST(self->hash, self->exchange_hash->data);
}

void
dh_make_server_secret(struct dh_instance *self)
{
  dh_generate_secret(self->method, self->secret, self->f);
}

struct lsh_string *
dh_make_server_msg(struct dh_instance *self,
		   struct signer *s)
{
  dh_hash_update(self, ssh_format("%S", self->server_key), 1);
  dh_hash_digest(self);

  return ssh_format("%c%S%n%fS",
		    SSH_MSG_KEXDH_REPLY,
		    self->server_key,
		    self->f, SIGN(s,
				  self->exchange_hash->length,
				  self->exchange_hash->data));
}

int
dh_process_server_msg(struct dh_instance *self,
		      struct lsh_string *packet)
{
  struct simple_buffer buffer;
  unsigned msg_number;
  
  simple_buffer_init(&buffer, packet->length, packet->data);

  if (!(parse_uint8(&buffer, &msg_number)
	&& (msg_number == SSH_MSG_KEXDH_REPLY)
	&& (self->server_key = parse_string_copy(&buffer))
	&& (parse_bignum(&buffer, self->f))
	&& (mpz_cmp_ui(self->f, 1) > 0)
	&& GROUP_RANGE(self->method->G, self->f)
	&& (self->signature = parse_string_copy(&buffer))
	&& parse_eod(&buffer)))
    return 0;

  dh_hash_update(self, ssh_format("%S", self->server_key), 1);
  
  GROUP_POWER(self->method->G, self->K, self->f, self->secret);
  return 1;
}
	  
int
dh_verify_server_msg(struct dh_instance *self,
		     struct verifier *v)
{
  dh_hash_digest(self);

  return VERIFY(v,
		self->hash->hash_size, self->exchange_hash->data,
		self->signature->length, self->signature->data);
}

