/* read_line.h
 *
 * Read-handler processing a line at a time.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef  LSH_READ_HANDLER_H_INCLUDED
#define  LSH_READ_HANDLER_H_INCLUDED

#include "abstract_io.h"

/* This limit follows the ssh specification */
#define MAX_LINE 255

/* May store a new handler into *h. */
struct line_handler
{
  struct lsh_object header;
  
  struct read_handler * (*handler)(struct line_handler **h,
				   UINT32 length,
				   UINT8 *line);
};

#define PROCESS_LINE(h, length, line) \
((h)->handler(&(h), (length), (line)))

struct read_line
{
  struct read_handler super; /* Super type */
  struct line_handler *handler;

  UINT32 pos;   /* Line buffer */
  UINT8 buffer[MAX_LINE];
};

struct read_handler *make_read_line(struct line_handler *handler);

#endif /* LSH_READ_HANDLER_H_INCLUDED */
