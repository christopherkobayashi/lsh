/* rsync.h
 *
 * $Id$
 */

/* 
   Copyright (C) Andrew Tridgell 1996
   Copyright (C) Paul Mackerras 1996
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* Hacked by Niels M�ller */
#ifndef RSYNC_H_INCLUDED
#define RSYNC_H_INCLUDED

#if LSH
# include "lsh_types.h"
#else
# if HAVE_CONFIG_H
#  include "config.h"
# endif
#endif

#include "md5.h"

#include <stdlib.h>

/* FIXME: replace with proper autoconf check */
#define OFF_T size_t

#define RSYNC_SUM_LENGTH MD5_DIGESTSIZE

/* Size of block count, block size, tail */
#define RSYNC_HEADER_SIZE 12

/* Size of weak sum, md5 sume */
#define RSYNC_ENTRY_SIZE 20

/* Initial checksum calculations (by the receiver) */

/* NOTE: Unlike zlib, we want to know the file size before we start.
 * This could be relxed, but requires some modifications to the
 * protocol. */
struct rsync_generate_state
{
  /* Public fields */
  UINT8 *next_in;
  UINT32 avail_in;
  UINT8 *next_out;
  UINT32 avail_out;

  UINT32 block_size;
  UINT32 total_length;
  UINT32 offset; /* Current offset in input file. */

  /* Weak check sum */
  unsigned a_sum;
  unsigned c_sum;
  
  struct md5_ctx block_sum;

  /* Internal state */
  UINT8 buf[RSYNC_ENTRY_SIZE];
  UINT8 buf_length; /* Zero means no buffered data. */
  UINT8 buf_pos;

  UINT32 left; /* Amount left of current block */
};

/* Return values */
/* Things are working fine */
#define RSYNC_PROGRESS    0
/* All data is flushed to the output */
#define RSYNC_DONE        1
/* No progress possible */
#define RSYNC_BUF_ERROR   2
/* Invalid input */
#define RSYNC_INPUT_ERROR 3
/* Out of memory (can happen only for rsync_read_table and rsync_send_init) */
#define RSYNC_MEMORY 4

int rsync_generate(struct rsync_generate_state *state);
int rsync_generate_init(struct rsync_generate_state *state,
			UINT32 block_size,
			UINT32 size);


/* Receiving a file. */

/* The receiver calls this function to copy at most LENGTH octets of
 * local data to the output buffer.
 *
 * OPAQUE is state private to the lookup function. DST and LENGTH give
 * the location of the destination buffer. INDEX is the block to read,
 * and OFFSET is a location within that block.
 *
 * The function should return
 *
 * -1 on failure (and it has to check INDEX and OFFSET for validity).
 * 0 if copying succeeds, but not all of the block was copied.
 * 1 if copying succeeds, and the final octet of the data swas copied.
 *
 * On success, the function should set *DONE to the amount of data copied.
 */

typedef int (*rsync_lookup_read_t)(void *opaque,
				   UINT8 *dst, UINT32 length,
				   UINT32 index, UINT32 offset, UINT32 *done);

struct rsync_receive_state
{
  /* Public fields */
  UINT8 *next_in;
  UINT32 avail_in;
  UINT8 *next_out;
  UINT32 avail_out;

  UINT32 block_size;
  /* UINT32 offset; */ /* Current offset in output file. */

  rsync_lookup_read_t lookup;
  void *opaque;
  
  struct md5_ctx full_sum; /* Sum of all input data */

  /* Private state */

  int state;
  
  UINT32 token; 
  UINT32 i;

  UINT8 buf[MD5_DIGESTSIZE];
};

int rsync_receive(struct rsync_receive_state *state);
void rsync_receive_init(struct rsync_receive_state *state);

/* Sending files */

struct rsync_table;
struct rsync_node;

struct rsync_read_table_state
{
  /* Public fields */
  struct rsync_table *table;

  UINT32 count; /* Block count */
  UINT32 block_size;
  UINT32 remainder;

  /* Private state */
  UINT8 buf[RSYNC_ENTRY_SIZE];
  unsigned pos;
};

int
rsync_read_table(struct rsync_read_table_state *state,
		 UINT32 length, UINT8 *input);

/* For reading the list of checksums. */
struct rsync_send_state
{
  /* Public fields */
  UINT8 *next_in;
  UINT32 avail_in;
  UINT8 *next_out;
  UINT32 avail_out;

  /* Limits */
  UINT32 max_count;
  UINT32 max_block_size;
  
  struct rsync_table *table;

  /* Internal state */
  int state;
  
  UINT32 buf_size;
  UINT8 *buf;
  UINT32 pos;
  
  unsigned sum_a;
  unsigned sum_b;
};

int rsync_send_init(struct rsync_send_state *state,
		    struct rsync_table *table);
		     
int rsync_send(struct rsync_send_state *state, int flush);

void rsync_send_free(struct rsync_send_state *state); 

void
rsync_update_1(unsigned *ap, unsigned *cp,
	       UINT32 length, UINT8 *data);

struct rsync_node *
rsync_search(unsigned *ap, unsigned *bp, unsigned block_size,
	     UINT32 length, UINT8 *start, UINT8 *end,
	     UINT32 *found, struct rsync_node **hash);

#endif /* RSYNC_H_INCLUDED */
