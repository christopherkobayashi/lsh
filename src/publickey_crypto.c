/* publickey_crypto.c
 *
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "publickey_crypto.h"

#include "atoms.h"
#include "bignum.h"
#include "connection.h"
#include "crypto.h"
#include "format.h"
#include "parse.h"
#include "sha.h"
#include "ssh.h"
#include "werror.h"
#include "xalloc.h"

#include <assert.h>

#define GABA_DEFINE
#include "publickey_crypto.h.x"
#undef GABA_DEFINE

/* #include "publickey_crypto.c.x" */

struct keypair *
make_keypair(UINT32 type,
	     struct lsh_string *public,
	     struct signer *private)
{
  NEW(keypair, self);
  
  self->type = type;
  self->public = public;
  self->private = private;
  return self;
}

static int
zn_range(struct abstract_group *c, mpz_t x)
{
  CAST(group_zn, closure, c);

  /* FIXME: As we are really working in a cyclic subgroup, we should
   * also try raising the element to the group order and check that we
   * get 1. Without that test, some numbers in the range [1, modulo-1]
   * will pass as members even if they are not generated by g. */
  return ( (mpz_sgn(x) == 1) && (mpz_cmp(x, closure->modulo) < 0) );
}

#if 0
static int
zn_member(struct abstract_group *c, mpz_t x)
{
  if (zn_range(c, x))
    {
      CAST(group_zn, closure, c);
      mpz_t t;
      int res;
      
      mpz_init(t);

      mpz_powm(t, x, closure->order, closure->modulo);
      res = !mpz_cmp_ui(t, 1);

      mpz_clear(t);

      return res;
    }
  return 0;
}
#endif

static void
zn_invert(struct abstract_group *c, mpz_t res, mpz_t x)
{
  CAST(group_zn, closure, c);

  if (!mpz_invert(res, x, closure->modulo))
    fatal("zn_invert: element is non-invertible\n");

  mpz_fdiv_r(res, res, closure->modulo);
}

static void
zn_combine(struct abstract_group *c, mpz_t res, mpz_t a, mpz_t b)
{
  CAST(group_zn, closure, c);

  mpz_mul(res, a, b);
  mpz_fdiv_r(res, res, closure->modulo);
}

static void
zn_power(struct abstract_group *c, mpz_t res, mpz_t g, mpz_t e)
{
  CAST(group_zn, closure, c);

  mpz_powm(res, g, e, closure->modulo);
}

static void
zn_small_power(struct abstract_group *c, mpz_t res, mpz_t g, UINT32 e)
{
  CAST(group_zn, closure, c);

  mpz_powm_ui(res, g, e, closure->modulo);
}

/* Assumes p is a prime number */
struct group_zn *
make_zn(mpz_t p, mpz_t g, mpz_t order)
{
  NEW(group_zn, res);

  res->super.range = zn_range;
  res->super.invert = zn_invert;
  res->super.combine = zn_combine;
  res->super.power = zn_power;     /* Pretty Mutation! Magical Recall! */
  res->super.small_power = zn_small_power;
  
  mpz_init_set(res->modulo, p);
  mpz_init_set(res->super.generator, g);
  mpz_init_set(res->super.order, order);

  return res;
}

/* These are not really operations on the group, but they are needed
 * for SRP. */
void
zn_ring_add(struct abstract_group *s,
	    mpz_t res, mpz_t a, mpz_t b)
{
  CAST(group_zn, self, s);
  mpz_add(res, a, b);
  mpz_fdiv_r(res, res, self->modulo);
}

void
zn_ring_subtract(struct abstract_group *s,
		 mpz_t res, mpz_t a, mpz_t b)
{
  CAST(group_zn, self, s);
  mpz_sub(res, a, b);
  mpz_fdiv_r(res, res, self->modulo);
}
