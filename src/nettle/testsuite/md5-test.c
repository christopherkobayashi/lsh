#include "testutils.h"
#include "md5.h"

int
test_main(void)
{
  test_hash(&nettle_md5, 0, "",
	    H("D41D8CD98F00B204 E9800998ECF8427E"));

  test_hash(&nettle_md5, 1, "a",
	    H("0CC175B9C0F1B6A8 31C399E269772661"));
	    
  test_hash(&nettle_md5, 3, "abc",
	    H("900150983cd24fb0 D6963F7D28E17F72"));

  test_hash(&nettle_md5, 14, "message digest",
	    H("F96B697D7CB7938D 525A2F31AAF161D0"));
  
  test_hash(&nettle_md5, 26, "abcdefghijklmnopqrstuvwxyz",
	    H("C3FCD3D76192E400 7DFB496CCA67E13B"));
  
  test_hash(&nettle_md5, 62,
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz"
	    "0123456789",
	    H("D174AB98D277D9F5 A5611C2C9F419D9F"));

  test_hash(&nettle_md5, 80,
	    "1234567890123456789012345678901234567890"
	    "1234567890123456789012345678901234567890",
	    H("57EDF4A22BE3C955 AC49DA2E2107B67A"));
#if 0
  /* Collisions, reported by Xiaoyun Wang1, Dengguo Feng2, Xuejia
     Lai3, Hongbo Yu1, http://eprint.iacr.org/2004/199 */

  test_hash(&nettle_md5,
	    HL("d131dd02 c5e6eec4 693d9a06 98aff95c 2fcab587 12467eab 4004583e b8fb7f89"
	       "55ad3406 09f4b302 83e48883 2571415a 085125e8 f7cdc99f d91dbdf2 80373c5b"),
	    H("1f160396 efc71ff4 bcff659f bf9d0fa3"));
#endif
  SUCCESS();
}
