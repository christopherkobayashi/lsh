/* cbc.c
 *
 * Cipher block chaining mode.
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2001 Niels M�ller
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

#include "cbc.h"

#include <assert.h>

void
cbc_encrypt(void *ctx, void (*f)(void *ctx,
				 unsigned length, uint8_t *dst,
				 const uint8_t *src),
	    unsigned block_size, uint8_t *iv,
	    unsigned length, uint8_t *dst,
	    const uint8_t *src)
{
  assert(!(length % block_size));

  for ( ; length; length -= block_size, src += block_size, dst += block_size)
    {
      memxor(iv, src, block_size);
      f(ctx, dst, src, block_size);
      memcpy(iv, dst, block_size);
    }
}

void
cbc_decrypt(void *ctx, void (*f)(void *ctx,
				 unsigned length, uint8_t *dst,
				 const uint8_t *src),
	    unsigned block_size, uint8_t *iv,
	    unsigned length, uint8_t *dst,
	    const uint8_t *src)
{
  assert(!(length % block_size));

  if (!length)
    return;

  if (src == dst)
    {
      /* Keep a copy of the ciphertext. */
      /* FIXME: If length is large enough, allocate a smaller buffer
       * and process one buffer size at a time */
      uint8_t *tmp = alloca(length);
      memcpy(tmp, src, length);
      src = tmp;
    }

  /* Decrypt in ECB mode */
  f(ctx, dst, src, length);

  /* XOR the cryptotext, shifted one block */
  memxor(dst, iv, block_size);
  memxor(dst + block_size, src, length - block_size);
  memcpy(iv, src + length - block_size, block_size);
}
