/* crypto.c
 *
 * Encryption classes on top of nettle.
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2001 Niels M�ller
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

#include "crypto.h"

#include "werror.h"
#include "xalloc.h"

#include "nettle/arcfour.h"
#include "nettle/aes.h"
#include "nettle/blowfish.h"
#include "nettle/des.h"
#include "nettle/twofish.h"

#include "nettle/cbc.h"

#include <assert.h>
#include <string.h>

#include "crypto.c.x"


/* Arcfour/RC4 */
/* GABA:
   (class
     (name arcfour_instance)
     (super crypto_instance)
     (vars
       (ctx . "struct arcfour_ctx")))
*/
   
static void
do_crypt_arcfour(struct crypto_instance *s,
		 UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(arcfour_instance, self, s);

  assert(!(length % 8));

  arcfour_crypt(&self->ctx, length, dst, src);
}

static struct crypto_instance *
make_arcfour_instance(struct crypto_algorithm *ignored UNUSED,
		      int mode UNUSED,
		      const UINT8 *key, const UINT8 *iv UNUSED)
{
  NEW(arcfour_instance, self);

  self->super.block_size = 8;
  self->super.crypt = do_crypt_arcfour;

  arcfour_set_key(&self->ctx, 16, key);

  return &self->super;
}

struct crypto_algorithm crypto_arcfour_algorithm =
{ STATIC_HEADER,
  8, 16, 0, make_arcfour_instance };

/* AES/Rijndael */
/* GABA:
   (class
     (name aes_instance)
     (super crypto_instance)
     (vars
       (ctx . "struct CBC_CTX(struct aes_ctx, AES_BLOCK_SIZE)")))
*/

static void
do_aes_encrypt(struct crypto_instance *s,
	       UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(aes_instance, self, s);

  CBC_ENCRYPT(&self->ctx, aes_encrypt, length, dst, src);
}

static void
do_aes_decrypt(struct crypto_instance *s,
	       UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(aes_instance, self, s);

  CBC_DECRYPT(&self->ctx, aes_decrypt, length, dst, src);
}

static struct crypto_instance *
make_aes_cbc_instance(struct crypto_algorithm *algorithm, int mode,
                      const UINT8 *key, const UINT8 *iv)
{
  NEW(aes_instance, self);

  self->super.block_size = AES_BLOCK_SIZE;
  self->super.crypt = ( (mode == CRYPTO_ENCRYPT)
			? do_aes_encrypt
			: do_aes_decrypt);

  aes_set_key(&self->ctx.ctx, algorithm->key_size, key);
  CBC_SET_IV(&self->ctx, iv);
  
  return(&self->super);
}

struct crypto_algorithm crypto_aes256_cbc_algorithm =
{ STATIC_HEADER, AES_BLOCK_SIZE, 32, AES_BLOCK_SIZE, make_aes_cbc_instance};


/* Triple DES */
/* GABA:
   (class
     (name des3_instance)
     (super crypto_instance)
     (vars
       (ctx . "struct CBC_CTX(struct des3_ctx, DES3_BLOCK_SIZE)")))
*/

static void
do_des3_encrypt(struct crypto_instance *s,
		UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(des3_instance, self, s);

  CBC_ENCRYPT(&self->ctx, des3_encrypt, length, dst, src);
}

static void
do_des3_decrypt(struct crypto_instance *s,
		UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(des3_instance, self, s);

  CBC_DECRYPT(&self->ctx, des3_decrypt, length, dst, src);
}

static struct crypto_instance *
make_des3_cbc_instance(struct crypto_algorithm *algorithm UNUSED,
                       int mode,
                       const UINT8 *key, const UINT8 *iv)
{
  NEW(des3_instance, self);
  UINT8 pkey[DES3_KEY_SIZE];

  /* Fix odd parity */
  des_fix_parity(DES3_KEY_SIZE, pkey, key);
  
  self->super.block_size = DES3_BLOCK_SIZE;
  self->super.crypt = ( (mode == CRYPTO_ENCRYPT)
			? do_des3_encrypt
			: do_des3_decrypt);

  CBC_SET_IV(&self->ctx, iv);
  
  if (des3_set_key(&self->ctx.ctx, pkey))
    return(&self->super);

  switch(self->ctx.ctx.status)
    {
    case DES_BAD_PARITY:
      fatal("Internal error! Bad parity in make_des3_instance.\n");
    case DES_WEAK_KEY:
      werror("Detected weak DES key.\n");
      KILL(self);
      return NULL;
    default:
      fatal("Internal error!\n");
    }
}

struct crypto_algorithm crypto_des3_cbc_algorithm =
{ STATIC_HEADER,
  DES3_BLOCK_SIZE, DES3_KEY_SIZE, DES3_BLOCK_SIZE, make_des3_cbc_instance };


/* Twofish */
/* GABA:
   (class
     (name twofish_instance)
     (super crypto_instance)
     (vars
       (ctx . "struct CBC_CTX(struct twofish_ctx, TWOFISH_BLOCK_SIZE)")))
*/

static void
do_twofish_encrypt(struct crypto_instance *s,
	       UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(twofish_instance, self, s);

  CBC_ENCRYPT(&self->ctx, twofish_encrypt, length, dst, src);
}

static void
do_twofish_decrypt(struct crypto_instance *s,
	       UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(twofish_instance, self, s);

  CBC_DECRYPT(&self->ctx, twofish_decrypt, length, dst, src);
}

static struct crypto_instance *
make_twofish_cbc_instance(struct crypto_algorithm *algorithm, int mode,
		      const UINT8 *key, const UINT8 *iv UNUSED)
{
  NEW(twofish_instance, self);

  self->super.block_size = TWOFISH_BLOCK_SIZE;
  self->super.crypt = ( (mode == CRYPTO_ENCRYPT)
			? do_twofish_encrypt
			: do_twofish_decrypt);

  twofish_set_key(&self->ctx.ctx, algorithm->key_size, key);
  CBC_SET_IV(&self->ctx, iv);

  return(&self->super);
}

struct crypto_algorithm crypto_twofish256_cbc_algorithm =
{ STATIC_HEADER,
  TWOFISH_BLOCK_SIZE, 32, TWOFISH_BLOCK_SIZE, make_twofish_cbc_instance};


/* Blowfish */
/* GABA:
   (class
     (name blowfish_instance)
     (super crypto_instance)
     (vars
       (ctx . "struct CBC_CTX(struct blowfish_ctx, BLOWFISH_BLOCK_SIZE)")))
*/

static void
do_blowfish_encrypt(struct crypto_instance *s,
                    UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(blowfish_instance, self, s);

  CBC_ENCRYPT(&self->ctx, blowfish_encrypt, length, dst, src);
}

static void
do_blowfish_decrypt(struct crypto_instance *s,
	       UINT32 length, const UINT8 *src, UINT8 *dst)
{
  CAST(blowfish_instance, self, s);

  CBC_DECRYPT(&self->ctx, blowfish_decrypt, length, dst, src);
}

static struct crypto_instance *
make_blowfish_cbc_instance(struct crypto_algorithm *algorithm, int mode,
                           const UINT8 *key, const UINT8 *iv UNUSED)
{
  NEW(blowfish_instance, self);

  self->super.block_size = BLOWFISH_BLOCK_SIZE;
  self->super.crypt = ( (mode == CRYPTO_ENCRYPT)
			? do_blowfish_encrypt
			: do_blowfish_decrypt);

  CBC_SET_IV(&self->ctx, iv);

  if (blowfish_set_key(&self->ctx.ctx, algorithm->key_size, key))
    return(&self->super);
  else
    {
      werror("Detected a weak blowfish key!\n");
      KILL(self);
      return NULL;
    }
}

struct crypto_algorithm crypto_blowfish_cbc_algorithm =
{ STATIC_HEADER,
  BLOWFISH_BLOCK_SIZE, BLOWFISH_KEY_SIZE, BLOWFISH_BLOCK_SIZE,
  make_blowfish_cbc_instance};

