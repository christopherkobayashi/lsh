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
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "getopt.h"

#include "alist.h"
#include "atoms.h"
#include "channel.h"
#include "charset.h"
#include "client.h"
#include "client_keyexchange.h"
#include "crypto.h"
#include "format.h"
#include "io.h"
#include "randomness.h"
#include "service.h"
/* #include "session.h" */
#include "ssh.h"
#include "userauth.h"
#include "werror.h"
#include "xalloc.h"

/* Block size for stdout and stderr buffers */
#define BLOCK_SIZE 32768

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
  struct fake_host_db *res;

  NEW(res);

  res->super.lookup = do_host_lookup;
  res->algorithm = a;

  return &res->super;
}

int main(int argc, char **argv)
{
  char *host = NULL;
  char *port = "ssh";
  int option;
  char *user;
  
  struct sockaddr_in remote;

  /* STATIC, because the object exists at gc time */
  struct io_backend backend = { STATIC_HEADER };

  struct lsh_string *random_seed;
  struct randomness *r;
  struct diffie_hellman_method *dh;
  struct keyexchange_algorithm *kex;
  struct alist *algorithms;
  struct make_kexinit *make_kexinit;
  struct packet_handler *kexinit_handler;
  struct lookup_verifier *lookup;
  struct ssh_service *service;

  int in, out, err;
  
  /* For filtering messages. Could perhaps also be used when converting
   * strings to and from UTF8. */
  setlocale(LC_CTYPE, "");
  /* FIXME: Choose character set depending on the locale */
  set_local_charset(CHARSET_LATIN1);
  
  while((option = getopt(argc, argv, "dl:p:qv")) != -1)
    switch(option)
      {
      case 'p':
	port = optarg;
	break;
      case 'l':
	user = optarg;
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
  if (!user)
      user = getenv("LOGNAME");

  if (!user)
    {
      werror("lsh: No user name.\n"
	     "Please use the -l option, or set LOGNAME in the environment\n");
      exit(EXIT_FAILURE);
    }

  if (!get_inaddr(&remote, host, port, "tcp"))
    {
      fprintf(stderr, "No such host or service\n");
      exit(1);
    }

  init_backend(&backend);
  
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

  /* Dup stdio file descriptors, so that they can be closed without
   * confusing the c library. */
  
  if ( (in = dup(STDIN_FILENO) < 0))
    {
      werror("Can't dup stdin: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
  if ( (out = dup(STDOUT_FILENO) < 0))
    {
      werror("Can't dup stdout: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
  if ( (err = dup(STDERR_FILENO) < 0))
    {
      werror("Can't dup stderr: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
  
  service = make_connection_service
    (make_alist(0, -1),
     make_alist(0, -1),
     make_client_startup(io_read(&backend, in, NULL, NULL),
			 io_write(&backend, out, BLOCK_SIZE, NULL),
			 io_write(&backend, err, BLOCK_SIZE, NULL),
			 ATOM_SHELL, ssh_format("")));
  
  kexinit_handler = make_kexinit_handler
    (CONNECTION_CLIENT,
     make_kexinit, algorithms,
     request_service(ATOM_SSH_USERAUTH, 
		     make_client_userauth(ssh_format("%lz", user),
					  ATOM_SSH_CONNECTION,
					  service)));
  
  if (!io_connect(&backend, &remote, NULL,
		  make_client_callback(&backend,
				       "lsh - a free ssh",
				       SSH_MAX_PACKET,
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

  
