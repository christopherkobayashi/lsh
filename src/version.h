/* version.h
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

#ifndef LSH_VERSION_H_INCLUDED
#define LSH_VERSION_H_INCLUDED

#define SOFTWARE_SLOGAN "a GNU ssh"

#define SERVER_VERSION_LINE ("SSH-2.0-lshd-" PACKAGE_VERSION " " SOFTWARE_SLOGAN)
#define CLIENT_VERSION_LINE ("SSH-2.0-lsh-transport-" PACKAGE_VERSION " " SOFTWARE_SLOGAN)

#define BUG_ADDRESS "<bug-lsh@gnu.org>"

#endif /* LSH_VERSION_H_INCLUDED */
