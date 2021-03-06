/* atoms.h
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998 Niels Möller
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301  USA
 */

#ifndef LSH_ATOMS_H_INCLUDED
#define LSH_ATOMS_H_INCLUDED

#include "lsh.h"

#include "atoms_defines.h"

/* Atoms are represented as plain (small) ints. Zero is used for all
 * atoms we don't know about. */

uint32_t get_atom_length(enum lsh_atom atom);
const uint8_t *get_atom_name(enum lsh_atom atom);
enum lsh_atom lookup_atom(uint32_t length, const uint8_t *name);

/* FIXME: Often used with constants, then we could replace the
   function calls with constants. */   
#define ATOM_LD(x) get_atom_length((x)), get_atom_name((x))

#endif /* LSH_ATOMS_H_INCLUDED */
