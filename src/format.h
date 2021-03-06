/* format.h
 *
 * Create a packet from a format string and arguments.
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

#ifndef LSH_FORMAT_H_INCLUDED
#define LSH_FORMAT_H_INCLUDED

#include <stdarg.h>

#include "atoms.h"

/* Format strings can contain the following %-specifications:
 *
 * %%  Insert a %-sign
 *
 * %c  Insert an 8-bit character
 *
 * %i  Insert a 32-bit integer, in network byte order
 *
 * %s  Insert a string, given by a length and a pointer.
 *
 * %S  Insert a string, given as a struct lsh_string pointer.
 *
 * %z  Insert a string, using a null-terminated argument.
 *
 * %r  Reserves space in the string, first argument is the length, and
 *     the start position is stored into the second argument, a uint32_t *.
 *
 * %a  Insert a string containing one atom.
 *
 * %A  Insert a string containing a list of atoms. The input is an
 *     int_list object. Zero elements are allowed and ignored.
 *
 * %X  Insert a string containing a list of atoms. The corresponding
 *     argument sublist should be terminated with a zero. (Not used)
 *
 * %n  Insert a string containing a bignum.
 *
 * There are also some valid modifiers:
 *
 * "l" (as in literal). It is applicable to the s, a, A, n and r
 * specifiers, and outputs strings *without* a length field.
 *
 * "d" (as in decimal). For integers, convert the integer to decimal
 * digits. For strings, format the input string using sexp syntax;
 * i.e. prefixed with the length in decimal.
 *
 * "x" (as in heXadecimal). For strings, format each character as two
 * hexadecimal digits. Does not currently mean any thing for numbers.
 * Note that this modifier is orthogonal to the decimal modifier.
 * 
 * "f" (as in free). Frees the input string after it has been copied.
 * Applicable to %S only.
 *
 * "u" (as in unsigned). Used with bignums, to use unsigned-only
 * number format. */

#if DEBUG_ALLOC && __GNUC__

struct lsh_string *
ssh_format_clue(const char *clue, const char *format, ...);

#define ssh_format(format, ...) \
ssh_format_clue(__FILE__ ":" STRING_LINE, format, ## __VA_ARGS__)

#else /* !DEBUG_ALLOC */

struct lsh_string *
ssh_format(const char *format, ...);

#endif /* !DEBUG_ALLOC */

uint32_t ssh_format_length(const char *format, ...);
void
ssh_format_write(const char *format,
		      struct lsh_string *buffer, uint32_t pos, ...);

uint32_t
ssh_vformat_length(const char *format, va_list args);

void
ssh_vformat_write(const char *format,
		       struct lsh_string *buffer, uint32_t pos, va_list args);

     
/* Short cut */
#define make_string(s) (ssh_format("%lz", (s)))

unsigned
format_size_in_decimal(uint32_t n);


/* Helper functions for formatting particular ssh messages */
struct lsh_string *
format_disconnect(int code, const char *msg, 
		  const char *language);

struct lsh_string *
format_unimplemented(uint32_t seqno);


#endif /* LSH_FORMAT_H_INCLUDED */
