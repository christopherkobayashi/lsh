/* nettle-benchmark.c
 *
 * Tries the performance of the various algorithms.
 *
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "aes.h"
#include "arcfour.h"
#include "blowfish.h"
#include "cast128.h"
#include "cbc.h"
#include "des.h"
#include "memxor.h"
#include "serpent.h"
#include "sha.h"
#include "twofish.h"

#include "nettle-meta.h"
#include "nettle-internal.h"

#include "getopt.h"

static double frequency = 0.0;

/* Process BENCH_BLOCK bytes at a time, for BENCH_INTERVAL seconds. */
#define BENCH_BLOCK 10240
#define BENCH_INTERVAL 0.1

/* FIXME: Proper configure test for rdtsc? */
#ifndef WITH_CYCLE_COUNTER
# if defined(__GNUC__) && defined(__i386__)
#  define WITH_CYCLE_COUNTER 1
# else
#  define WITH_CYCLE_COUNTER 0
# endif
#endif

#if WITH_CYCLE_COUNTER
#define GET_CYCLE_COUNTER(hi, lo)		\
  __asm__("xorl %%eax,%%eax\n"			\
	  "movl %%ebx, %%edi\n"			\
	  "cpuid\n"				\
	  "rdtsc\n"				\
	  "movl %%edi, %%ebx\n"			\
	  : "=a" (lo), "=d" (hi)		\
	  : /* No inputs. */			\
	  : "%edi", "%ecx", "cc")
#define BENCH_ITERATIONS 10
#endif

/* Returns second per function call */
static double
time_function(void (*f)(void *arg), void *arg)
{
  unsigned ncalls;
#if HAVE_CLOCK_GETTIME && defined CLOCK_PROCESS_CPUTIME_ID
  struct timespec before;
  struct timespec after;
  struct timespec done;

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &before);
  done = before;
  done.tv_nsec += BENCH_INTERVAL * 1e9;
  if (done.tv_nsec >= 1000000000L)
    {
      done.tv_nsec -= 1000000000L;
      done.tv_sec ++;
    }
  ncalls = 0;

  do 
    {
      f(arg);
      clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &after);
      ncalls++;
    }
  while (after.tv_sec < done.tv_sec
	 || (after.tv_nsec < done.tv_nsec && after.tv_sec == done.tv_sec));
  
  return (after.tv_sec - before.tv_sec
	  + 1e-9 * (after.tv_nsec - before.tv_nsec)) / ncalls;
#else /* !HAVE_CLOCK_GETTIME */
  clock_t before;
  clock_t after;
  clock_t done;
  
  before = clock();
  done = before + BENCH_INTERVAL * CLOCKS_PER_SEC;
  ncalls = 0;
  
  do 
    {
      f(arg);
      after = clock();
      ncalls++;
    }
  while (after < done);
  
  return ((double)(after - before)) / CLOCKS_PER_SEC / ncalls;
#endif /* !HAVE_CLOCK_GETTIME */
}

struct bench_memxor_info
{
  uint8_t *dst;
  const uint8_t *src;
};

static void
bench_memxor(void *arg)
{
  struct bench_memxor_info *info = arg;
  memxor (info->dst, info->src, BENCH_BLOCK);
}

struct bench_hash_info
{
  void *ctx;
  nettle_hash_update_func *update;
  const uint8_t *data;
};

static void
bench_hash(void *arg)
{
  struct bench_hash_info *info = arg;
  info->update(info->ctx, BENCH_BLOCK, info->data);
}

struct bench_cipher_info
{
  void *ctx;
  nettle_crypt_func *crypt;
  uint8_t *data;
};

static void
bench_cipher(void *arg)
{
  struct bench_cipher_info *info = arg;
  info->crypt(info->ctx, BENCH_BLOCK, info->data, info->data);
}

struct bench_cbc_info
{
  void *ctx;
  nettle_crypt_func *crypt;
 
  uint8_t *data;
  
  unsigned block_size;
  uint8_t *iv;
};

static void
bench_cbc_encrypt(void *arg)
{
  struct bench_cbc_info *info = arg;
  cbc_encrypt(info->ctx, info->crypt,
	      info->block_size, info->iv,
	      BENCH_BLOCK, info->data, info->data);
}

static void
bench_cbc_decrypt(void *arg)
{
  struct bench_cbc_info *info = arg;
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
header(void)
{
  printf("%18s %11s Mbyte/s%s\n",
	 "Algorithm", "mode", 
	 frequency > 0.0 ? " cycles/byte cycles/block" : "");  
}

static void
display(const char *name, const char *mode, unsigned block_size,
	double time)
{
  printf("%18s %11s %7.2f",
	 name, mode,
	 BENCH_BLOCK / (time * 1048576.0));
  if (frequency > 0.0)
    {
      printf(" %11.2f", time * frequency / BENCH_BLOCK);
      if (block_size > 0)
	printf(" %12.2f", time * frequency * block_size / BENCH_BLOCK);
    }
  printf("\n");
}

static void *
xalloc(size_t size)
{
  void *p = malloc(size);
  if (!p)
    {
      fprintf(stderr, "Virtual memory exhausted.\n");
      abort();
    }

  return p;
}

static void
time_memxor(void)
{
  struct bench_memxor_info info;
  uint8_t src[BENCH_BLOCK + 1];
  uint8_t dst[BENCH_BLOCK + 1];

  info.src = src;
  info.dst = dst;

  display ("xor", "aligned", 1, time_function(bench_memxor, &info));
  info.src++;
  display ("xor", "unaligned", 1, time_function(bench_memxor, &info));  
}

static void
time_hash(const struct nettle_hash *hash)
{
  static uint8_t data[BENCH_BLOCK];
  struct bench_hash_info info;

  info.ctx = xalloc(hash->context_size); 
  info.update = hash->update;
  info.data = data;

  init_data(data);
  hash->init(info.ctx);

  display(hash->name, "update", hash->block_size,
	  time_function(bench_hash, &info));

  free(info.ctx);
}

static void
time_cipher(const struct nettle_cipher *cipher)
{
  void *ctx = xalloc(cipher->context_size);
  uint8_t *key = xalloc(cipher->key_size);

  static uint8_t data[BENCH_BLOCK];

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

    display(cipher->name, "ECB encrypt", cipher->block_size,
	    time_function(bench_cipher, &info));
  }
  
  {
    struct bench_cipher_info info;
    info.ctx = ctx;
    info.crypt = cipher->decrypt;
    info.data = data;
    
    init_key(cipher->key_size, key);
    cipher->set_decrypt_key(ctx, cipher->key_size, key);

    display(cipher->name, "ECB decrypt", cipher->block_size,
	    time_function(bench_cipher, &info));
  }

  /* Don't use nettle cbc to benchmark openssl ciphers */
  if (cipher->block_size && cipher->name[0] != 'o')
    {
      uint8_t *iv = xalloc(cipher->block_size);
      
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

	display(cipher->name, "CBC encrypt", cipher->block_size,
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

	display(cipher->name, "CBC decrypt", cipher->block_size,
		time_function(bench_cbc_decrypt, &info));
      }
      free(iv);
    }
  free(ctx);
  free(key);
}

static int
compare_double(const void *ap, const void *bp)
{
  double a = *(const double *) ap;
  double b = *(const double *) bp;
  if (a < b)
    return -1;
  else if (a > b)
    return 1;
  else
    return 0;
}

/* Try to get accurate cycle times for assembler functions. */
static void
bench_sha1_compress(void)
{
#if WITH_CYCLE_COUNTER
  uint32_t state[_SHA1_DIGEST_LENGTH];
  uint8_t data[BENCH_ITERATIONS * SHA1_DATA_SIZE];
  uint32_t start_lo, start_hi, end_lo, end_hi;

  double count[5];
  
  uint8_t *p;
  unsigned i, j;

  for (j = 0; j < 5; j++)
    {
      i = 0;
      p = data;
      GET_CYCLE_COUNTER(start_hi, start_lo);
      for (; i < BENCH_ITERATIONS; i++, p += SHA1_DATA_SIZE)
	_nettle_sha1_compress(state, p);

      GET_CYCLE_COUNTER(end_hi, end_lo);

      end_hi -= (start_hi + (start_lo > end_lo));
      end_lo -= start_lo;

      count[j] = ldexp(end_hi, 32) + end_lo;
    }

  qsort(count, 5, sizeof(double), compare_double);
  printf("sha1_compress: %.2f cycles\n\n", count[2] / BENCH_ITERATIONS);  
#endif
}

#if WITH_OPENSSL
# define OPENSSL(x) x,
#else
# define OPENSSL(x)
#endif

int
main(int argc, char **argv)
{
  unsigned i;
  int c;
  const char *alg;

  const struct nettle_hash *hashes[] =
    {
      &nettle_md2, &nettle_md4, &nettle_md5,
      OPENSSL(&nettle_openssl_md5)
      &nettle_sha1, OPENSSL(&nettle_openssl_sha1)
      &nettle_sha224, &nettle_sha256,
      &nettle_sha384, &nettle_sha512,
      NULL
    };

  const struct nettle_cipher *ciphers[] =
    {
      &nettle_aes128, &nettle_aes192, &nettle_aes256,
      OPENSSL(&nettle_openssl_aes128)
      OPENSSL(&nettle_openssl_aes192)
      OPENSSL(&nettle_openssl_aes256)
      &nettle_arcfour128, OPENSSL(&nettle_openssl_arcfour128)
      &nettle_blowfish128, OPENSSL(&nettle_openssl_blowfish128)
      &nettle_camellia128, &nettle_camellia192, &nettle_camellia256,
      &nettle_cast128, OPENSSL(&nettle_openssl_cast128)
      &nettle_des, OPENSSL(&nettle_openssl_des)
      &nettle_des3,
      &nettle_serpent256,
      &nettle_twofish128, &nettle_twofish192, &nettle_twofish256,
      NULL
    };

  while ( (c = getopt(argc, argv, "f:")) != -1)
    switch (c)
      {
      case 'f':
	frequency = atof(optarg);
	if (frequency > 0.0)
	  break;

      case ':': case '?':
	fprintf(stderr, "Usage: nettle-benchmark [-f clock frequency] [alg]\n");
	return EXIT_FAILURE;

      default:
	abort();
    }

  alg = argv[optind];
  
  bench_sha1_compress();

  header();

  if (!alg || strstr ("memxor", alg))
    {
      time_memxor();
      printf("\n");
    }
  
  for (i = 0; hashes[i]; i++)
    {
      if (!alg || strstr(hashes[i]->name, alg))
	time_hash(hashes[i]);
    }
  for (i = 0; ciphers[i]; i++)
    if (!alg || strstr(ciphers[i]->name, alg))
      time_cipher(ciphers[i]);

  return 0;
}
