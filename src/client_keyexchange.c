/* client_keyexchange.c
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

#include "client_keyexchange.h"

#include "atoms.h"
#include "command.h"
#include "debug.h"
#include "format.h"
#include "lookup_verifier.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#include <assert.h>

#include "client_keyexchange.c.x"

/* GABA:
   (class
     (name dh_client_exchange)
     (super keyexchange_algorithm)
     (vars
       (dh object diffie_hellman_method)))
       ; alist of signature-algorithm -> lookup_verifier
       ;; (verifiers object alist)))
*/

/* Handler for the kex_dh_reply message */
/* GABA:
   (class
     (name dh_client)
     (super packet_handler)
     (vars
       ;; (hostname string)
       (dh struct diffie_hellman_instance)
       (hostkey_algorithm . UINT32)
       (verifier object lookup_verifier)
       (install object install_keys)))
*/
    
static void
do_handle_dh_reply(struct packet_handler *c,
		   struct ssh_connection *connection,
		   struct lsh_string *packet)
{
  CAST(dh_client, closure, c);
  struct verifier *v;
  struct hash_instance *hash;
  
  trace("handle_dh_reply()\n");
  
  if (!dh_process_server_msg(&closure->dh, packet))
    {
      disconnect_kex_failed(connection, "Bad dh-reply\r\n");
      return;
    }
    
  v = LOOKUP_VERIFIER(closure->verifier, closure->hostkey_algorithm,
		      NULL, closure->dh.server_key);

  if (!v)
    {
      /* FIXME: Use a more appropriate error code? */
      disconnect_kex_failed(connection, "Bad server host key\r\n");
      return;
    }

#if DATAFELLOWS_WORKAROUNDS
  if (closure->hostkey_algorithm == ATOM_SSH_DSS && 
      (connection->peer_flags & PEER_SSH_DSS_KLUDGE))
    {
      v = make_dsa_verifier_kludge(v);
    }
#endif

  
  if (!dh_verify_server_msg(&closure->dh, v))
    {
      /* FIXME: Use a more appropriate error code? */
      disconnect_kex_failed(connection, "Invalid server signature\r\n");
      return;
    }
  
  /* Key exchange successful! Send a newkeys message, and install a
   * handler for receiving the newkeys message. */

  C_WRITE(connection, ssh_format("%c", SSH_MSG_NEWKEYS));

  /* FIXME: Perhaps more of this key handling could be abstracted
   * away, instead of duplicating it in client_keyexchange.c and
   * server_keyexchange.c. */

  /* A hash instance initialized with the key, to be used for key
   * generation */
  hash = kex_build_secret(closure->dh.method->H,
			  closure->dh.exchange_hash,
			  closure->dh.K);
  
  /* Record session id */
  if (!connection->session_id)
    {
      connection->session_id = closure->dh.exchange_hash;
      closure->dh.exchange_hash = NULL; /* For gc */
    }
  
  if (!INSTALL_KEYS(closure->install, connection, hash))
    {
      werror("Installing new keys failed. Hanging up.\n");
      KILL(hash);

      PROTOCOL_ERROR(connection->e, "Refusing to use weak key.");

      return;
    }

  KILL(hash);

  connection->dispatch[SSH_MSG_KEXDH_REPLY] = connection->fail;
  connection->kex_state = KEX_STATE_NEWKEYS;

#if DATAFELLOWS_WORKAROUNDS
  if (! (connection->peer_flags & PEER_SEND_NO_DEBUG))
#endif
    send_verbose(connection->write, "Key exchange successful!", 0);
  
  if (connection->established)
    {
      struct command_continuation *c = connection->established;
      connection->established = NULL;
  
      COMMAND_RETURN(c, connection);
    }
}

static void
do_init_client_dh(struct keyexchange_algorithm *c,
		  struct ssh_connection *connection,
		  int hostkey_algorithm_atom,
		  struct lsh_object *extra,
		  struct object_list *algorithms)
{
  CAST(dh_client_exchange, closure, c);
  CAST_SUBTYPE(lookup_verifier, verifier, extra);
  
  NEW(dh_client, dh);

  CHECK_SUBTYPE(ssh_connection, connection);

  assert(verifier);
  
  /* Initialize */
  dh->super.handler = do_handle_dh_reply;
  init_diffie_hellman_instance(closure->dh, &dh->dh, connection);

  dh->verifier = verifier;
  dh->hostkey_algorithm = hostkey_algorithm_atom;

  dh->install = make_install_new_keys(0, algorithms);
  
  /* Send client's message */
  C_WRITE(connection, dh_make_client_msg(&dh->dh));

  /* Install handler */
  connection->dispatch[SSH_MSG_KEXDH_REPLY] = &dh->super;
  
  connection->kex_state = KEX_STATE_IN_PROGRESS;
}


struct keyexchange_algorithm *
make_dh_client(struct diffie_hellman_method *dh)
     /* struct alist *verifiers) */
{
  NEW(dh_client_exchange, self);

  CHECK_TYPE(diffie_hellman_method, dh);
  
  self->super.init = do_init_client_dh;
  self->dh = dh;
  /* self->verifiers = verifiers; */
  
  return &self->super;
}

