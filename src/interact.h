/* interact.h
 *
 * Interact with the user.
 *
 * $Id$*/

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1999 Niels M�ller
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

#ifndef LSH_INTERACT_H_INCLUDED
#define LSH_INTERACT_H_INCLUDED

#include "lsh.h"

/* Forward declaration */
struct abstract_interact;
struct terminal_dimensions;

#define GABA_DECLARE
#include "interact.h.x"
#undef GABA_DECLARE

/* Abstract class defining methods needed to communicate with the
 * user's terminal. */

struct terminal_dimensions
{
  UINT32 char_width;
  UINT32 char_height;
  UINT32 pixel_width;
  UINT32 pixel_height;
};

/* GABA:
   (class
     (name window_change_callback)
     (vars
       (f method void "struct abstract_interact *i")))
*/

#define WINDOW_CHANGE_CALLBACK(c, i) ((c)->f((c), (i)))

/* GABA:
   (class
     (name abstract_interact)
     (vars
       (is_tty method int)
       ; (read_line method int "UINT32 size" "UINT8 *buffer")
       (read_password method (string)
                  "UINT32 max_length"
                  "struct lsh_string *prompt"
		  "int free")
       (yes_or_no method int
                  "struct lsh_string *prompt"
		  "int def" "int free")
       (window_size method int "struct terminal_dimensions *")
       (window_change_subscribe method (object resource)
		  "struct window_change_callback *callback")))
*/

#define INTERACT_IS_TTY(i) \
  ((i)->is_tty((i)))
#define INTERACT_READ_PASSWORD(i, l, p, f) \
  ((i)->read_password((i), (l), (p), (f)))
#define INTERACT_YES_OR_NO(i, p, d, f) \
  ((i)->yes_or_no((i), (p), (d), (f)))
#define INTERACT_WINDOW_SIZE(i, d) \
  ((i)->window_size((i), (d)))
#define INTERACT_WINDOW_SUBSCRIBE(i, c) \
  ((i)->window_change_subscribe((i), (c)))

extern int tty_fd;

int lsh_open_tty(void);
int tty_read_line(UINT32 size, UINT8 *buffer);
int yes_or_no(struct lsh_string *s, int def, int free);

#endif /* LSH_INTERACT_H_INCLUDED */
