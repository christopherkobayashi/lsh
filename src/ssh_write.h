/* ssh_write.h
 *
 */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2005 Niels M�ller
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

/* GABA:
   (class
     (name ssh_write_state)
     (vars
       ; Number of bytes of the first string that have already been written
       (done . uint32_t)
       (q struct string_queue)))
*/

void
init_ssh_write_state(struct ssh_write_state *self);

struct ssh_write_state *
make_ssh_write_state(void);

/* Returns 1 on success, 0 on EWOULDBLOCK, and -1 on error */
int
ssh_write_data(struct ssh_write_state *self,
	       oop_source *source, int fd,
	       struct lsh_string *data);
