/* sha512.c
 *
 * The sha512 hash function FIXME: Add the SHA384 variant.
 *
 * See http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2001, 2010 Niels M�ller
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

/* Modelled after the sha1.c code by Peter Gutmann. */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sha.h"

#include "macros.h"

/* Generated by the gp script

     {
       print("obase=16");
       for (i = 1,80,
         root = prime(i)^(1/3);
         fraction = root - floor(root);
         print(floor(2^64 * fraction));
       );
       quit();
     }

   piped through

     |grep -v '^[' | bc \
       |awk '{printf("0x%sULL,%s", $1, NR%3 == 0 ? "\n" : "");}'

   to convert it to hex.
*/

static const uint64_t
K[80] =
{
  0x428A2F98D728AE22ULL,0x7137449123EF65CDULL,
  0xB5C0FBCFEC4D3B2FULL,0xE9B5DBA58189DBBCULL,
  0x3956C25BF348B538ULL,0x59F111F1B605D019ULL,
  0x923F82A4AF194F9BULL,0xAB1C5ED5DA6D8118ULL,
  0xD807AA98A3030242ULL,0x12835B0145706FBEULL,
  0x243185BE4EE4B28CULL,0x550C7DC3D5FFB4E2ULL,
  0x72BE5D74F27B896FULL,0x80DEB1FE3B1696B1ULL,
  0x9BDC06A725C71235ULL,0xC19BF174CF692694ULL,
  0xE49B69C19EF14AD2ULL,0xEFBE4786384F25E3ULL,
  0xFC19DC68B8CD5B5ULL,0x240CA1CC77AC9C65ULL,
  0x2DE92C6F592B0275ULL,0x4A7484AA6EA6E483ULL,
  0x5CB0A9DCBD41FBD4ULL,0x76F988DA831153B5ULL,
  0x983E5152EE66DFABULL,0xA831C66D2DB43210ULL,
  0xB00327C898FB213FULL,0xBF597FC7BEEF0EE4ULL,
  0xC6E00BF33DA88FC2ULL,0xD5A79147930AA725ULL,
  0x6CA6351E003826FULL,0x142929670A0E6E70ULL,
  0x27B70A8546D22FFCULL,0x2E1B21385C26C926ULL,
  0x4D2C6DFC5AC42AEDULL,0x53380D139D95B3DFULL,
  0x650A73548BAF63DEULL,0x766A0ABB3C77B2A8ULL,
  0x81C2C92E47EDAEE6ULL,0x92722C851482353BULL,
  0xA2BFE8A14CF10364ULL,0xA81A664BBC423001ULL,
  0xC24B8B70D0F89791ULL,0xC76C51A30654BE30ULL,
  0xD192E819D6EF5218ULL,0xD69906245565A910ULL,
  0xF40E35855771202AULL,0x106AA07032BBD1B8ULL,
  0x19A4C116B8D2D0C8ULL,0x1E376C085141AB53ULL,
  0x2748774CDF8EEB99ULL,0x34B0BCB5E19B48A8ULL,
  0x391C0CB3C5C95A63ULL,0x4ED8AA4AE3418ACBULL,
  0x5B9CCA4F7763E373ULL,0x682E6FF3D6B2B8A3ULL,
  0x748F82EE5DEFB2FCULL,0x78A5636F43172F60ULL,
  0x84C87814A1F0AB72ULL,0x8CC702081A6439ECULL,
  0x90BEFFFA23631E28ULL,0xA4506CEBDE82BDE9ULL,
  0xBEF9A3F7B2C67915ULL,0xC67178F2E372532BULL,
  0xCA273ECEEA26619CULL,0xD186B8C721C0C207ULL,
  0xEADA7DD6CDE0EB1EULL,0xF57D4F7FEE6ED178ULL,
  0x6F067AA72176FBAULL,0xA637DC5A2C898A6ULL,
  0x113F9804BEF90DAEULL,0x1B710B35131C471BULL,
  0x28DB77F523047D84ULL,0x32CAAB7B40C72493ULL,
  0x3C9EBE0A15C9BEBCULL,0x431D67C49C100D4CULL,
  0x4CC5D4BECB3E42B6ULL,0x597F299CFC657E2AULL,
  0x5FCB6FAB3AD6FAECULL,0x6C44198C4A475817ULL,
};

#define COMPRESS(ctx, data) (_nettle_sha512_compress((ctx)->state, (data), K))

void
sha512_init(struct sha512_ctx *ctx)
{
  /* Initial values, generated by the gp script
       {
         for (i = 1,8,
	   root = prime(i)^(1/2);
	   fraction = root - floor(root);
	   print(floor(2^64 * fraction));
	 );
       }
. */
  static const uint64_t H0[_SHA512_DIGEST_LENGTH] =
  {
    0x6A09E667F3BCC908ULL,0xBB67AE8584CAA73BULL,
    0x3C6EF372FE94F82BULL,0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL,0x9B05688C2B3E6C1FULL,
    0x1F83D9ABFB41BD6BULL,0x5BE0CD19137E2179ULL,
  };

  memcpy(ctx->state, H0, sizeof(H0));

  /* Initialize bit count */
  ctx->count_low = ctx->count_high = 0;
  
  /* Initialize buffer */
  ctx->index = 0;
}

void
sha512_update(struct sha512_ctx *ctx,
	      unsigned length, const uint8_t *data)
{
  MD_UPDATE (ctx, length, data, COMPRESS, MD_INCR(ctx));
}

static void
sha512_write_digest(struct sha512_ctx *ctx,
		    unsigned length,
		    uint8_t *digest)
{
  uint64_t high, low;

  unsigned i;
  unsigned words;
  unsigned leftover;

  assert(length <= SHA512_DIGEST_SIZE);

  MD_PAD(ctx, 16, COMPRESS);

  /* There are 1024 = 2^10 bits in one block */  
  high = (ctx->count_high << 10) | (ctx->count_low >> 54);
  low = (ctx->count_low << 10) | (ctx->index << 3);

  /* This is slightly inefficient, as the numbers are converted to
     big-endian format, and will be converted back by the compression
     function. It's probably not worth the effort to fix this. */
  WRITE_UINT64(ctx->block + (SHA512_DATA_SIZE - 16), high);
  WRITE_UINT64(ctx->block + (SHA512_DATA_SIZE - 8), low);
  COMPRESS(ctx, ctx->block);

  words = length / 8;
  leftover = length % 8;

  for (i = 0; i < words; i++, digest += 8)
    WRITE_UINT64(digest, ctx->state[i]);

  if (leftover)
    {
      /* Truncate to the right size */
      uint64_t word = ctx->state[i] >> (8*(8 - leftover));

      do {
	digest[--leftover] = word & 0xff;
	word >>= 8;
      } while (leftover);
    }
}

void
sha512_digest(struct sha512_ctx *ctx,
	      unsigned length,
	      uint8_t *digest)
{
  assert(length <= SHA512_DIGEST_SIZE);

  sha512_write_digest(ctx, length, digest);
  sha512_init(ctx);
}

/* sha384 variant. FIXME: Move to separate file? */
void
sha384_init(struct sha512_ctx *ctx)
{
  /* Initial values, generated by the gp script
       {
         for (i = 9,16,
	   root = prime(i)^(1/2);
	   fraction = root - floor(root);
	   print(floor(2^64 * fraction));
	 );
       }
. */
  static const uint64_t H0[_SHA512_DIGEST_LENGTH] =
  {
    0xCBBB9D5DC1059ED8ULL, 0x629A292A367CD507ULL,
    0x9159015A3070DD17ULL, 0x152FECD8F70E5939ULL,
    0x67332667FFC00B31ULL, 0x8EB44A8768581511ULL,
    0xDB0C2E0D64F98FA7ULL, 0x47B5481DBEFA4FA4ULL,
  };

  memcpy(ctx->state, H0, sizeof(H0));

  /* Initialize bit count */
  ctx->count_low = ctx->count_high = 0;
  
  /* Initialize buffer */
  ctx->index = 0;
}

void
sha384_digest(struct sha512_ctx *ctx,
	      unsigned length,
	      uint8_t *digest)
{
  assert(length <= SHA384_DIGEST_SIZE);

  sha512_write_digest(ctx, length, digest);
  sha384_init(ctx);
}
