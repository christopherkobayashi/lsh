#include "testutils.h"

#include "algorithms.h"
#include "format.h"
#include "randomness.h"
#include "spki.h"
#include "werror.h"
#include "xalloc.h"

#include "nettle/knuth-lfib.h"
#include "nettle/sexp.h"

/* -1 means invalid */
static const signed char hex_digits[0x100] =
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

static unsigned
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

struct lsh_string *
decode_hex(const char *h)
{  
  const unsigned char *hex = (const unsigned char *) h;
  UINT32 length = decode_hex_length(h);
  UINT8 *dst;
  
  unsigned i = 0;
  struct lsh_string *s = ssh_format("%lr", length, &dst);

  for (;;)
    {
      int high, low;
    
      while (*hex && isspace(*hex))
	hex++;

      if (!*hex)
	return s;

      high = hex_digits[*hex++];
      if (high < 0)
	return NULL;

      while (*hex && isspace(*hex))
	hex++;

      if (!*hex)
	return NULL;

      low = hex_digits[*hex++];
      if (low < 0)
	return NULL;

      dst[i++] = (high << 4) | low;
    }
}

int
main(int argc, char **argv)
{
  (void) argc; (void) argv;
  return test_main();
}

void
test_cipher(const char *name, struct crypto_algorithm *algorithm,
	    const struct lsh_string *key,
	    const struct lsh_string *plain,
	    const struct lsh_string *cipher,
	    const struct lsh_string *iv)
{
  struct crypto_instance *c;
  struct lsh_string *x;

  (void) name;
  
  if (iv)
    {
      if (algorithm->iv_size != iv->length)
	FAIL();
    }
  else if (algorithm->iv_size)
    FAIL();

  c = MAKE_ENCRYPT(algorithm, key->data, iv ? iv->data : NULL);

  x = crypt_string(c, plain, 0);
  if (!lsh_string_eq(x, cipher))
    FAIL();
  
  KILL(c);
  
  c = MAKE_DECRYPT(algorithm, key->data, iv ? iv->data : NULL);

  x = crypt_string(c, x, 1);
  if (!lsh_string_eq(x, plain))
    FAIL();

  KILL(c);
  lsh_string_free(x);
}

void
test_hash(const char *name,
	  const struct hash_algorithm *algorithm,
	  const struct lsh_string *data,
	  const struct lsh_string *digest)
{
  (void) name;
  if (!lsh_string_eq(hash_string(algorithm, data, 0), digest))
    FAIL();
}

void
test_mac(const char *name,
	  struct mac_algorithm *algorithm,
	  const struct lsh_string *key,
	  const struct lsh_string *data,
	  const struct lsh_string *digest)
{
  (void) name;
  if (!lsh_string_eq(mac_string(algorithm, key, 0, data, 0),
		     digest))
    FAIL();
}

struct bad_random
{
  struct randomness super;
  struct knuth_lfib_ctx *ctx;
};

static void
do_bad_random(struct randomness *r, UINT32 length, UINT8 *dst)
{
  struct bad_random *self = (struct bad_random *) r;
  knuth_lfib_random(self->ctx, length, dst);
}

void
test_sign(const char *name,
	  const struct lsh_string *key,
	  struct lsh_string *msg,
	  const struct lsh_string *signature)
{
  struct alist *algorithms;
  struct lsh_string *sign;
  struct signer *s;
  struct verifier *v;
  struct sexp_iterator i;
  UINT8 *decoded;  

  struct knuth_lfib_ctx ctx;
  struct bad_random r = { { STACK_HEADER, RANDOM_GOOD /* a lie */,
			    do_bad_random, NULL },
			  &ctx
			  };
  knuth_lfib_init(&ctx, time(NULL));

  algorithms = all_signature_algorithms(&r.super);
  (void) name;

  decoded = alloca(key->length);
  memcpy(decoded, key->data, key->length);
  
  if (!sexp_transport_iterator_first(&i, key->length, decoded))
    FAIL();

  /* Can't use spki_make_signer, because private-key tag is missing. */
  s = spki_sexp_to_signer(algorithms, &i, NULL);
  if (!s)
    FAIL();

  sign = SIGN(s, ATOM_SPKI, msg->length, msg->data);

  /* If caller passed signature == NULL, skip this check. */
  if (signature && !lsh_string_eq(signature, sign))
    FAIL();

  v = SIGNER_GET_VERIFIER(s);
  if (!v)
    /* Can't create verifier */
    FAIL();

  if (!VERIFY(v, ATOM_SPKI, msg->length, msg->data,
	      sign->length, sign->data))
    /* Unexpected verification failure. */
    FAIL();
  
  /* Modify message slightly. */
  if (msg->length < 10)
    FAIL();

  msg->data[5] ^= 0x40;

  if (VERIFY(v, ATOM_SPKI, msg->length, msg->data,
	     sign->length, sign->data))
    /* Unexpected verification success. */
    FAIL();

  lsh_string_free(sign);
  
  KILL(v);
  KILL(s);
}

static int
test_spki_match(const char *name,
		const struct lsh_string *resource,
		const struct lsh_string *access)
{
  struct sexp_iterator i;
  struct spki_tag *tag;

  if (!sexp_iterator_first(&i, resource->length, resource->data))
    return 0;
  
  tag = spki_sexp_to_tag(&i, 17);

  if (!tag)
    return 0;

  if (!sexp_iterator_first(&i, access->length, access->data))
    return 0;
  
  (void) name;
  return SPKI_TAG_MATCH(tag, &i);
}

void
test_spki_grant(const char *name,
		const struct lsh_string *resource,
		const struct lsh_string *access)
{
  if (!test_spki_match(name, resource, access))
    FAIL();
}

void
test_spki_deny(const char *name,
	       const struct lsh_string *resource,
	       const struct lsh_string *access)
{
  if (test_spki_match(name, resource, access))
    FAIL();
}

