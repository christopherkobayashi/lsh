/* tag.h
 *
 * Operations on SPKI "tags", i.e. authorization descriptions. */

/* libspki
 *
 * Copyright (C) 2002 Niels Möller
 *  
 * The libspki library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The libspki library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the libspki library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02111-1301, USA.
 */

#ifndef LIBSPKI_TAG_H_INCLUDED
#define LIBSPKI_TAG_H_INCLUDED

struct sexp_iterator;

#include <nettle/realloc.h>
#include <nettle/buffer.h>

struct spki_tag;

struct spki_tag *
spki_tag_compile(void *ctx, nettle_realloc_func *realloc,
		 struct sexp_iterator *i);

struct spki_tag *
spki_tag_from_sexp(void *ctx, nettle_realloc_func *realloc,
		   unsigned length,
		   const uint8_t *expr);

void
spki_tag_release(void *ctx, nettle_realloc_func *realloc,
		 struct spki_tag *tag);

/* Returns true if the requested authorization is included in the
 * delegated one. For now, star forms are recognized only in the
 * delegation, not in the request. */
int
spki_tag_includes(const struct spki_tag *delegated,
		  const struct spki_tag *request);

struct spki_tag *
spki_tag_intersect(void *ctx, nettle_realloc_func *realloc,
		   struct spki_tag *a,
		   struct spki_tag *b);

unsigned
spki_tag_format(const struct spki_tag *tag, struct nettle_buffer *buffer);

#endif /* LIBSPKI_TAG_H_INCLUDED */

