/* client_x11.h
 *
 * Client side of X11 forwaarding.
 *
 * $id:$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2001 Niels M�ller
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

#ifndef CLIENT_X11_H_INCLUDED
#define CLIENT_X11_H_INCLUDED

struct channel_open *
make_channel_open_x11(const char *display, struct lsh_string *fake);

struct command *
make_forward_x11(const char *display, struct lsh_string *fake);
     
#endif /* CLIENT_X11_H_INCLUDED */

