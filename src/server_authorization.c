/* server_authorization.c
 *
 * user authorization database
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1999 Balazs Scheidler
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

#include "server_authorization.h"
#include "xalloc.h"
#include "sexp.h"
#include "spki.h"
#include "format.h"
#include "server_password.h"

#include <sys/stat.h>
#include <unistd.h>

#include "server_authorization.c.x"

/* For now a key is authorized if a file named as the hash of the
   SPKI pubkey exists */

/* GABA:
   (class
     (name authorization_db)
     (super lookup_verifier)
     (vars
       (signalgo object signature_algorithm)
       (hashalgo object hash_algorithm)))
*/

static struct verifier *do_key_lookup(struct lookup_verifier *c,
				      struct lsh_string *keyholder,
				      struct lsh_string *key)
{
  CAST(authorization_db, closure, c);
  struct sexp *pubkey_spki;
  struct lsh_string *pubkey_spki_blob, *filename;
  UINT8 *pubkey_spki_hash, *hash_hex;
  struct hash_instance *h;
  struct stat st;
  UINT32 i;
  struct unix_user *user;

  user = lookup_user(keyholder, 0);
  if (!user)
    return NULL;

  pubkey_spki = keyblob2spki(key);
  if (!pubkey_spki)
    return NULL;

  pubkey_spki_blob = sexp_format(pubkey_spki, SEXP_CANONICAL, 0);
  /* FIXME: spki acl reading should go here */
  KILL(pubkey_spki);

  h = MAKE_HASH(closure->hashalgo);
  pubkey_spki_hash = alloca(closure->hashalgo->hash_size);
  HASH_UPDATE(h, pubkey_spki_blob->length, pubkey_spki_blob->data);
  HASH_DIGEST(h, pubkey_spki_hash);
  KILL(h);
  lsh_string_free(pubkey_spki_blob);
  
  filename = ssh_format("%lS/.lsh/authorization/%lr%c", 
			user->home,
			closure->hashalgo->hash_size * 2, &hash_hex,
			0);
  for (i = 0; i < closure->hashalgo->hash_size; i++)
    {
      UINT8 hexchars[] = "0123456789abcdef";
      hash_hex[i * 2] = hexchars[(pubkey_spki_hash[i] & 0xf0) >> 4];
      hash_hex[i * 2 + 1] = hexchars[(pubkey_spki_hash[i] & 0x0f)];
    }
  if (stat(filename->data, &st) == 0)
    {
      lsh_string_free(filename);
      /* FIXME: maybe MAKE_VERIFIER() should get the key in SPKI form */
      return MAKE_VERIFIER(closure->signalgo, key->length, key->data);
    }
  lsh_string_free(filename);
  return NULL;
}

struct lookup_verifier *
make_authorization_db(struct signature_algorithm *s,
		      struct hash_algorithm *h)
{
  NEW(authorization_db, res);

  res->super.lookup = do_key_lookup;
  res->signalgo = s;
  res->hashalgo = h;

  return &res->super;
}
