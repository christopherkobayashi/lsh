/* handshake.c
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998, 1999, 2000 Niels M�ller
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

#include "handshake.h"

#include "command.h"
#include "compress.h"
#include "connection.h"
#include "format.h"
#include "io.h"
#include "keyexchange.h"
#include "read_line.h"
#include "read_packet.h"
#include "ssh.h"
#include "unpad.h"
#include "version.h"
#include "werror.h"
#include "xalloc.h"

#define GABA_DEFINE
#include "handshake.h.x"
#undef GABA_DEFINE

#include "handshake.c.x"

#if DATAFELLOWS_WORKAROUNDS
/* Bug compatibility information. */
struct compat_info
{
  const char *prefix;
  enum peer_flag flags;
};

static const struct compat_info
compat[] =
  {
    /* FIXME: Is there any 2.0.x without these bugs? */
    { "2.0.", (PEER_SSH_DSS_KLUDGE | PEER_SERVICE_ACCEPT_KLUDGE
	       | PEER_USERAUTH_REQUEST_KLUDGE | PEER_SEND_NO_DEBUG 
	       | PEER_X11_OPEN_KLUDGE) },
    { "2.1.0", (PEER_SSH_DSS_KLUDGE
		| PEER_USERAUTH_REQUEST_KLUDGE | PEER_SEND_NO_DEBUG) },
    { "3.0", PEER_SEND_NO_DEBUG },
    { "Sun_SSH_1.0", PEER_KEXINIT_LANGUAGE_KLUDGE },
    { NULL, 0 }
  };
    
static enum peer_flag
compat_peer_flags(uint32_t length, uint8_t *software)
{
  unsigned i;
  for (i = 0; compat[i].prefix; i++)
    {
      unsigned j;
      for (j = 0; ; j++)
	{
	  if (!compat[i].prefix[j])
	    /* Prefix matched */
	    return compat[i].flags;

	  else if (j == length || compat[i].prefix[j] != software[j])
	    /* No match */
	    break;
	}
    }
  /* Default flags are 0 */
  return 0;
}
#else /* !DATAFELLOWS_WORKAROUNDS */
#define compat_peer_flags(a,b) 0
#endif /* !DATAFELLOWS_WORKAROUNDS */

/* GABA:
   (class
     (name connection_line_handler)
     (super line_handler)
     (vars
       (connection object ssh_connection)
       ; Needed for fallback.
       (fd . int)
       (fallback object ssh1_fallback)))
*/

/* Returns -1 if the line is not the start of a SSH handshake, 0 if
 * the line appears to be an SSH handshake, but with bogus version
 * fields, or 1 if the line was parsed sucessfully. */
static int
split_version_string(uint32_t length, uint8_t *line,
		     uint32_t *protover_len, uint8_t **protover,
		     uint32_t *swver_len, uint8_t **swver,
		     uint32_t *comment_len, uint8_t **comment)
{
  uint8_t *sep;

  if (length < 4 || memcmp(line, "SSH-", 4) != 0)
    {
      /* not an ssh identification string */
      return -1;
    }
  line += 4; length -= 4;
  sep = memchr(line, '-', length);
  if (!sep)
    {
      return 0;
    }
  *protover_len = sep - line;
  *protover = line;
  
  line = sep + 1;
  length -= *protover_len + 1;

  /* FIXME: The spec is not clear about the separator here. Can there
   * be other white space than a single space character? */
  sep = memchr(line, ' ', length);
  if (!sep)
    {
      *swver_len = length;
      *swver = line;
      *comment = NULL;
      *comment_len = 0;
      return 1;
    }

  *swver_len = sep - line;
  *swver = line;
  *comment = sep + 1;
  *comment_len = length - *swver_len - 1;
  return 1;
}

static void
do_line(struct line_handler **h,
	struct read_handler **r,
	uint32_t length,
	uint8_t *line,
	struct exception_handler *e)
{
  CAST(connection_line_handler, closure, *h);
  uint32_t protover_len, swver_len, comment_len;
  uint8_t *protover, *swver, *comment;

  struct ssh_connection *connection = closure->connection;
  int mode = connection->flags & CONNECTION_MODE;
  
  switch(split_version_string(length, line, 
			      &protover_len, &protover, 
			      &swver_len, &swver,
			      &comment_len, &comment))
    {
    case 1:
      {
	/* Parse and remember format string */
	/* NOTE: According to the spec, there's no reason for the server
	 * to accept a client that wants version 1.99. But Datafellow's
	 * ssh2 client does exactly that, so we have to support it. And
	 * I don't think it causes any harm. */
	
	if ( ((protover_len >= 3) && !memcmp(protover, "2.0", 3))
	     || ((protover_len == 4) && !memcmp(protover, "1.99", 4)) )
	  {
	    struct read_handler *new;	  
#if WITH_SSH1_FALLBACK
	    if (closure->fallback)
	      {
		assert(mode == CONNECTION_SERVER);
	      
		/* Sending keyexchange packet was delayed. Do it now */
		send_kexinit(connection);
	      }
#endif /* WITH_SSH1_FALLBACK */

	    connection->peer_flags = compat_peer_flags(swver_len, swver);

	    new = make_read_packet(
		    make_packet_debug(&connection->super,
				      (connection->debug_comment
				       ? ssh_format("%lz received", connection->debug_comment)
				       : ssh_format("Received"))),
		    connection);
	    
	    connection->versions[!mode]
	      = ssh_format("%ls", length, line);

	    verbose("Client version: %pS\n"
		    "Server version: %pS\n",
		    connection->versions[CONNECTION_CLIENT],
		    connection->versions[CONNECTION_SERVER]);

	    *r = new;
	    return;
	  }
#if WITH_SSH1_FALLBACK      
      else if (closure->fallback
	       && (protover_len >= 2)
	       && !memcmp(protover, "1.", 2))
	{
	  *h = NULL;
	  SSH1_FALLBACK(closure->fallback,
			closure->fd,
			length, line,
			e);

	  return;
	}
#endif /* WITH_SSH1_FALLBACK */
      else
	{
	  werror("Unsupported protocol version: %ps\n",
		 length, line);

	  *h = NULL;

	  EXCEPTION_RAISE(connection->e,
			  make_protocol_exception
			  (SSH_DISCONNECT_PROTOCOL_VERSION_NOT_SUPPORTED,
			   NULL));
	  return;
	}
	fatal("Internal error!\n");
      }
    case 0:
      werror("Incorrectly formatted version string: %s\n", length, line);
      KILL(closure);
      *h = NULL;
      
      PROTOCOL_ERROR(connection->e,
		     "Incorrectly version string.");
      
      return;
    case -1:
      /* Display line */
      werror("%ps\n", length, line);

      /* Read next line */
      break;
    default:
      fatal("Internal error!\n");
    }
}

static struct read_handler *
make_connection_read_line(struct ssh_connection *connection, 
			  int fd,
			  struct ssh1_fallback *fallback)
{
  NEW(connection_line_handler, closure);

  closure->super.handler = do_line;
  closure->connection = connection;
  closure->fd = fd;
  closure->fallback = fallback;
  return make_read_line(&closure->super, connection->e);
}


struct handshake_info *
make_handshake_info(enum connection_flag flags,
		    const char *id_comment,
		    const char *debug_comment,
		    uint32_t block_size,
		    struct randomness *r,
		    struct alist *algorithms,
		    struct ssh1_fallback *fallback,
		    struct lsh_string *banner_text)
{
  NEW(handshake_info, self);
  self->flags = flags;
  self->id_comment = id_comment;
  self->debug_comment = debug_comment;
  self->block_size = block_size;
  self->random = r;
  self->algorithms = algorithms;
  self->fallback = fallback;
  self->banner_text = banner_text;

  return self;
}

/* (handshake handshake_info make_kexinit extra listen_value) -> connection 
 *
 * extra is passed on to KEYEXCHANGE_INIT, it is typically a set of
 * private keys (for the server) or a hostkey database (for the
 * client). */

/* Buffer size when reading from the socket */
#define BUF_SIZE (1<<14)

/* Ten minutes */
#define HANDSHAKE_TIMEOUT 600

DEFINE_COMMAND4(handshake_command)
     (struct lsh_object *a1,
      struct lsh_object *a2,
      struct lsh_object *extra,
      struct lsh_object *a4,
      struct command_continuation *c,
      struct exception_handler *e)
{
  CAST(handshake_info, info, a1);
  CAST_SUBTYPE(make_kexinit, init, a2);
  /* NOTE: For lsh, the lv from connect is mostly bogus, lv->peer is
   * NULL */
  CAST(listen_value, lv, a4);
  
  struct lsh_string *version;
  struct ssh_connection *connection;
  int mode = info->flags & CONNECTION_MODE;

  if (lv->peer)
    verbose("Initiating handshake with %S\n", lv->peer->ip);
  
  switch (mode)
    {
    case CONNECTION_CLIENT:
      version = ssh_format("SSH-%lz-%lz %lz",
			   CLIENT_PROTOCOL_VERSION,
			   SOFTWARE_CLIENT_VERSION,
			   info->id_comment);
      break;
    case CONNECTION_SERVER:
#if WITH_SSH1_FALLBACK
      if (info->fallback)
	{
	  version =
	    ssh_format("SSH-%lz-%lz %lz",
		       SSH1_SERVER_PROTOCOL_VERSION,
		       SOFTWARE_SERVER_VERSION,
		       info->id_comment);
	}
      else
#endif
	version =
	  ssh_format("SSH-%lz-%lz %lz",
		     SERVER_PROTOCOL_VERSION,
		     SOFTWARE_SERVER_VERSION,
		     info->id_comment);
      break;
    default:
      fatal("do_handshake: Internal error\n");
    }
  
  /* Installing the right exception handler is a little tricky. The
   * passed in handler is typically the top-level handler provided by
   * lsh.c or lshd.c. On top of this, we add the io_exception_handler
   * which takes care of EXC_FINISH_READ exceptions and closes the
   * connection's socket. And on top of this, we have a
   * connection_exception handler, which takes care of EXC_PROTOCOL
   * exceptions, sends a disconnect message, and then raises an
   * EXC_FINISH_READ exception. */
  
  connection = make_ssh_connection
    (info->flags,
     lv->peer, lv->local, info->debug_comment, 
     make_exc_finish_read_handler(lv->fd, e, HANDLER_CONTEXT));

  connection_after_keyexchange(connection, c);
  
  connection_init_io
    (connection, 
     &io_read_write(lv->fd,
		    make_buffered_read
		    (BUF_SIZE,
		     make_connection_read_line(connection, 
					       lv->fd->fd, info->fallback)),
		    info->block_size,
		    make_connection_close_handler(connection))
     ->write_buffer->super,
     info->random);

  /* Install timeout. */
  connection_set_timeout(connection,
			 HANDSHAKE_TIMEOUT,
			 "Handshake timed out");
  
  connection->versions[mode] = version;
  connection->kexinit = init; 
  connection->dispatch[SSH_MSG_KEXINIT]
    = make_kexinit_handler(extra, info->algorithms);

#if WITH_SSH1_FALLBACK
  /* In this mode the server SHOULD NOT send carriage return character (ascii
   * 13) after the version identification string.
   *
   * Furthermore, it should not send any data after the identification string,
   * until the client's identification string is received. */
  if (info->fallback)
    {
      A_WRITE(connection->raw,
	      ssh_format("%lS\n", version));
      return;
    }
#endif /* WITH_SSH1_FALLBACK */

  if (info->banner_text)
    {
      A_WRITE(connection->raw, 
	      ssh_format("%lS\r\n", info->banner_text));
    }

  A_WRITE(connection->raw,
	  ssh_format("%lS\r\n", version));
  
  send_kexinit(connection);
}
