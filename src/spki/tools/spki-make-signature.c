/* spki-make-signature.c */

/* libspki
 *
 * Copyright (C) 2003 Niels M�ller
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

#include "certificate.h"
#include "parse.h"

#include "io.h"

#include <nettle/rsa.h>

#include <stdarg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getopt.h"

static void
die(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

static void
usage(void)
{
  fprintf(stderr,
	  "Usage: spki-make-signature [OPTION...] KEY-FILE.\n\n"
	  "  --digest        Read a digest, instead if a file to be hashed,\n"
	  "                  from stdin.\n"
	  "  --seed-file     A yarrow seed file to use.\n");
  exit(EXIT_FAILURE);
}

struct sign_options
{
  const char *key_file;
  const char *seed_file;
  int digest_mode;
};

static void
parse_options(struct sign_options *o,
	      int argc, char **argv)
{
  o->key_file = NULL;
  o->seed_file = NULL;
  o->digest_mode = 0;
  
  for (;;)
    {
      static const struct option options[] =
	{
	  /* Name, args, flag, val */
	  { "digest", no_argument, NULL, 'd' },
	  { "seed-file", required_argument, NULL, 's' },
	  { NULL, 0, NULL, 0 }
	};

      int c;
      int option_index = 0;
     
      c = getopt_long(argc, argv, "V?s:w:", options, &option_index);
    
      switch (c)
	{
	default:
	  abort();
	
	case -1:
	  if (optind == argc)
	    die("spki-make-signature: No key-file given.\n");

	  o->key_file = argv[optind++];
	  if (optind != argc)
	    die("spki-make-signature: Too many arguments.\n");

	  return;

	case 's':
	  o->seed_file = optarg;
	  break;

	case 'd':
	  o->digest_mode = 1;
	  break;

	case '?':
	  usage();
	}
    }
}

static void *
xalloc(size_t size)
{
  void *p = malloc(size);
  if (!p)
    die("Virtual memory exhausted.\n");
  return p;
}

int
main(int argc, char **argv)
{
  struct sign_options o;
  struct spki_iterator i;
  struct rsa_public_key pub;
  struct rsa_private_key priv;
  const struct nettle_hash *hash_algorithm = NULL;
  
  char *key;
  unsigned key_length;
  enum spki_type type;
  
  char *digest;

  mpz_t s;
  struct nettle_buffer buffer;
  
  parse_options(&o, argc, argv);

  key_length = read_file_by_name(o.key_file, 0, &key);

  if (!key_length)
    die("Failed to read key-file `%s'\n", o.key_file);

  if (! (spki_transport_iterator_first(&i, key_length, key)
	 && spki_check_type(&i, SPKI_TYPE_PRIVATE_KEY)))
    die("Invalid private key.\n");

  switch ( (type = i.type) )
    {
    case SPKI_TYPE_RSA_PKCS1_MD5:
      hash_algorithm = &nettle_md5;
      break;

    case SPKI_TYPE_RSA_PKCS1_SHA1:
      hash_algorithm = &nettle_sha1;
      break;

    default:
      die("Unsupported key type.\n");
    }

  rsa_public_key_init(&pub);
  rsa_private_key_init(&priv);

  if (!rsa_keypair_from_sexp_alist(&pub, &priv, 0, &i.sexp))
    die("Invalid RSA key.\n");

  if (o.digest_mode)
    {
      unsigned digest_length = read_file(stdin,
					 hash_algorithm->digest_size + 1,
					 &digest);
       if (digest_length != hash_algorithm->digest_size)
	 die("Unexpected size of input digest.\n");
     }
   else
     {
       void *ctx = xalloc(hash_algorithm->context_size);
       digest = xalloc(hash_algorithm->digest_size);

       hash_algorithm->init(ctx);
       hash_file(hash_algorithm, ctx, stdin);
       hash_algorithm->digest(ctx, hash_algorithm->digest_size, digest);
     }

   mpz_init(s);

   if (hash_algorithm == &nettle_md5)
     rsa_md5_sign_digest(&priv, digest, s);
   else if (hash_algorithm == &nettle_sha1)
     rsa_sha1_sign_digest(&priv, digest, s);
   else
     die("Internal error.\n");

   nettle_buffer_init_realloc(&buffer, NULL, nettle_xrealloc);
   sexp_format(&buffer,
	       "(signature(hash %0s%s)(public-key(%s (n%b)(e%b)))(%s%b))",
	       hash_algorithm->name, hash_algorithm->digest_size, digest,
	       spki_type_names[type].length, spki_type_names[type].name,
	       pub.n, pub.e,
	       spki_type_names[type].length, spki_type_names[type].name,
	       s);

  mpz_clear(s);
  rsa_public_key_clear(&pub);
  rsa_private_key_clear(&priv);
  
  if (!write_file(stdout, buffer.size, buffer.contents))
    die("Writing signature failed.\n");

  nettle_buffer_clear(&buffer);

  return EXIT_SUCCESS;
}
