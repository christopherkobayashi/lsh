/* abstract_compress.h
 *
 * interface to compression algorithms...
 * 
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998 Balazs Scheidler, Niels M�ller
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

#ifndef LSH_ABSTRACT_COMPRESS_H_INCLUDED
#define LSH_ABSTRACT_COMPRESS_H_INCLUDED

#include "lsh_types.h"

#define CLASS_DECLARE
#include "abstract_compress.h.x"
#undef CLASS_DECLARE

/* CLASS:
    (class 
       (name compress_instance)
       (vars
         (codec method (string)
         		"struct lsh_string *data" "int free")))
*/

#define CODEC(instance, packet, free) \
((instance)->codec((instance), (packet), (free)))

#define COMPRESS_DEFLATE 0
#define COMPRESS_INFLATE 1

/* CLASS:
   (class
     (name compress_algorithm)
     (vars
        (make_compress method (object compress_instance)
        		"int mode")))
*/

#define MAKE_CODEC(algo, mode) \
((algo)->make_compress((algo), (mode)))

#define MAKE_DEFLATE(algo) MAKE_CODEC((algo), COMPRESS_DEFLATE)
#define MAKE_INFLATE(algo) MAKE_CODEC((algo), COMPRESS_INFLATE)

#endif
