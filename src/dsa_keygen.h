/* dsa_keygen.h
 *
 * Generate dsa key pairs..
 *
 * $Id$
 */

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

#ifndef LSH_DSS_KEYGEN_H_INCLUDED
#define LSH_DSS_KEYGEN_H_INCLUDED

#include "bignum.h"

void dsa_nist_gen(mpz_t p, mpz_t q, struct randomness *r, unsigned l);
void dsa_find_generator(mpz_t g, struct randomness *r, mpz_t p, mpz_t q);

#endif /* LSH_DSS_KEYGEN_H_INCLUDED */
