/* kexinit.c
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998, 2005 Niels M�ller
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

#include "kexinit.h"

#include "format.h"
#include "lsh_string.h"
#include "parse.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#if 0
#define GABA_DEFINE
# include "kexinit.h.x"
#undef GABA_DEFINE
#endif

#define NLISTS 10

/* Arbitrary limit on list length */
/* An SSH-2.0-Sun_SSH_1.0 server has been reported to list 250
 * different algorithms, or in fact a list of installed locales. */
#define KEXINIT_MAX_ALGORITMS 500

struct kexinit *
parse_kexinit(struct lsh_string *packet)
{
  NEW(kexinit, res);
  struct simple_buffer buffer;
  unsigned msg_number;
  uint32_t reserved;
  
  struct int_list *lists[NLISTS];
  int i;
  
  simple_buffer_init(&buffer, STRING_LD(packet));

  if (!parse_uint8(&buffer, &msg_number)
      || (msg_number != SSH_MSG_KEXINIT) )
    {
      KILL(res);
      return NULL;
    }

  if (!parse_octets(&buffer, 16, res->cookie))
    {
      KILL(res);
      return NULL;
    }

  for (i = 0; i<NLISTS; i++)
    if ( !(lists[i] = parse_atom_list(&buffer, KEXINIT_MAX_ALGORITMS)))
      break;

  if ( (i<NLISTS)
       || !parse_boolean(&buffer, &res->first_kex_packet_follows)
       || !parse_uint32(&buffer, &reserved)
       || reserved || !parse_eod(&buffer) )
    {
      /* Bad format */
      int j;
      for (j = 0; j<i; j++)
	KILL(lists[j]);
      KILL(res);
      return NULL;
    }
  
  res->kex_algorithms = lists[0];
  res->server_hostkey_algorithms = lists[1];

  for (i=0; i<KEX_PARAMETERS; i++)
    res->parameters[i] = lists[2 + i];

  res->languages_client_to_server = lists[8];
  res->languages_server_to_client = lists[9];

  return res;
}

struct lsh_string *
format_kexinit(struct kexinit *kex)
{
  return ssh_format("%c%ls%A%A%A%A%A%A%A%A%A%A%c%i",
		    SSH_MSG_KEXINIT,
		    16, kex->cookie,
		    kex->kex_algorithms,
		    kex->server_hostkey_algorithms,
		    kex->parameters[KEX_ENCRYPTION_CLIENT_TO_SERVER],
		    kex->parameters[KEX_ENCRYPTION_SERVER_TO_CLIENT],
		    kex->parameters[KEX_MAC_CLIENT_TO_SERVER],
		    kex->parameters[KEX_MAC_SERVER_TO_CLIENT],
		    kex->parameters[KEX_COMPRESSION_CLIENT_TO_SERVER],
		    kex->parameters[KEX_COMPRESSION_SERVER_TO_CLIENT],
		    kex->languages_client_to_server,
		    kex->languages_server_to_client,
		    kex->first_kex_packet_follows, 0);
}

static int
select_algorithm(struct int_list *client_list,
		 struct int_list *server_list)
{
  /* FIXME: This quadratic complexity algorithm should do as long as
   * the lists are short. To avoid DOS-attacks, there should probably
   * be some limit on the list lengths. */
  unsigned i, j;

  for(i = 0; i < LIST_LENGTH(client_list); i++)
    {
      int a = LIST(client_list)[i];
      if (!a)
	/* Unknown algorithm */
	continue;
      for(j = 0; j < LIST_LENGTH(server_list); j++)
	if (a == LIST(server_list)[j])
	  return a;
    }

  return 0;
}

const char *
handle_kexinit(struct kexinit_state *self, struct lsh_string *packet,
	       struct alist *algorithms, int mode)
{
  int kex_algorithm_atom;

  int parameter[KEX_PARAMETERS];

  struct kexinit *msg = parse_kexinit(packet);

  int i;

  verbose("Received KEXINIT message. Key exchange initated.\n");

  assert(self->state == KEX_STATE_INIT);
  
  if (!msg)
    return "Invalid KEXINIT message";

  if (!LIST_LENGTH(msg->kex_algorithms))
    return "No keyexchange method";

  /* Save value for later signing */
  self->literal_kexinit[!mode] = lsh_string_dup(packet);
  self->kexinit[!mode] = msg;

  /* Caller must send our kexinit message first */ 
  assert(self->kexinit[mode]);
  
  /* Select key exchange algorithms */

  /* FIXME: Look at the hostkey algorithm as well. */
  if (LIST(self->kexinit[0]->kex_algorithms)[0]
      == LIST(self->kexinit[1]->kex_algorithms)[0])
    {
      /* Use this algorithm */
      kex_algorithm_atom
	= LIST(self->kexinit[0]->kex_algorithms)[0];

      self->state = KEX_STATE_IN_PROGRESS;
    }
  else
    {
      if (msg->first_kex_packet_follows)
	{
	  /* Wrong guess */
	  self->state = KEX_STATE_IGNORE;
	}

      /* FIXME: Ignores that some keyexchange algorithms require
       * certain features of the host key algorithms. */
      
      kex_algorithm_atom
	= select_algorithm(self->kexinit[0]->kex_algorithms,
			   self->kexinit[1]->kex_algorithms);

      if  (!kex_algorithm_atom)
	return "No common key exchange method";
    }
  
  self->hostkey_algorithm
    = select_algorithm(self->kexinit[0]->server_hostkey_algorithms,
		       self->kexinit[1]->server_hostkey_algorithms);

  /* FIXME: This is actually ok for SRP. */
  if (!self->hostkey_algorithm)
    return "No common hostkey algorithm";

  verbose("Selected keyexchange algorithm: %a\n"
	  "  with hostkey algorithm:       %a\n",
	  kex_algorithm_atom, self->hostkey_algorithm);
    
  for(i = 0; i<KEX_PARAMETERS; i++)
    {
      parameter[i]
	= select_algorithm(self->kexinit[0]->parameters[i],
			   self->kexinit[1]->parameters[i]);
      
      if (!parameter[i])
	return "Algorithm negotiation failed";
    }

  verbose("Selected bulk algorithms: (client to server, server to client)\n"
	  "  Encryption:             (%a, %a)\n"
	  "  Message authentication: (%a, %a)\n"
	  "  Compression:            (%a, %a)\n",
	  parameter[0], parameter[1],
	  parameter[2], parameter[3], 
	  parameter[4], parameter[5]);

  self->algorithm_list = alloc_object_list(KEX_LIST_LENGTH);
  
  for (i = 0; i<KEX_PARAMETERS; i++)
    LIST(self->algorithm_list)[i] = ALIST_GET(algorithms, parameter[i]);

  LIST(self->algorithm_list)[KEX_KEY_EXCHANGE] = ALIST_GET(algorithms, kex_algorithm_atom);

  return NULL;
}

/* Returns a hash instance for generating various session keys. Consumes K. */
struct hash_instance *
kex_build_secret(const struct hash_algorithm *H,
		 struct lsh_string *exchange_hash,
		 struct lsh_string *K)
{
  /* We include a length field for the key, but not for the exchange
   * hash. */
  
  struct hash_instance *hash = make_hash(H);
  struct lsh_string *s = ssh_format("%fS%lS", K, exchange_hash);

  hash_update(hash, STRING_LD(s));
  lsh_string_free(s);
  
  return hash;
}


#define IV_TYPE(t) ((t) + 4)

static struct lsh_string *
kex_make_key(struct hash_instance *secret,
	     uint32_t key_length,
	     int type,
	     struct lsh_string *session_id)
{
  /* Indexed by the KEX_* values */
  static const uint8_t tags[] = "CDEFAB";
  
  struct lsh_string *key;
  struct hash_instance *hash;
  struct lsh_string *digest;
  
  key = lsh_string_alloc(key_length);

  debug("\nConstructing session key of type %i\n", type);
  
  if (!key_length)
    return key;
  
  hash = hash_copy(secret);

  hash_update(hash, 1, tags + type); 
  hash_update(hash, STRING_LD(session_id));
  digest = hash_digest_string(hash);

  /* Is one digest large anough? */
  if (key_length <= HASH_SIZE(hash))
    lsh_string_write(key, 0, key_length, lsh_string_data(digest));

  else
    {
      unsigned left = key_length;
      uint32_t pos = 0;
      
      KILL(hash);
      hash = hash_copy(secret);
      
      for (;;)
	{
	  /* The n:th time we enter this loop, digest holds K_n (using
	   * the notation of section 5.2 of the ssh "transport"
	   * specification), and hash contains the hash state
	   * corresponding to
	   *
	   * H(secret | K_1 | ... | K_{n-1}) */

	  struct hash_instance *tmp;

	  assert(pos + left == key_length);
	  
	  /* Append digest to the key data. */
	  lsh_string_write_string(key, pos, digest);
	  pos += HASH_SIZE(hash);
	  left -= HASH_SIZE(hash);

	  /* And to the hash state */
	  hash_update(hash, HASH_SIZE(hash), lsh_string_data(digest));
	  lsh_string_free(digest);
	  
	  if (left <= HASH_SIZE(hash))
	    break;
	  
	  /* Get a new digest, without disturbing the hash object (as
	   * we'll need it again). We use another temporary hash for
	   * extracting the digest. */
	  
	  tmp = hash_copy(hash);
	  digest = hash_digest_string(tmp);
	  KILL(tmp);
	}

      /* Get the final digest, and use some of it for the key. */
      digest = hash_digest_string(hash);
      lsh_string_write(key, pos, left, lsh_string_data(digest));
    }

  lsh_string_free(digest);
  KILL(hash);

  debug("Expanded key: %xS", key);

  return key;
}

int
kex_make_encrypt(struct crypto_instance **c,
		 struct hash_instance *secret,
		 struct object_list *algorithms,
		 int type,
		 struct lsh_string *session_id)
{
  CAST_SUBTYPE(crypto_algorithm, algorithm, LIST(algorithms)[type]);
  
  struct lsh_string *key;
  struct lsh_string *iv = NULL;
  
  assert(LIST_LENGTH(algorithms) == KEX_LIST_LENGTH);
  
  *c = NULL;

  if (!algorithm)
    return 1;

  key = kex_make_key(secret, algorithm->key_size,
		     type, session_id);
  
  if (algorithm->iv_size)
    iv = kex_make_key(secret, algorithm->iv_size,
		      IV_TYPE(type), session_id);
  
  *c = MAKE_ENCRYPT(algorithm, lsh_string_data(key),
		    iv ? lsh_string_data(iv) : NULL);

  lsh_string_free(key);
  lsh_string_free(iv);
  
  return *c != NULL;
}

int
kex_make_decrypt(struct crypto_instance **c,
		 struct hash_instance *secret,
		 struct object_list *algorithms,
		 int type,
		 struct lsh_string *session_id)
{
  CAST_SUBTYPE(crypto_algorithm, algorithm, LIST(algorithms)[type]);

  struct lsh_string *key;
  struct lsh_string *iv = NULL;

  assert(LIST_LENGTH(algorithms) == KEX_LIST_LENGTH);

  *c = NULL;

  if (!algorithm)
    return 1;

  key = kex_make_key(secret, algorithm->key_size,
		     type, session_id);
    
  if (algorithm->iv_size)
    iv = kex_make_key(secret, algorithm->iv_size,
		      IV_TYPE(type), session_id);
  
  *c = MAKE_DECRYPT(algorithm, lsh_string_data(key),
		    iv ? lsh_string_data(iv) : NULL);

  lsh_string_free(key);
  lsh_string_free(iv);
    
  return *c != NULL;
}

struct mac_instance *
kex_make_mac(struct hash_instance *secret,
	     struct object_list *algorithms,
	     int type,
	     struct lsh_string *session_id)
{
  CAST_SUBTYPE(mac_algorithm, algorithm, LIST(algorithms)[type]);

  struct mac_instance *mac;
  struct lsh_string *key;

  assert(LIST_LENGTH(algorithms) == KEX_LIST_LENGTH);
  
  if (!algorithm)
    return NULL;

  key = kex_make_key(secret, algorithm->key_size,
		     type, session_id);

  mac = MAKE_MAC(algorithm, algorithm->key_size, lsh_string_data(key));

  lsh_string_free(key);
  return mac;
}

struct compress_instance *
kex_make_deflate(struct object_list *algorithms,
		 int type)
{
  CAST_SUBTYPE(compress_algorithm, algorithm, LIST(algorithms)[type]);
  
  return algorithm ? MAKE_DEFLATE(algorithm) : NULL;
}

struct compress_instance *
kex_make_inflate(struct object_list *algorithms,
		 int type)
{
  CAST_SUBTYPE(compress_algorithm, algorithm, LIST(algorithms)[type]);

  return algorithm ? MAKE_INFLATE(algorithm) : NULL;
}