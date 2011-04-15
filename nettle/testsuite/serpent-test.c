#include "testutils.h"
#include "serpent.h"

static uint8_t *
decode_hex_reverse (const char *hex)
{
  unsigned length = decode_hex_length (hex);
  uint8_t *p = xalloc(length);
  unsigned i;

  decode_hex(p, hex);

  for (i = 0; i < (length+1)/2; i++)
    {
      uint8_t t = p[i];
      p[i] = p[length - 1 - i];
      p[length - 1 - i] = t;
    }
  return p;
}

#define RH(x) decode_hex_reverse(x)
#define RHL(x) decode_hex_length(x), decode_hex_reverse(x)

int
test_main(void)
{
  /* The first test for each key size from the ecb_vk.txt and ecb_vt.txt
   * files in the serpent package. */

  /* NOTE: These vectors uses strange byte-reversed order of inputs
     and outputs. */
  /* 128 bit key */

  /* vk, 1 */
  test_cipher(&nettle_serpent128,
	      RHL("8000000000000000 0000000000000000"),
	      RHL("0000000000000000 0000000000000000"),
	      RH("49AFBFAD9D5A3405 2CD8FFA5986BD2DD"));

  /* vt, 1 */
  test_cipher(&nettle_serpent128,
	      RHL("0000000000000000 0000000000000000"),
	      RHL("8000000000000000 0000000000000000"),
	      RH("10B5FFB720B8CB90 02A1142B0BA2E94A"));

  /* 192 bit key */

  /* vk, 1 */
  test_cipher(&nettle_serpent192,
	      RHL("8000000000000000 0000000000000000"
		 "0000000000000000"),
	      RHL("0000000000000000 0000000000000000"),
	      RH("E78E5402C7195568 AC3678F7A3F60C66"));

  /* vt, 1 */
  test_cipher(&nettle_serpent192,
	      RHL("0000000000000000 0000000000000000"
		 "0000000000000000"),
	      RHL("8000000000000000 0000000000000000"),
	      RH("B10B271BA25257E1 294F2B51F076D0D9"));

  /* 256 bit key */

  /* vk, 1 */
  test_cipher(&nettle_serpent256,
	      RHL("8000000000000000 0000000000000000"
		 "0000000000000000 0000000000000000"),
	      RHL("0000000000000000 0000000000000000"),
	      RH("ABED96E766BF28CB C0EBD21A82EF0819"));

  /* vt, 1 */
  test_cipher(&nettle_serpent256,
	      RHL("0000000000000000 0000000000000000"
		 "0000000000000000 0000000000000000"),
	      RHL("8000000000000000 0000000000000000"),
	      RH("DA5A7992B1B4AE6F 8C004BC8A7DE5520"));

  /* Test vectors from
     http://www.cs.technion.ac.il/~biham/Reports/Serpent/ */

  /* serpent128 */
  /* Set 4, vector#  0 */
  test_cipher(&nettle_serpent128,
	      HL("000102030405060708090A0B0C0D0E0F"),
	      HL("00112233445566778899AABBCCDDEEFF"),
	      H("563E2CF8740A27C164804560391E9B27"));

  /* Set 4, vector#  1 */
  test_cipher(&nettle_serpent128,
	      HL("2BD6459F82C5B300952C49104881FF48"),
	      HL("EA024714AD5C4D84EA024714AD5C4D84"),
	      H("92D7F8EF2C36C53409F275902F06539F"));

  /* serpent192 */
  /* Set 4, vector#  0 */
  test_cipher(&nettle_serpent192,
	      HL("000102030405060708090A0B0C0D0E0F1011121314151617"),
	      HL("00112233445566778899AABBCCDDEEFF"),
	      H("6AB816C82DE53B93005008AFA2246A02"));

  /* Set 4, vector#  1 */
  test_cipher(&nettle_serpent192,
	      HL("2BD6459F82C5B300952C49104881FF482BD6459F82C5B300"),
	      HL("EA024714AD5C4D84EA024714AD5C4D84"),
	      H("827B18C2678A239DFC5512842000E204"));

  /* serpent256 */
  /* Set 4, vector#  0 */
  test_cipher(&nettle_serpent256,
	      HL("000102030405060708090A0B0C0D0E0F"
		 "101112131415161718191A1B1C1D1E1F"),
	      HL("00112233445566778899AABBCCDDEEFF"),
	      H("2868B7A2D28ECD5E4FDEFAC3C4330074"));

  /* Set 4, vector#  1 */
  test_cipher(&nettle_serpent256,
	      HL("2BD6459F82C5B300952C49104881FF48"
		 "2BD6459F82C5B300952C49104881FF48"),
	      HL("EA024714AD5C4D84EA024714AD5C4D84"),
	      H("3E507730776B93FDEA661235E1DD99F0"));

  SUCCESS();
}
