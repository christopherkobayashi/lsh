/* lsh.c
 *
 * client main program
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998 Niels M�ller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

#include "getopt.h"

#include "alist.h"
#include "atoms.h"
#include "client.h"
#include "client_keyexchange.h"
#include "crypto.h"
#include "format.h"
#include "io.h"
#include "randomness.h"
#include "werror.h"
#include "xalloc.h"

#define BLOCK_SIZE 32768

/* Global variable */
struct io_backend backend;

void usage(void) NORETURN;

void usage(void)
{
  exit(1);
}

struct fake_host_db
{
  struct lookup_verifier super;

  struct signature_algorithm *algorithm;
};

static struct verifier *do_host_lookup(struct lookup_verifier *c,
				       struct lsh_string *key)
{
  struct fake_host_db *closure = (struct fake_host_db *) c;

  MDEBUG(closure);
  
  return MAKE_VERIFIER(closure->algorithm, key->length, key->data);
}

static struct lookup_verifier *make_fake_host_db(struct signature_algorithm *a)
{
  struct fake_host_db *res = xalloc(sizeof(struct fake_host_db));

  res->super.lookup = do_host_lookup;
  res->algorithm = a;

  return &res->super;
}

int main(int argc, char **argv)
{
  char *host = NULL;
  char *port = "ssh";
  int option;

  struct sockaddr_in remote;

  struct lsh_string *random_seed;
  struct randomness *r;
  struct diffie_hellman_method *dh;
  struct keyexchange_algorithm *kex;
  struct alist *algorithms;
  struct make_kexinit *make_kexinit;
  struct packet_handler *kexinit_handler;
  struct lookup_verifier *lookup;
  
  /* For filtering messages. Could perhaps also be used when converting
   * strings to and from UTF8. */
  setlocale(LC_CTYPE, "");
  
  while((option = getopt(argc, argv, "dp:qv")) != -1)
    switch(option)
      {
      case 'p':
	port = optarg;
	break;
      case 'q':
	quiet_flag = 1;
	break;
      case 'd':
	debug_flag = 1;
	break;
      case 'v':
	verbose_flag = 1;
	break;
      default:
	usage();
      }

  if ( (argc - optind) < 1)
    usage();

  host = argv[optind];

  random_seed = ssh_format("%z", "gazonk");
  r = make_poor_random(&sha_algorithm, random_seed);
  dh = make_dh1(r);
  /* No randomness is needed for verifying signatures */
  lookup = make_fake_host_db(make_dss_algorithm(NULL)); 
  kex = make_dh_client(dh, lookup);
  algorithms = make_alist(4,
			  ATOM_ARCFOUR, crypto_rc4_algorithm,
			  ATOM_HMAC_SHA1, make_hmac_algorithm(&sha_algorithm),
			  ATOM_DIFFIE_HELLMAN_GROUP1_SHA1, kex,
			  ATOM_SSH_DSS, make_dss_algorithm(r), -1);
  make_kexinit = make_test_kexinit(r);
  kexinit_handler = make_kexinit_handler(CONNECTION_CLIENT,
					 make_kexinit, algorithms);

  if (!get_inaddr(&remote, host, port, "tcp"))
    {
      fprintf(stderr, "No such host or service\n");
      exit(1);
    }

  if (!io_connect(&backend, &remote, NULL,
		  make_client_callback(&backend,
				       "lsh - a free ssh",
				       BLOCK_SIZE,
				       r, make_kexinit,
				       kexinit_handler)))
    {
      werror("lsh: Connection failed: %s\n", strerror(errno));
      return 1;
    }
  
  lsh_string_free(random_seed);
  
  io_run(&backend);

  return 0;
}

  
