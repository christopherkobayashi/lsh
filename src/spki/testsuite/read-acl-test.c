#include "certificate.h"

#include "nettle/sexp.h"

#include <stdlib.h>

#define ASSERT(x) do { if (!(x)) abort(); } while(0)

#define LLENGTH(x) (sizeof(x) - 1)
#define LDATA(x) (sizeof(x) - 1), x

int
main(int argc, char **argv)
{
  struct spki_acl_db db;
  struct sexp_iterator i;
  
  spki_acl_init(&db);

  ASSERT(sexp_iterator_first
	 (&i, LDATA
	  ("(3:acl(7:version1:\0)(5:entry"
	   "(4:hash3:md516:xxxxxxxxxxxxxxxx)"
	   "(3:tag(3:ftp18:ftp.lysator.liu.se)))"
	   "(5:entry(10:public-key2:k1)"
	   "(3:tag(4:http32:http://www.lysator.liu.se/~nisse4:read))))")));

  ASSERT(spki_acl_parse(&db, &i));

  ASSERT(spki_principal_by_key(&db, LDATA("k1")));
  ASSERT(spki_principal_by_md5(&db, "xxxxxxxxxxxxxxxx"));

  return 0;
}
		    
			       
