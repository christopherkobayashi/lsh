/* werror.h
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef LSH_ERROR_H_INCLUDED
#define LSH_ERROR_H_INCLUDED

#include "lsh_types.h"

/* Global variables */
extern int debug_flag;
extern int quiet_flag;
extern int verbose_flag;

void werror(char *format, ...) PRINTF_STYLE(1,2);
void debug(char *format, ...) PRINTF_STYLE(1,2);
void verbose(char *format, ...) PRINTF_STYLE(1,2);

/* For outputting data recieved from the other end */
void werror_safe(UINT32 length, UINT8 *msg);
void debug_safe(UINT32 length, UINT8 *msg);
void verbose_safe(UINT32 length, UINT8 *msg);

void werror_safe_utf8(UINT32 length, UINT8 *msg);

void fatal(char *format, ...) PRINTF_STYLE(1,2) NORETURN;

#endif /* LSH_ERROR_H_INCLUDED */
