/* testutils.c */

#include "testutils.h"

#include "cbc.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* For getopt() */
#include <unistd.h>

/* -1 means invalid */
const signed char hex_digits[0x100] =
  {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };

unsigned
decode_hex_length(const char *h)
{
  const unsigned char *hex = (const unsigned char *) h;
  unsigned count;
  unsigned i;
  
  for (count = i = 0; hex[i]; i++)
    {
      if (isspace(hex[i]))
	continue;
      if (hex_digits[hex[i]] < 0)
	abort();
      count++;
    }

  if (count % 2)
    abort();
  return count / 2;  
}

int
decode_hex(uint8_t *dst, const char *h)
{  
  const unsigned char *hex = (const unsigned char *) h;
  unsigned i = 0;
  
  for (;;)
  {
    int high, low;
    
    while (*hex && isspace(*hex))
      hex++;

    if (!*hex)
      return 1;

    high = hex_digits[*hex++];
    if (high < 0)
      return 0;

    while (*hex && isspace(*hex))
      hex++;

    if (!*hex)
      return 0;

    low = hex_digits[*hex++];
    if (low < 0)
      return 0;

    dst[i++] = (high << 4) | low;
  }
}

const uint8_t *
decode_hex_dup(const char *hex)
{
  uint8_t *p;
  unsigned length = decode_hex_length(hex);

  p = malloc(length);
  if (!p)
    abort();

  if (decode_hex(p, hex))
    return p;
  else
    {
      free(p);
      return NULL;
    }
}

int verbose = 0;

int
main(int argc, char **argv)
{
  int c;

  while ((c = getopt (argc, argv, "v")) != -1)
    switch (c)
      {
      case 'v':
	verbose = 1;
	break;
      case '?':
	if (isprint (optopt))
	  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	else
	  fprintf (stderr,
		   "Unknown option character `\\x%x'.\n",
		   optopt);
      default:
	abort();
      }

  return test_main();
}

void
test_cipher(const struct nettle_cipher *cipher,
	    unsigned key_length,
	    const uint8_t *key,
	    unsigned length,
	    const uint8_t *cleartext,
	    const uint8_t *ciphertext)
{
  void *ctx = alloca(cipher->context_size);
  uint8_t *data = alloca(length);

  cipher->set_encrypt_key(ctx, key_length, key);
  cipher->encrypt(ctx, length, data, cleartext);

  if (!MEMEQ(length, data, ciphertext))
    FAIL();

  cipher->set_decrypt_key(ctx, key_length, key);
  cipher->decrypt(ctx, length, data, data);

  if (!MEMEQ(length, data, cleartext))
    FAIL();
}

void
test_cipher_cbc(const struct nettle_cipher *cipher,
		unsigned key_length,
		const uint8_t *key,
		unsigned length,
		const uint8_t *cleartext,
		const uint8_t *ciphertext,
		const uint8_t *iiv)
{
  void *ctx = alloca(cipher->context_size);
  uint8_t *data = alloca(length);
  uint8_t *iv = alloca(cipher->block_size);
  
  cipher->set_encrypt_key(ctx, key_length, key);
  memcpy(iv, iiv, cipher->block_size);

  cbc_encrypt(ctx, cipher->encrypt,
	      cipher->block_size, iv,
	      length, data, cleartext);

  if (!MEMEQ(length, data, ciphertext))
    FAIL();

  cipher->set_decrypt_key(ctx, key_length, key);
  memcpy(iv, iiv, cipher->block_size);

  cbc_decrypt(ctx, cipher->decrypt,
	      cipher->block_size, iv,
	      length, data, data);

  if (!MEMEQ(length, data, cleartext))
    FAIL();
}

void
test_hash(const struct nettle_hash *hash,
	  unsigned length,
	  const uint8_t *data,
	  const uint8_t *digest)
{
  void *ctx = alloca(hash->context_size);
  uint8_t *buffer = alloca(hash->digest_size);

  hash->init(ctx);
  hash->update(ctx, length, data);
  hash->digest(ctx, hash->digest_size, buffer);

  if (!MEMEQ(hash->digest_size, digest, buffer))
    FAIL();

  memset(buffer, 0, hash->digest_size);

  hash->init(ctx);
  hash->update(ctx, length, data);
  hash->digest(ctx, hash->digest_size - 1, buffer);

  if (!MEMEQ(hash->digest_size - 1, digest, buffer))
    FAIL();

  if (buffer[hash->digest_size - 1])
    FAIL();
}
