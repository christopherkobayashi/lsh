/* nettle-benchmark.c
 *
 * Tries the performance of the various algorithms.
 *
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

#if HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include "aes.h"
#include "arcfour.h"
#include "blowfish.h"
#include "cast128.h"
#include "des.h"
#include "serpent.h"
#include "twofish.h"

#include "nettle-meta.h"
#include "nettle-internal.h"

#include "cbc.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#if __GNUC__
# define UNUSED __attribute__ ((__unused__))
#else
# define UNUSED
#endif

/* Encrypt 10MB, 1K at a time. */
#define BENCH_BLOCK 1024
#define BENCH_COUNT 10240

/* Total MB:s, for MB/s figures. */
#define BENCH_TOTAL 10.0

static double
time_function(void (*f)(void *arg), void *arg)
{
  clock_t before;
  clock_t after;

  before = clock();
  
  f(arg);
  
  after = clock();

  return ((double)(after - before)) / CLOCKS_PER_SEC;
}

struct bench_cipher_info
{
  void *ctx;
  nettle_crypt_func crypt;
  uint8_t *data;
};

static void
bench_cipher(void *arg)
{
  struct bench_cipher_info *info = arg;
  unsigned i;
  
  for (i = 0; i<BENCH_COUNT; i++)
    info->crypt(info->ctx, BENCH_BLOCK, info->data, info->data);
}

struct bench_cbc_info
{
  void *ctx;
  nettle_crypt_func crypt;
 
  uint8_t *data;
  
  unsigned block_size;
  uint8_t *iv;
};

static void
bench_cbc_encrypt(void *arg)
{
  struct bench_cbc_info *info = arg;
  unsigned i;

  for (i = 0; i<BENCH_COUNT; i++)
    cbc_encrypt(info->ctx, info->crypt,
		info->block_size, info->iv,
		BENCH_BLOCK, info->data, info->data);
}

static void
bench_cbc_decrypt(void *arg)
{
  struct bench_cbc_info *info = arg;
  unsigned i;

  for (i = 0; i<BENCH_COUNT; i++)
    cbc_decrypt(info->ctx, info->crypt,
		info->block_size, info->iv,
		BENCH_BLOCK, info->data, info->data);
}

/* Set data[i] = floor(sqrt(i)) */
static void
init_data(uint8_t *data)
{
  unsigned i,j;
  for (i = j = 0; i<BENCH_BLOCK;  i++)
    {
      if (j*j < i)
	j++;
      data[i] = j;
    }
}

static void
init_key(unsigned length,
         uint8_t *key)
{
  unsigned i;
  for (i = 0; i<length; i++)
    key[i] = i;
}

static void
display(const char *name, const char *mode,
	double elapsed)
{
  printf("%13s (%s): %.2fs, %.3fMB/s\n",
	 name, mode,
	 elapsed, BENCH_TOTAL / elapsed);
}

static void
time_cipher(const struct nettle_cipher *cipher)
{
  void *ctx = alloca(cipher->context_size);
  uint8_t *key = alloca(cipher->key_size);

  uint8_t data[BENCH_BLOCK];

  printf("\n");
  
  init_data(data);

  {
    /* Decent initializers are a GNU extension, so don't use it here. */
    struct bench_cipher_info info;
    info.ctx = ctx;
    info.crypt = cipher->encrypt;
    info.data = data;
    
    init_key(cipher->key_size, key);
    cipher->set_encrypt_key(ctx, cipher->key_size, key);

    display(cipher->name, "ECB encrypt",
	    time_function(bench_cipher, &info));
  }
  
  {
    struct bench_cipher_info info;
    info.ctx = ctx;
    info.crypt = cipher->decrypt;
    info.data = data;
    
    init_key(cipher->key_size, key);
    cipher->set_decrypt_key(ctx, cipher->key_size, key);

    display(cipher->name, "ECB decrypt",
	    time_function(bench_cipher, &info));
  }

  if (cipher->block_size)
    {
      uint8_t *iv = alloca(cipher->block_size);
      
      /* Do CBC mode */
      {
        struct bench_cbc_info info;
	info.ctx = ctx;
	info.crypt = cipher->encrypt;
	info.data = data;
	info.block_size = cipher->block_size;
	info.iv = iv;
    
        memset(iv, 0, sizeof(iv));
    
        cipher->set_encrypt_key(ctx, cipher->key_size, key);

	display(cipher->name, "CBC encrypt",
		time_function(bench_cbc_encrypt, &info));
      }

      {
        struct bench_cbc_info info;
	info.ctx = ctx;
	info.crypt = cipher->decrypt;
	info.data = data;
	info.block_size = cipher->block_size;
	info.iv = iv;
    
        memset(iv, 0, sizeof(iv));

        cipher->set_decrypt_key(ctx, cipher->key_size, key);

	display(cipher->name, "CBC decrypt",
		time_function(bench_cbc_decrypt, &info));
      }
    }
}

#if HAVE_LIBCRYPTO
# define OPENSSL(x) x,
#else
# define OPENSSL(x)
#endif

int
main(int argc UNUSED, char **argv UNUSED)
{
  unsigned i;
  const struct nettle_cipher *ciphers[] =
    {
      &nettle_aes128, &nettle_aes192, &nettle_aes256,
      &nettle_arcfour128,
      &nettle_blowfish128,
      OPENSSL(&nettle_openssl_blowfish128)
      &nettle_cast128, OPENSSL(&nettle_openssl_cast128)
      &nettle_des, OPENSSL(&nettle_openssl_des)
      &nettle_des3,
      &nettle_serpent256,
      &nettle_twofish128, &nettle_twofish192, &nettle_twofish256,
      NULL
    };

  for (i = 0; ciphers[i]; i++)
    time_cipher(ciphers[i]);
  
  return 0;
}
