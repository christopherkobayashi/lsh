/* write_buffer.h
 *
 *
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

#ifndef LSH_WRITE_BUFFER_H_INCLUDED
#define LSH_WRITE_BUFFER_H_INCLUDED

#include "abstract_io.h"

/* For the packet queue */
/* NOTE: No object header */
struct buffer_node
{
  struct buffer_node *next;
  struct buffer_node *prev;
  struct lsh_string *packet;
};

struct write_buffer
{
  struct abstract_write super;
  
  UINT32 block_size;
  UINT8 *buffer; /* Size is twice the blocksize */

  int empty;

  /* If non-zero, don't accept any more data. The i/o-channel shoudl be closed
   * once the current buffers are flushed. */
  int closed; 
#if 0
  int try_write;
#endif
  
  struct buffer_node *head;
  struct buffer_node *tail;

  UINT32 pos; /* Partial packet */
  struct lsh_string *partial;

  UINT32 start;
  UINT32 end;
};

struct write_buffer *write_buffer_alloc(UINT32 size);
int write_buffer_pre_write(struct write_buffer *buffer);
void write_buffer_close(struct write_buffer *buffer);

#endif /* LSH_WRITE_BUFFER_H_INCLUDED */
