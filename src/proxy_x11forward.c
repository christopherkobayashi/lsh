/* proxy_x11forward.c
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "proxy_x11forward.h"
#include "proxy_channel.h"
#include "xalloc.h"
#include "ssh.h"
#include "werror.h"
#include "channel_commands.h"
#include "format.h"

#define WINDOW_SIZE 10000

static void
do_proxy_open_x11(struct channel_open *s UNUSED,
		  struct ssh_connection *connection,
		  UINT32 type,
		  UINT32 send_max_packet,
		  struct simple_buffer *args,
		  struct command_continuation *c,
		  struct exception_handler *e)
{
  struct lsh_string *host = NULL;
  UINT32 port = 0;

  if ((host = parse_string_copy(args)) &&
#if DATAFELLOWS_WORKAROUNDS
      ((connection->peer_flags & PEER_X11_OPEN_KLUDGE) ||
       parse_uint32(args, &port)) &&
#else
      parse_uint32(args, &port) &&
#endif
      parse_eod(args))
    {
      struct proxy_channel *server
	= make_proxy_channel(WINDOW_SIZE,
			     /* FIXME: We should adapt to the other
			      * end's max packet size. Parhaps should
			      * be done by
			      * do_proxy_channel_open_continuation() ?
			      * */
			     SSH_MAX_PACKET,
			     NULL, 0);

      struct command *o;

      if (connection->chain->peer_flags & PEER_X11_OPEN_KLUDGE)
	o = make_proxy_channel_open_command(type, 
					    send_max_packet,
					    ssh_format("%S",
						       host), 
					    NULL);
      else
	/* FIXME: maybe parse the sent string to get the port value */
	o = make_proxy_channel_open_command(type, 
					    send_max_packet,
					    ssh_format("%S%i",
						       host, port), 
					    NULL);
      if (port)
	werror("x11 open request: host=%S:%i\n", host, port);
      else
	werror("datafellows compatible x11 open request: %S\n", host);
      COMMAND_CALL(o,
		   connection->chain,
		   make_proxy_channel_open_continuation(c, server),
		   e);

    }
  else
    {
      PROTOCOL_ERROR(e, "Trailing garbage in open message");
    }
  lsh_string_free(host);
}

struct channel_open *
make_proxy_open_x11(void)
{
  NEW(channel_open, self);

  self->handler = do_proxy_open_x11;
  return self;
}

