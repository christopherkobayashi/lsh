/* server_authorization.c
 *
 * user authorization database
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1999 Bal�zs Scheidler
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

#include "format.h"
#include "server_userauth.h"
#include "sexp.h"
#include "spki.h"
#include "xalloc.h"

#include <assert.h>

#include "server_authorization.c.x"

/* For now a key is authorized if a file named as the hash of the
   SPKI pubkey exists */

/* GABA:
   (class
     (name authorization_db)
     (super lookup_verifier)
     (vars
       (index_name string)
       ;; (signalgo object signature_algorithm)
       (hashalgo object hash_algorithm)))
*/

static struct verifier *
do_key_lookup(struct lookup_verifier *c,
	      int method,
	      struct lsh_user *keyholder,
	      struct lsh_string *key)
{
  CAST(authorization_db, closure, c);
  struct lsh_string *filename;

  struct dsa_verifier *v;

  assert(keyholder);
  
  if (method != ATOM_SSH_DSS)
    return NULL;

  /* FIXME: Perhaps this is the right place to choose to apply the
   * PEER_SSH_DSS_KLUDGE? */
  
  v = make_ssh_dss_verifier(key->length, key->data);

  if (!v)
    return NULL;
  
  /* FIXME: Proper spki acl reading should go here. */
  
  filename = ssh_cformat(".lsh/%lS/%lxfS", 
			 closure->index_name,
			 hash_string(closure->hashalgo,
				     sexp_format(dsa_to_spki_public_key(&v->public),
						 SEXP_CANONICAL, 0),
				     1));
  
  if (USER_FILE_EXISTS(keyholder, filename, 1))
    return &v->super;

  return NULL;
}

struct lookup_verifier *
make_authorization_db(struct lsh_string *index_name, 
		      /* struct signature_algorithm *s, */
		      struct hash_algorithm *h)
{
  NEW(authorization_db, res);

  res->super.lookup = do_key_lookup;
  res->index_name = index_name;
  /* res->signalgo = s; */
  res->hashalgo = h;

  return &res->super;
}
