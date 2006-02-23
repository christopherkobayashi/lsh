/* atoms.c
 *
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "atoms.h"


struct atom_rassoc
{
  const uint8_t *name;
  uint32_t length;
};

const struct atom_assoc *
gperf_atom (const char *str, unsigned int len);

#include "atoms_gperf.c"

struct atom_rassoc atom_table[] =
#include "atoms_table.c"
;

uint32_t get_atom_length(int atom)
{ return atom_table[atom].length; }

const uint8_t *get_atom_name(int atom)
{ return atom_table[atom].name; }
  
enum lsh_atom
lookup_atom(uint32_t length, const uint8_t *name)
{
  /* NOTE: The automatically generated code uses const char *, and
   * some compilers signal a fatal error on char * / unsigned char *
   * mismatch. */
  const struct atom_assoc *pair = gperf_atom( (const char *) name, length);

  return pair ? pair->id : 0;
}
