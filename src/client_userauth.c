/* client_userauth.c
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

#include "userauth.h"

#include "charset.h"
#include "command.h"
#include "format.h"
#include "parse.h"
#include "password.h"
#include "service.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

/* FIXME: For now, use only password authentication. A better method
 * would be to first send a set of publickey authentication requests
 * for the available keys (for some configurable value of
 * "available"). This is analogous to unlocking a door by first
 * examining the keys on one's keyring to see if any of them can be
 * inserted into the lock. Preferably, at this point one should use
 * spki hashed public keys rather than the public keys themselves.
 *
 * Next we wait for SSH_MSH_USERAUTH_FAILURE or SSH_MSG_USERAUTH_PK_OK
 * messages. If any of the keys is recognized, we compute a signature
 * and send it to the server (analogously to inserting the key into
 * the lock and turning it around).
 *
 * If none of the keys were recognized, or if no keys were available
 * from the start, we ask the user for a password and attempt to log
 * in using that. */

static struct packet_handler *make_banner_handler(void);

#include "client_userauth.c.x"

/* GABA:
   (class
     (name client_userauth)
     (super command)
     (vars
       (username string)            ; Remote user name to authenticate as.
       (service_name simple int)    ; Service we want to access .
  
       ; FIXME: Keys to try
       ))
*/

/* GABA:
   (class
     (name success_handler)
     (super packet_handler)
     (vars
       (c object command_continuation)))
*/

/* GABA:
   (class
     (name failure_handler)
     (super packet_handler)
     (vars
       (e object exception_handler)
       (userauth object client_userauth)))
*/

static struct lsh_string *format_userauth_password(struct lsh_string *name,
						   int service,
						   struct lsh_string *passwd,
						   int free)
{
  return ssh_format(free ? "%c%S%a%a%c%fS" : "%c%S%a%a%c%S",
		    SSH_MSG_USERAUTH_REQUEST,
		    name,
		    service,
		    ATOM_PASSWORD,
		    0,
		    passwd);
}

#define MAX_PASSWD 100

static void
send_passwd(struct client_userauth *userauth,
	    struct ssh_connection *connection)
{
  struct lsh_string *passwd
    = read_password(MAX_PASSWD,
		    ssh_format("Password for %lS: ",
			       userauth->username), 1);
  
  if (!passwd)
    {
      /* FIXME: What to do now??? */
      fatal("read_password failed!?\n");
    }
  
  C_WRITE(connection,
	  format_userauth_password(local_to_utf8(userauth->username, 0),
				   userauth->service_name,
				   local_to_utf8(passwd, 1),
				   1));
}

static void
do_userauth_success(struct packet_handler *c,
		    struct ssh_connection *connection,
		    struct lsh_string *packet)
{
  CAST(success_handler, closure, c);
  struct simple_buffer buffer;

  unsigned msg_number;
    
  simple_buffer_init(&buffer, packet->length, packet->data);

  if (parse_uint8(&buffer, &msg_number)
      && (msg_number == SSH_MSG_USERAUTH_SUCCESS)
      && parse_eod(&buffer))
    {
      werror("User authentication successful.\n");

      lsh_string_free(packet);
      
      connection->dispatch[SSH_MSG_USERAUTH_SUCCESS] = connection->fail;
      connection->dispatch[SSH_MSG_USERAUTH_FAILURE] = connection->fail;
      connection->dispatch[SSH_MSG_USERAUTH_BANNER] = connection->fail;
      
      COMMAND_RETURN(closure->c, connection);
    }
  else
    {
      lsh_string_free(packet);
      EXCEPTION_RAISE
	(connection->e,
	 make_protocol_exception(SSH_DISCONNECT_PROTOCOL_ERROR,
				 "Invalid USERAUTH_SUCCESS message"));
    }
}

/* Arbitrary limit on list length */
#define USERAUTH_MAX_METHODS 47

static void
do_userauth_failure(struct packet_handler *c,
		    struct ssh_connection *connection,
		    struct lsh_string *packet)
{
  CAST(failure_handler, closure, c);
  struct simple_buffer buffer;

  unsigned msg_number;
  struct int_list *methods = NULL;
  int partial_success;
    
  simple_buffer_init(&buffer, packet->length, packet->data);

  if (parse_uint8(&buffer, &msg_number)
      && (msg_number == SSH_MSG_USERAUTH_FAILURE)
      && ( (methods = parse_atom_list(&buffer, USERAUTH_MAX_METHODS)) )
      && parse_boolean(&buffer, &partial_success)
      && parse_eod(&buffer))
    {
      unsigned i;

      static const struct exception denied
	= STATIC_EXCEPTION(EXC_FINISH_IO, "Access denied");
      
      lsh_string_free(packet);

      if (partial_success)
	/* Doesn't help us */
	werror("Received SSH_MSH_USERAUTH_FAILURE "
	       "indicating partial success.\n");

      for(i = 0; i < LIST_LENGTH(methods); i++)
	if (LIST(methods)[i] == ATOM_PASSWORD)
	  {
	    /* Try again */
	    KILL(methods);
	    send_passwd(closure->userauth, connection);
	    return;
	  }
      /* No methods that we can use */
      KILL(methods);

      EXCEPTION_RAISE(closure->e, &denied);
    }
  else
    {
      KILL(methods);
      lsh_string_free(packet);
      EXCEPTION_RAISE
	(connection->e,
	 make_protocol_exception(SSH_DISCONNECT_PROTOCOL_ERROR,
				 "Invalud USERAUTH_FAILURE message."));
    }
}

static void
do_userauth_banner(struct packet_handler *closure,
		   struct ssh_connection *connection UNUSED,
		   struct lsh_string *packet)
{
  struct simple_buffer buffer;

  unsigned msg_number;
  UINT32 length;
  UINT8 *msg;

  UINT32 language_length;
  UINT8 *language;
  
  CHECK_TYPE(packet_handler, closure);

  simple_buffer_init(&buffer, packet->length, packet->data);

  if (parse_uint8(&buffer, &msg_number)
      && (msg_number == SSH_MSG_USERAUTH_BANNER)
      && parse_string(&buffer, &length, &msg)
      && parse_string(&buffer, &language_length, &language)
      && parse_eod(&buffer))
    {
      /* Ignore language tag */
      werror("%ups", length, msg);
    }
  else
    EXCEPTION_RAISE
      (connection->e,
       make_protocol_exception(SSH_DISCONNECT_PROTOCOL_ERROR,
			       "Invalid USERAUTH_SUCCESS message"));

  lsh_string_free(packet);
}

static struct packet_handler *
make_success_handler(struct command_continuation *c)
{
  NEW(success_handler, self);

  self->super.handler = do_userauth_success;
  self->c = c;

  return &self->super;
}

static struct packet_handler *
make_failure_handler(struct client_userauth *userauth,
		     struct exception_handler *e)
{
  NEW(failure_handler, self);

  self->super.handler = do_userauth_failure;
  self->e = e;
  self->userauth = userauth;

  return &self->super;
}

static struct packet_handler *make_banner_handler(void)
{
  NEW(packet_handler, self);

  self->handler = do_userauth_banner;
  
  return self;
}

static void
do_client_userauth(struct command *s,
		   struct lsh_object *x,
		   struct command_continuation *c,
		   struct exception_handler *e UNUSED)
{
  CAST(client_userauth, self, s);
  CAST(ssh_connection, connection, x);
  
  connection->dispatch[SSH_MSG_USERAUTH_SUCCESS]
    = make_success_handler(c);
  connection->dispatch[SSH_MSG_USERAUTH_FAILURE]
    = make_failure_handler(self, e);
  connection->dispatch[SSH_MSG_USERAUTH_BANNER]
    = make_banner_handler();

  /* Pass e on? */
  return send_passwd(self, connection);
}

struct command *make_client_userauth(struct lsh_string *username,
				     int service_name)
{
  NEW(client_userauth, self);

  self->super.call = do_client_userauth;
  self->username = username;
  self->service_name = service_name;

  return &self->super;
}
