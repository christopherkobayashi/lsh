/* keyexchange.c
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "keyexchange.h"

#include "abstract_io.h"
/* For filter_algorithms */
#include "algorithms.h"
#include "alist.h"
#include "command.h"
#include "connection.h"
#include "format.h"
#include "io.h"
#include "lsh_string.h"
#include "parse.h"
#include "publickey_crypto.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#define GABA_DEFINE
#include "keyexchange.h.x"
#undef GABA_DEFINE

#include "keyexchange.c.x"

/* Define this to get very frequent re-exchanges */
#ifndef STRESS_KEYEXCHANGE
# define STRESS_KEYEXCHANGE 0
#endif

/* GABA:
   (class
     (name kexinit_handler)
     (super packet_handler)
     (vars
       ; Extra argument for the KEYEXCHANGE_INIT call.
       (extra object lsh_object)
       
       ; Maps names to algorithms. It's dangerous to lookup random atoms
       ; in this table, as not all objects have the same type. This
       ; mapping is used only on atoms that have appeared in *both* the
       ; client's and the server's list of algorithms (of a certain
       ; type), and therefore the remote side can't screw things up.
       (algorithms object alist)))
*/

#define NLISTS 10

/* Arbitrary limit on list length */
/* An SSH-2.0-Sun_SSH_1.0 server has been reported to list 250
 * different algorithms, or in fact a list of installed locales. */
#define KEXINIT_MAX_ALGORITMS 47
#define KEXINIT_MAX_ALGORITMS_SUN 300

static struct kexinit *
parse_kexinit(struct lsh_string *packet, enum peer_flag peer_flags)
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
    {
      unsigned limit = KEXINIT_MAX_ALGORITMS;
#if DATAFELLOWS_WORKAROUNDS
      if ( (peer_flags & PEER_KEXINIT_LANGUAGE_KLUDGE)
	   && i >= 8)
	limit = KEXINIT_MAX_ALGORITMS_SUN;
#endif /* DATAFELLOWS_WORKAROUNDS */
      if ( !(lists[i] = parse_atom_list(&buffer, limit)))
	break;
    }

  if ( (i<NLISTS)
       || !parse_boolean(&buffer, &res->first_kex_packet_follows)
       || !parse_uint32(&buffer, &reserved)
       || reserved || !parse_eod(&buffer) )
    {
      /* Bad format */
      int j;
      for (j = 0; j<i; j++)
	KILL(lists[i]);
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
format_kex(struct kexinit *kex)
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
  
void
send_kexinit(struct ssh_connection *connection)
{
  struct lsh_string *s;
  int mode = connection->flags & CONNECTION_MODE;

  struct kexinit *kex
    = connection->kexinits[mode]
    = MAKE_KEXINIT(connection->kexinit);
  
  assert(kex->first_kex_packet_follows == !!kex->first_kex_packet);
  assert(connection->read_kex_state == KEX_STATE_INIT);

  /* First, disable any key reexchange timer */
  if (connection->key_expire)
    {
      KILL_RESOURCE(connection->key_expire);
      connection->key_expire = NULL;
    }
  
  s = format_kex(kex);

  /* Save value for later signing */
#if 0
  debug("send_kexinit: Storing literal_kexinits[%i]\n", mode);
#endif
  
  connection->literal_kexinits[mode] = s; 
  connection_send_kex_start(connection);  

  connection_send_kex(connection, lsh_string_dup(s));

  /* NOTE: This feature isn't fully implemented, as we won't tell
   * the selected key exchange method if the guess was "right". */
  if (kex->first_kex_packet_follows)
    {
      s = kex->first_kex_packet;
      kex->first_kex_packet = NULL;

      connection_send_kex(connection, s);
    }
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

void
disconnect_kex_failed(struct ssh_connection *connection, const char *msg)
{
  EXCEPTION_RAISE
    (connection->e,
     make_protocol_exception(SSH_DISCONNECT_KEY_EXCHANGE_FAILED,
			     msg));
}

static void
do_handle_kexinit(struct packet_handler *c,
		  struct ssh_connection *connection,
		  struct lsh_string *packet)
{
  CAST(kexinit_handler, closure, c);
  
  int kex_algorithm_atom;
  int hostkey_algorithm_atom;

  int parameters[KEX_PARAMETERS];
  struct object_list *algorithms;

  int mode = connection->flags & CONNECTION_MODE;
  struct kexinit *msg = parse_kexinit(packet, connection->peer_flags);

  int i;

  verbose("Received KEXINIT message. Key exchange initated.\n");
  
  if (connection->read_kex_state != KEX_STATE_INIT)
    {
      PROTOCOL_ERROR(connection->e, "Unexpected KEXINIT message.");
      return;
    }      
  
  if (!msg)
    {
      disconnect_kex_failed(connection, "Invalid KEXINIT message.");
      return;
    }

  if (!LIST_LENGTH(msg->kex_algorithms))
    {
      disconnect_kex_failed(connection, "No keyexchange method.");
      return;
    }
    
    
  /* Save value for later signing */
#if 0
  debug("do_handle_kexinit: Storing literal_kexinits[%i]\n", !mode);
#endif
  connection->literal_kexinits[!mode] = lsh_string_dup(packet);
  
  connection->kexinits[!mode] = msg;
  
  /* Have we sent a kexinit message already? */
  if (!connection->kexinits[mode])
    send_kexinit(connection);

  /* Select key exchange algorithms */

  /* FIXME: Look at the hostkey algorithm as well. */
  if (LIST(connection->kexinits[CONNECTION_CLIENT]->kex_algorithms)[0]
      == LIST(connection->kexinits[CONNECTION_SERVER]->kex_algorithms)[0])
    {
      /* Use this algorithm */
      kex_algorithm_atom
	= LIST(connection->kexinits[CONNECTION_CLIENT]->kex_algorithms)[0];

      connection->read_kex_state = KEX_STATE_IN_PROGRESS;
    }
  else
    {
      if (msg->first_kex_packet_follows)
	{
	  /* Wrong guess */
	  connection->read_kex_state = KEX_STATE_IGNORE;
	}

      /* FIXME: Ignores that some keyexchange algorithms require
       * certain features of the host key algorithms. */
      
      kex_algorithm_atom
	= select_algorithm(connection->kexinits[CONNECTION_CLIENT]->kex_algorithms,
			   connection->kexinits[CONNECTION_SERVER]->kex_algorithms);

      /* FIXME: This is actually ok for SRP. */
      if  (!kex_algorithm_atom)
	{
	  disconnect_kex_failed(connection,
				"No common key exchange method.\r\n");
	  return;
	}
    }
  
  hostkey_algorithm_atom
    = select_algorithm(connection->kexinits[CONNECTION_CLIENT]->server_hostkey_algorithms,
		       connection->kexinits[CONNECTION_SERVER]->server_hostkey_algorithms);

  if (!hostkey_algorithm_atom)
    {
      disconnect_kex_failed(connection, "No common hostkey algorithm.\r\n");
      return;
    }

  verbose("Selected keyexchange algorithm: %a\n"
	  "  with hostkey algorithm:       %a\n",
	  kex_algorithm_atom, hostkey_algorithm_atom);
    
  for(i = 0; i<KEX_PARAMETERS; i++)
    {
      parameters[i]
	= select_algorithm(connection->kexinits[CONNECTION_CLIENT]->parameters[i],
			   connection->kexinits[CONNECTION_SERVER]->parameters[i]);
      
      if (!parameters[i])
	{
	  disconnect_kex_failed(connection, "Algorithm negotiation failed.");
	  return;
	}
    }

  verbose("Selected bulk algorithms: (client to server, server to client)\n"
	  "  Encryption:             (%a, %a)\n"
	  "  Message authentication: (%a, %a)\n"
	  "  Compression:            (%a, %a)\n",
	  parameters[0], parameters[1],
	  parameters[2], parameters[3], 
	  parameters[4], parameters[5]);
  
  algorithms = alloc_object_list(KEX_PARAMETERS);
  
  for (i = 0; i<KEX_PARAMETERS; i++)
    LIST(algorithms)[i] = ALIST_GET(closure->algorithms, parameters[i]);

  {
    CAST_SUBTYPE(keyexchange_algorithm, kex_algorithm,
		 ALIST_GET(closure->algorithms, kex_algorithm_atom));

    KEYEXCHANGE_INIT( kex_algorithm,
		      connection,
		      hostkey_algorithm_atom,
		      closure->extra, /* hostkey_algorithm, */
		      algorithms);
  }
}

struct packet_handler *
make_kexinit_handler(struct lsh_object *extra,
		     struct alist *algorithms)
{
  NEW(kexinit_handler, self);

  self->super.handler = do_handle_kexinit;

  self->extra = extra;
  self->algorithms = algorithms;
  
  return &self->super;
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
  
  assert(LIST_LENGTH(algorithms) == KEX_PARAMETERS);
  
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

  assert(LIST_LENGTH(algorithms) == KEX_PARAMETERS);

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

  assert(LIST_LENGTH(algorithms) == KEX_PARAMETERS);
  
  if (!algorithm)
    return NULL;

  key = kex_make_key(secret, algorithm->key_size,
		     type, session_id);

  mac = MAKE_MAC(algorithm, algorithm->key_size, lsh_string_data(key));

  lsh_string_free(key);
  return mac;
}

static struct compress_instance *
kex_make_deflate(struct object_list *algorithms,
		 int type)
{
  CAST_SUBTYPE(compress_algorithm, algorithm, LIST(algorithms)[type]);
  
  return algorithm ? MAKE_DEFLATE(algorithm) : NULL;
}

static struct compress_instance *
kex_make_inflate(struct object_list *algorithms,
		 int type)
{
  CAST_SUBTYPE(compress_algorithm, algorithm, LIST(algorithms)[type]);

  return algorithm ? MAKE_INFLATE(algorithm) : NULL;
}

/* GABA:
   (class
     (name reexchange_timeout)
     (super lsh_callback)
     (vars
       (connection object ssh_connection)))
*/

static void
do_reexchange_timeout(struct lsh_callback *s)
{
  CAST(reexchange_timeout, self, s);
  assert(!self->connection->send_kex_only);

  verbose("Session key expired. Initiating key re-exchange.\n");
  send_kexinit(self->connection);
}

static void
set_reexchange_timeout(struct ssh_connection *connection,
		       unsigned seconds)
{
  NEW(reexchange_timeout, timeout);

  verbose("Setting session key lifetime to %i seconds\n",
	  seconds);
  timeout->super.f = do_reexchange_timeout;
  timeout->connection = connection;

  assert(!connection->key_expire);
  connection->key_expire = io_callout(&timeout->super,
				      seconds);
  
  remember_resource(connection->resources, connection->key_expire); 
}

/* GABA:
   (class
     (name newkeys_handler)
     (super packet_handler)
     (vars
       (crypto object crypto_instance)
       (mac object mac_instance)
       (compression object compress_instance)))
*/

/* Maximum lifetime for the session keys. Use longer timeout on
 * the server side. */

#if STRESS_KEYEXCHANGE
# define SESSION_KEY_LIFETIME_CLIENT 4
# define SESSION_KEY_LIFETIME_SERVER 14
#else
/* 40 minutes */
# define SESSION_KEY_LIFETIME_CLIENT 2400
/* 90 minutes */
# define SESSION_KEY_LIFETIME_SERVER 5400
#endif

static void
do_handle_newkeys(struct packet_handler *c,
		  struct ssh_connection *connection,
		  struct lsh_string *packet)
{
  CAST(newkeys_handler, closure, c);
  struct simple_buffer buffer;
  unsigned msg_number;

  simple_buffer_init(&buffer, STRING_LD(packet));

  verbose("Received NEWKEYS. Key exchange finished.\n");
  
  if (parse_uint8(&buffer, &msg_number)
      && (msg_number == SSH_MSG_NEWKEYS)
      && (parse_eod(&buffer)))
    {
      connection->rec_crypto = closure->crypto;
      connection->rec_mac = closure->mac;
      connection->rec_compress = closure->compression;

      connection->read_kex_state = KEX_STATE_INIT;

      connection->kexinits[CONNECTION_CLIENT]
	= connection->kexinits[CONNECTION_SERVER] = NULL;

      lsh_string_free(connection->literal_kexinits[CONNECTION_CLIENT]);
      lsh_string_free(connection->literal_kexinits[CONNECTION_SERVER]);
      
      connection->literal_kexinits[CONNECTION_CLIENT]
	= connection->literal_kexinits[CONNECTION_SERVER] = NULL;

      /* Normally, packet entries in the dispatch table must never be
       * NULL, but SSH_MSG_NEWKEYS is handled specially by
       * connection.c:connection_handle_packet. So we could use NULL
       * here, but for uniformity we don't do that. */
      
      connection->dispatch[SSH_MSG_NEWKEYS] = &connection_fail_handler;

      /* Set maximum lifetime for the session keys. Use longer timeout on
       * the server side. */
      
      set_reexchange_timeout
	(connection,
	 ((connection->flags & CONNECTION_MODE) == CONNECTION_SERVER)
	 ? SESSION_KEY_LIFETIME_SERVER : SESSION_KEY_LIFETIME_CLIENT);
      KILL(closure);
    }
  else
    PROTOCOL_ERROR(connection->e, "Invalid NEWKEYS message");
}

struct packet_handler *
make_newkeys_handler(struct crypto_instance *crypto,
		     struct mac_instance *mac,
		     struct compress_instance *compression)
{
  NEW(newkeys_handler,self);

  self->super.handler = do_handle_newkeys;
  self->crypto = crypto;
  self->mac = mac;
  self->compression = compression;

  return &self->super;
}

/* Uses the same algorithms for both directions */
/* GABA:
   (class
     (name simple_kexinit)
     (super make_kexinit)
     (vars
       (r object randomness)
       (kex_algorithms object int_list)
       (hostkey_algorithms object int_list)
       (crypto_algorithms object int_list)
       (mac_algorithms object int_list)
       (compression_algorithms object int_list)
       (languages object int_list)))
*/

static struct kexinit *
do_make_simple_kexinit(struct make_kexinit *c)
{
  CAST(simple_kexinit, closure, c);
  NEW(kexinit, kex);

  RANDOM(closure->r, 16, kex->cookie);

  kex->kex_algorithms = closure->kex_algorithms;
  kex->server_hostkey_algorithms = closure->hostkey_algorithms;
  kex->parameters[KEX_ENCRYPTION_CLIENT_TO_SERVER]
    = closure->crypto_algorithms;
  kex->parameters[KEX_ENCRYPTION_SERVER_TO_CLIENT]
    = closure->crypto_algorithms;
  kex->parameters[KEX_MAC_CLIENT_TO_SERVER] = closure->mac_algorithms;
  kex->parameters[KEX_MAC_SERVER_TO_CLIENT] = closure->mac_algorithms;
  kex->parameters[KEX_COMPRESSION_CLIENT_TO_SERVER]
    = closure->compression_algorithms;
  kex->parameters[KEX_COMPRESSION_SERVER_TO_CLIENT]
    = closure->compression_algorithms;
  kex->languages_client_to_server = closure->languages;
  kex->languages_server_to_client = closure->languages;
  kex->first_kex_packet_follows = 0;

  kex->first_kex_packet = NULL;

  return kex;
}

struct make_kexinit *
make_simple_kexinit(struct randomness *r,
		    struct int_list *kex_algorithms,
		    struct int_list *hostkey_algorithms,
		    struct int_list *crypto_algorithms,
		    struct int_list *mac_algorithms,
		    struct int_list *compression_algorithms,
		    struct int_list *languages)
{
  NEW(simple_kexinit, res);

  assert(r->quality == RANDOM_GOOD);
  
  res->super.make = do_make_simple_kexinit;
  res->r = r;
  res->kex_algorithms = kex_algorithms;
  res->hostkey_algorithms = hostkey_algorithms;
  res->crypto_algorithms = crypto_algorithms;
  res->mac_algorithms = mac_algorithms;
  res->compression_algorithms = compression_algorithms;
  res->languages = languages;

  return &res->super;
}

static int
install_keys(struct object_list *algorithms,
	     struct ssh_connection *connection,
	     struct hash_instance *secret)
{
  struct crypto_instance *rec;
  struct crypto_instance *send;
  int is_server = connection->flags & CONNECTION_SERVER;

  assert(LIST_LENGTH(algorithms) == KEX_PARAMETERS);

  if (!kex_make_decrypt(&rec, secret, algorithms,
			 KEX_ENCRYPTION_SERVER_TO_CLIENT ^ is_server,
			connection->session_id))
    /* Weak or invalid key */
    return 0;

  if (!kex_make_encrypt(&send, secret, algorithms,
			KEX_ENCRYPTION_CLIENT_TO_SERVER ^ is_server,
			connection->session_id))
    {
      KILL(rec);
      return 0;
    }
  
  /* Keys for receiving */
  connection->dispatch[SSH_MSG_NEWKEYS] = make_newkeys_handler
    (rec,
     kex_make_mac(secret, algorithms,
		  KEX_MAC_SERVER_TO_CLIENT ^ is_server,
		  connection->session_id),
     kex_make_inflate(algorithms,
		      KEX_COMPRESSION_SERVER_TO_CLIENT ^ is_server));

  /* Keys for sending */
  /* NOTE: The NEWKEYS-message should have been sent before this
   * is done. */
  connection->send_crypto = send;
  
  connection->send_mac 
    = kex_make_mac(secret, algorithms,
		   KEX_MAC_CLIENT_TO_SERVER ^ is_server,
		   connection->session_id);

  connection->send_compress
    = kex_make_deflate(algorithms,
		       KEX_COMPRESSION_CLIENT_TO_SERVER ^ is_server);
  
  return 1;
}


/* Returns a hash instance for generating various session keys. NOTE:
 * This mechanism changed in the transport-05 draft. Before this, the
 * exchange hash was not included at this point. */
static struct hash_instance *
kex_build_secret(const struct hash_algorithm *H,
		 struct lsh_string *exchange_hash,
		 struct lsh_string *K)
{
  /* We include a length field for the key, but not for the exchange
   * hash. */
  
  struct hash_instance *hash = make_hash(H);
  struct lsh_string *s = ssh_format("%S%lS", K, exchange_hash);

  hash_update(hash, STRING_LD(s));
  lsh_string_free(s);
  
  return hash;
}


/* NOTE: Consumes both the exchange_hash and K */
void
keyexchange_finish(struct ssh_connection *connection,
		   struct object_list *algorithms,
		   const struct hash_algorithm *H,
		   struct lsh_string *exchange_hash,
		   struct lsh_string *K)
{
  struct hash_instance *hash;
  
  /* Send a newkeys message, and install a handler for receiving the
   * newkeys message. */

  assert(connection->send_kex_only);
  connection_send_kex(connection, ssh_format("%c", SSH_MSG_NEWKEYS));
  
  /* A hash instance initialized with the key, to be used for key
   * generation */
  hash = kex_build_secret(H, exchange_hash, K);
  lsh_string_free(K);
  
  /* Record session id */
  if (!connection->session_id)
    connection->session_id = exchange_hash;
  else
    lsh_string_free(exchange_hash);
  
  if (!install_keys(algorithms, connection, hash))
    {
      werror("Installing new keys failed. Hanging up.\n");
      KILL(hash);

      PROTOCOL_ERROR(connection->e, "Refusing to use weak key.");

      return;
    }

  KILL(hash);

  /* If any messages were queued during the key exchange, send them
   * now. */
  connection_send_kex_end(connection);
  
  connection->read_kex_state = KEX_STATE_NEWKEYS;
}
