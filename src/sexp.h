/* sexp.h
 *
 * An implementation of Ron Rivest's S-expressions, used in spki.
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

#ifndef LSH_SEXP_H_INCLUDED
#define LSH_SEXP_H_INCLUDED

#include "abstract_io.h"

/* CLASS:
   (class
     (name sexp)
     (vars
       (output method int "int style" "struct abstract_write output")))
*/

/* CLASS:
   (class
     (name sexp_string)
     (super sexp)
     (vars
       (display string)
       (contents string)))
*/

/* CLASS:
   (class
     (name sexp_atom)
     (super sexp)
     (vars
       (atom . int)))
*/

/* CLASS:
   (class
     (name "sexp_cons")
     (super sexp)
     (vars
       (car object sexp)
       (cdr object sex_cons)))
*/

/* Creating sexps */
/* atom->sexp */
struct sexp *sexp_a(int a);

/* cstring->sexp */
struct sexp *sexp_z(char *s);

/* mpz->atom */
struct sexp *sexp_n(mpz_t n);
struct sexp *sexp_sn(mpz_t n);

/* cons */
struct sexp *sexp_c(struct sexp *car, struct sexp_cons *cdr);

/* list */
struct sexp *sexp_l(unsigned n, ...);

/* Extracting information from sexp. These functions accept NULL
 * arguments, and return NULL if the conversion is not possible */

int *sexp_consp(struct sexp *e);

/* For lists */
struct sexp *sexp_car(struct sexp *e);
struct sexp *sexp_cdr(struct sexp *e);

int sexp *sexp_null_cdr(struct sexp *e);

struct lsh_string *sexp_contents(struct sexp *e);
struct lsh_string *sexp_display(struct sexp *e);
int sexp_atom(struct sexp *e);
int sexp_bignum_u(struct sexp *e, mpz_t n);
int sexp_bignum_s(struct sexp *e, mpz_t n);


/* Parsing sexp */

/* CLASS:
   (class
     (name sexp_handler)
     (vars
       (method int "struct sexp *e")))
*/

struct read_handler make_read_sexp(struct sexp_handler *h)

#endif /* LSH_SEXP_H_INCLUDED */

 
