/* ssh_read.h
 *
 * Fairly general liboop-based packer reader.
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

#ifndef LSH_SSH_READ_H_INCLUDED
#define LSH_SSH_READ_H_INCLUDED

#include <oop.h>

#include "lsh.h"

struct ssh_read_state;

#define GABA_DECLARE
# include "ssh_read.h.x"
#undef GABA_DECLARE

/* GABA:
   (class
     (name ssh_read_state)
     (vars
       ; A callback is installed iff both STATE and ACTIVE are non-null
       (state . "oop_call_fd *")
       (active . int)

       (pos . uint32_t)

       ; Fix buffer space of size SSH_MAX_BLOCK_SIZE       
       (header string)
       ; Current header length
       (header_length . uint32_t);
       
       ; The line or packet being read
       (data string)

       ; Called when header is read. If it returns non-NULL, the
       ; reader goes into packet-reading mode. In this case, this
       ; method is expected to also initialize self->pos properly.
       (process_header method "struct lsh_string *")

       ; Called for each complete line or packet, and with NULL at EOF.
       (handle_data method void "struct lsh_string *")
       ; Called with the errno value 
       (io_error method void "int error")))
*/  

void
ssh_read_stop(struct ssh_read_state *self, oop_source *source, int fd);

void
ssh_read_start(struct ssh_read_state *self, oop_source *source, int fd);

void
ssh_read_line(struct ssh_read_state *self, uint32_t max_length,
	      oop_source *source, int fd,
	      void (*handle_data)
	        (struct ssh_read_state *state, struct lsh_string *packet));

void
ssh_read_packet(struct ssh_read_state *self,
		oop_source *source, int fd,
		void (*handle_data)
		  (struct ssh_read_state *state, struct lsh_string *packet));

void
init_ssh_read_state(struct ssh_read_state *state,
		    uint32_t max_header, uint32_t header_length,
		    struct lsh_string * (*process_header)
		      (struct ssh_read_state *state),
		    void (*io_error)(struct ssh_read_state *state, int error));

struct ssh_read_state *
make_ssh_read_state(uint32_t max_header, uint32_t header_length,
		    struct lsh_string * (*process_header)
		      (struct ssh_read_state *state),
		    void (*io_error)(struct ssh_read_state *state, int error));

struct lsh_string *
service_process_header(struct ssh_read_state *state);

#endif /* LSH_SSH_READ_H_INCLUDED */
