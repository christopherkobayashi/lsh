/* service.h
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

#warning service.h is obsolete
#ifndef LSH_SERVICE_H_INCLUDED
#define LSH_SERVICE_H_INCLUDED

#include "alist.h"
#include "connection.h"


struct lsh_string *format_service_request(int name);
struct lsh_string *format_service_accept(int name);

#endif /* LSH_SERVICE_H_INCLUDED */
