/* spki.h
 *
 * An implementation of SPKI certificate checking
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1999 Balazs Scheidler
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

#ifndef LSH_SPKI_H_INCLUDED
#define LSH_SPKI_H_INCLUDED

#include "exception.h"
#include "sexp.h"

#define GABA_DECLARE
#include "sexp.h.x"
#undef GABA_DECLARE

/* GABA:
   (class
     (name spki_exception)
     (super exception)
     (vars
       (expr object sexp)))
*/

struct exception *
make_spki_exception(UINT32 type, const char *msg, struct sexp *expr);

/* FIXME: should support keyblobs other than ssh-dss */
struct sexp *keyblob2spki(struct lsh_string *keyblob);

extern struct command spki_public2private;

#endif /* LSH_SPKI_H_INCLUDED */
