/* channel_commands.c
 *
 * $Id$
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

#include "channel_commands.h"

#define GABA_DEFINE
#include "channel_commands.h.x"
#undef GABA_DEFINE

int do_channel_open_command(struct command *s,
			    struct lsh_object *x,
			    struct command_continuation *c)
{
  CAST_SUBTYPE(channel_open_command, self, s);
  CAST_SUBTYPE(ssh_connection, connection, x);
  struct lsh_string *request;
  struct ssh_channel *channel = NEW_CHANNEL(self, connection, &request);

  if (!channel)
    {
      /* Probably, we have run out of channel numbers. */
      werror("do_channel_open_command: NEW_CHANNEL failed\n");
      return COMMAND_RETURN(c, NULL);
    }

  channel->open_continuation = c;
  
  return A_WRITE(connection->write, request);
}
