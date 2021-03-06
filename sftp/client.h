/* client.h
 *
 * Utility functions for the client side of the protocol.
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2001 Niels Möller
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA */

#ifndef SFTP_CLIENT_H_INCLUDED
#define SFTP_CLIENT_H_INCLUDED

#include "buffer.h"

struct client_ctx
{
  struct sftp_input *i;
  struct sftp_output *o;

  /* Status from latest message. */
  uint32_t status;
};

/* Handles are strings, choosen by the server. */
struct client_handle
{
  uint32_t length;
  uint8_t *data;
};

/* Creates a file handle */
struct client_handle *
sftp_open(struct client_ctx *ctx,
	  const char *name,
	  uint32_t flags,
	  const struct sftp_attrib *a);

/* Destroys a file or directory handle */
int
sftp_close(struct client_ctx *ctx,
	   struct client_handle *handle);


#endif /* SFTP_CLIENT_H_INCLUDED */
