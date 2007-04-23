#include "testutils.h"

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#if HAVE_LIBGMP
#include "bignum.h"

static void
test_bignum(const char *hex, unsigned length, const uint8_t *base256)
{
  mpz_t a;
  mpz_t b;
  uint8_t *buf;
  
  mpz_init_set_str(a, hex, 16);
  nettle_mpz_init_set_str_256_s(b, length, base256);

  if (mpz_cmp(a, b))
    FAIL();

  buf = xalloc(length + 1);
  memset(buf, 17, length + 1);

  nettle_mpz_get_str_256(length, buf, a);
  if (!MEMEQ(length, buf, base256))
    FAIL();

  if (buf[length] != 17)
    FAIL();

  mpz_clear(a); mpz_clear(b);
  free(buf);
}

static void
test_size(long x, unsigned size)
{
  mpz_t t;

  mpz_init_set_si(t, x);
  ASSERT(nettle_mpz_sizeinbase_256_s(t) == size);
  mpz_clear(t);
}
#endif /* HAVE_LIBGMP */


int
test_main(void)
{
#if HAVE_LIBGMP
  test_size(0, 1);
  test_size(1, 1);
  test_size(0x7f, 1);
  test_size(0x80, 2);
  test_size(0x81, 2);
  test_size(0xff, 2);
  test_size(0x100, 2);
  test_size(0x101, 2);
  test_size(0x1111, 2);
  test_size(0x7fff, 2);
  test_size(0x8000, 3);
  test_size(0x8001, 3);

  test_size(-      1, 1); /*     ff */
  test_size(-   0x7f, 1); /*     81 */
  test_size(-   0x80, 1); /*     80 */
  test_size(-   0x81, 2); /*   ff7f */
  test_size(-   0xff, 2); /*   ff01 */
  test_size(-  0x100, 2); /*   ff00 */
  test_size(-  0x101, 2); /*   feff */
  test_size(- 0x1111, 2); /*   eeef */
  test_size(- 0x7fff, 2); /*   8001 */
  test_size(- 0x8000, 2); /*   8000 */
  test_size(- 0x8001, 3); /* ff7fff */

  test_bignum("0", HL("00"));
  test_bignum("010203040506", HL("010203040506"));
  test_bignum("80010203040506", HL("0080010203040506"));

  test_bignum(   "-1", HL(    "ff"));
  test_bignum(  "-7f", HL(    "81"));
  test_bignum(  "-80", HL(    "80"));
  test_bignum(  "-81", HL(  "ff7f"));
  test_bignum("-7fff", HL(  "8001"));
  test_bignum("-8000", HL(  "8000"));
  test_bignum("-8001", HL("ff7fff"));
  
  SUCCESS();
#else /* !HAVE_LIBGMP */
  SKIP();
#endif /* !HAVE_LIBGMP */
}