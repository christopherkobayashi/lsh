#include "testutils.h"

#include "knuth-lfib.h"

#if __GNUC__
# define UNUSED __attribute__ ((__unused__))
#else
# define UNUSED
#endif

static void
progress(void *ctx UNUSED, int c)
{
  fputc(c, stderr);
}

static void
test_rsa_key(struct rsa_public_key *pub,
	     struct rsa_private_key *key)
{
  mpz_t tmp;
  mpz_t phi;
  
  mpz_init(tmp); mpz_init(phi);
  
  if (verbose)
    {
      /* FIXME: Use gmp_printf */
      fprintf(stderr, "Public key: n=");
      mpz_out_str(stderr, 16, pub->n);
      fprintf(stderr, "\n    e=");
      mpz_out_str(stderr, 16, pub->e);

      fprintf(stderr, "\n\nPrivate key: d=");
      mpz_out_str(stderr, 16, key->d);
      fprintf(stderr, "\n    p=");
      mpz_out_str(stderr, 16, key->p);
      fprintf(stderr, "\n    q=");
      mpz_out_str(stderr, 16, key->q);
      fprintf(stderr, "\n    a=");
      mpz_out_str(stderr, 16, key->a);
      fprintf(stderr, "\n    b=");
      mpz_out_str(stderr, 16, key->b);
      fprintf(stderr, "\n    c=");
      mpz_out_str(stderr, 16, key->c);
      fprintf(stderr, "\n\n");
    }

  /* Check n = p q */
  mpz_mul(tmp, key->p, key->q);
  if (mpz_cmp(tmp, pub->n))
    FAIL();

  /* Check c q = 1 mod p */
  mpz_mul(tmp, key->c, key->q);
  mpz_fdiv_r(tmp, tmp, key->p);
  if (mpz_cmp_ui(tmp, 1))
    FAIL();

  /* Check ed = 1 (mod phi) */
  mpz_sub_ui(phi, key->p, 1);
  mpz_sub_ui(tmp, key->q, 1);

  mpz_mul(phi, phi, tmp);

  mpz_mul(tmp, pub->e, key->d);
  mpz_fdiv_r(tmp, tmp, phi);
  if (mpz_cmp_ui(tmp, 1))
    FAIL();

  /* Check a e = 1 (mod (p-1) ) */
  mpz_sub_ui(phi, key->p, 1);
  mpz_mul(tmp, pub->e, key->a);
  mpz_fdiv_r(tmp, tmp, phi);
  if (mpz_cmp_ui(tmp, 1))
    FAIL();
  
  /* Check b e = 1 (mod (q-1) ) */
  mpz_sub_ui(phi, key->q, 1);
  mpz_mul(tmp, pub->e, key->b);
  mpz_fdiv_r(tmp, tmp, phi);
  if (mpz_cmp_ui(tmp, 1))
    FAIL();
  
  mpz_clear(tmp); mpz_clear(phi);
}

int
test_main(void)
{
#if HAVE_LIBGMP

  struct rsa_public_key pub;
  struct rsa_private_key key;
  
  struct knuth_lfib_ctx lfib;

  mpz_t expected;
  
  mpz_init(expected);
  
  rsa_init_private_key(&key);
  rsa_init_public_key(&pub);

  /* Generate a 1024 bit key with random e */
  knuth_lfib_init(&lfib, 13);

  if (!rsa_generate_keypair(&pub, &key,
			    &lfib, (nettle_random_func) knuth_lfib_random,
			    NULL, verbose ? progress : NULL,
			    1024, 50))
    FAIL();

  test_rsa_key(&pub, &key);
  
  mpz_set_str(expected,
	      "34db1d465b94b12f" "bc1c024d2c6385ff" "a52a6aeb1754a58b"
	      "b9f0ace0186cfd45" "3963e33440b88696" "513b50956ff463c6"
	      "c369830dbe9f0605" "68c796cfe29ab35e" "722af1d3f5835610"
	      "4fb7bb44d6f319d9" "1a1fcc789ab79e82" "98bac0d68187f05a"
	      "1d0c1fcc324d1e0e" "69a4653de09c7c5b" "2278b3658b95b104"
	      "bafcfe2b5f9f88e3", 16);

  test_rsa_md5(&pub, &key, expected);

  /* Generate a 2000 bit key with fixed e */
  knuth_lfib_init(&lfib, 17);

  mpz_set_ui(pub.e, 17);
  if (!rsa_generate_keypair(&pub, &key,
			    &lfib, (nettle_random_func) knuth_lfib_random,
			    NULL, verbose ? progress : NULL,
			    2000, 0))
    FAIL();

  test_rsa_key(&pub, &key);

  mpz_set_str(expected,
	      "a311f80570e2c7b3" "d4888aae13c4c29b" "aa0139339809e581"
	      "98722ca37c4a95ea" "0c94aae3b712f2a1" "ea0784a7ddea5127"
	      "ca5dd218b96a9dac" "f607a90dfcb9458e" "cebdd0c25d84d0b9"
	      "3c74ac49d678f25a" "4a4493092c2e79d2" "d3b399ec589643a6"
	      "244e7b5c202fda3a" "9de1b5224d95ddb2" "a381b7cd061a85cb"
	      "dc65af62213da9d0" "636439ee50642694" "11137ef8c2e8887f"
	      "55e795e0b0ac6eb2" "b8576973a0d8ebda" "a8ee1e4ba23e4338"
	      "15121310036920d0" "906844742e6ed25f" "b2bb3fe9caadf7ae"
	      "fcd84d53730cf570" "1c8666c60238cfd4" "2fd1b6346a7d06fa"
	      "44df010cfc7a3bca" "8cadd0cd9b68fa8b" "41204cbf8fdb6775"
	      "f92107ef036bf139" "99bf", 16);

  test_rsa_sha1(&pub, &key, expected);
  
  rsa_clear_private_key(&key);
  rsa_clear_public_key(&pub);
  mpz_clear(expected);

  SUCCESS();
  
#else /* !HAVE_LIBGMP */
  SKIP();
#endif /* !HAVE_LIBGMP */
}
