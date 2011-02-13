/* gcm_aes.c
 *
 * Galois counter mode using AES as the underlying cipher.
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2011 Niels Möller
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

#include "gcm.h"

void
gcm_aes_set_key(struct gcm_aes_ctx *ctx, unsigned length, const uint8_t *key)
{
  GCM_SET_KEY(ctx, aes_set_encrypt_key, (nettle_crypt_func *) aes_encrypt,
	      length, key);
}

void
gcm_aes_set_iv(struct gcm_aes_ctx *ctx,
	       unsigned length, const uint8_t *iv)
{
  gcm_set_iv(&ctx->gcm, &ctx->key, length, iv);
}

void
gcm_aes_update(struct gcm_aes_ctx *ctx, unsigned length, const uint8_t *data)
{
  GCM_UPDATE(ctx, (nettle_crypt_func *) aes_encrypt,
	     length, data);
}

void
gcm_aes_encrypt(struct gcm_aes_ctx *ctx,
		unsigned length, uint8_t *dst, const uint8_t *src)
{
  GCM_ENCRYPT(ctx, (nettle_crypt_func *) aes_encrypt,
	      length, dst, src);
}

void
gcm_aes_decrypt(struct gcm_aes_ctx *ctx,
		unsigned length, uint8_t *dst, const uint8_t *src)
{
  GCM_DECRYPT(ctx, (nettle_crypt_func *) aes_encrypt,
	      length, dst, src);
}

void
gcm_aes_digest(struct gcm_aes_ctx *ctx,
	       unsigned length, uint8_t *digest)
{
  GCM_DIGEST(ctx, (nettle_crypt_func *) aes_encrypt,
	     length, digest);
  
}
