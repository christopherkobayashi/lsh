/* base64.h
 *
 * "ASCII armor" codecs.
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2002 Niels M�ller, Dan Egnor
 *  
 * The nettle library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The nettle library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the nettle library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */
 
#ifndef NETTLE_BASE64_H_INCLUDED
#define NETTLE_BASE64_H_INCLUDED

#include <inttypes.h>

/* Base64 encoding */

#define BASE64_BINARY_BLOCK_SIZE 3
#define BASE64_TEXT_BLOCK_SIZE 4

unsigned /* Returns the length of encoded data */
base64_encode(uint8_t *dst,
              unsigned src_length,
              const uint8_t *src);

/* Precise length of encoded data (including padding) */
#define BASE64_ENCODE_LENGTH(src_length)		\
        ((BASE64_BINARY_BLOCK_SIZE - 1 + (src_length))	\
	/ BASE64_BINARY_BLOCK_SIZE * BASE64_TEXT_BLOCK_SIZE)

/* FIXME: Perhaps rename to base64_decode_ctx? */
struct base64_ctx /* Internal, do not modify */
{
  uint16_t accum; /* Partial byte accumulated so far, filled msb first */
  int16_t shift;  /* Bitshift for the next 6-bit segment added to buffer */
};

void
base64_decode_init(struct base64_ctx *ctx);

unsigned /* Returns the length of decoded data */
base64_decode_update(struct base64_ctx *ctx,
                     uint8_t *dst,
                     unsigned src_length,
                     const uint8_t *src);

/* FIXME: Does this always round correctly? */
/* Maximum length of decoded data */
#define BASE64_DECODE_LENGTH(src_length) \
	((src_length) * BASE64_BINARY_BLOCK_SIZE / BASE64_TEXT_BLOCK_SIZE)

#endif /* NETTLE_BASE64_H_INCLUDED */
