/* pkcs5-test.c
 *
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

/* FIXME: In which include file can getopt be found? Solaris man page
 * says stdlib.h, linux's says unistd.h. */
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

#include "crypto.h"

static void
usage(void)
{
  fprintf(stderr, "pkcs5-test [-i iterations] [-s salt] [-l key length] password");
  exit(1);
}

static void
print_hex(unsigned length, const char *data)
{
  static const char digits[16] = "0123456789ABCDEF";
  unsigned i;

  for (i = 0; i<length; i++)
    {
      if (!(i%8))
	putchar(' ');

      putchar(digits[(data[i]/0x10) & 0xf]);
      putchar(digits[data[i] & 0xf]);
    }
}

int main(int argc, char **argv)
{
  int iterations = 10;
  char *salt = "pepper";
  int length = 32;
  char *password;
  int c;

  char *key;

  enum { OPT_HELP = 300 };
  static const struct option options[] =
    {
      /* Name, args, flag, val */
      { "help", no_argument, NULL, OPT_HELP },
      { NULL, 0, NULL, 0 }
    };  
  while ((c = getopt_long(argc, argv, "i:s:l:", options, NULL)) != -1)
    {
      switch (c)
	{
	case 'i':
	  iterations = atoi(optarg);
	  if ( (iterations < 1) || (iterations > 10000000))
	    usage();
	  break;
	case 's':
	  salt = optarg;
	  break;
	case 'l':
	  length = atoi(optarg);
	  if ( (length < 1) || (length > 5000) )
	    usage();
	  break;
	case OPT_HELP:
	  usage();
	  return EXIT_SUCCESS;
	case '?':
	  return EXIT_FAILURE;
	default:
	  abort();
	}
    }

  if (optind != (argc - 1))
    usage();

  password = argv[optind];
  
  key = alloca(length);
  
  pkcs5_derive_key(make_hmac_algorithm(&nettle_sha1),
		   strlen(password), password,
		   strlen(salt), salt,
		   iterations,
		   length, key);

  printf("Key:");
  print_hex(length, key);
  printf("\n");

  return 0;
}

