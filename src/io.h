/* io.h
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef LSH_IO_H_INCLUDED
#define LSH_IO_H_INCLUDED

#include "abstract_io.h"
#include "resource.h"
#include "write_buffer.h"

#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define GABA_DECLARE
#include "io.h.x"
#undef GABA_DECLARE

/* A closed function with a file descriptor as argument */
/* GABA:
   (class
     (name fd_callback)
     (vars
       (f indirect-method int "int fd")))
*/

#define FD_CALLBACK(c, fd) ((c)->f(&(c), (fd)))

/* Close callbacks are called with a reason as argument. */

/* End of file while reading.
 * Or when a closed write_buffer has been flushed successfully. */
/* FIXME: Should we use separate codes for these two events? */
#define CLOSE_EOF 1

/* EPIPE when writing */
#define CLOSE_BROKEN_PIPE 2

#define CLOSE_WRITE_FAILED 3

/* #define CLOSE_READ_FAILED 4 */

#define CLOSE_PROTOCOL_FAILURE 5

/* GABA:
   (class
     (name close_callback)
     (vars
       (f method int "int reason")))
*/

#define CLOSE_CALLBACK(c, r) ((c)->f((c), (r)))

/* GABA:
   (class
     (name lsh_fd)
     (super resource)
     (vars
       (next object lsh_fd)
       (fd simple int)

       ; User's close callback
       (close_reason simple int)
       (close_callback object close_callback)

       ; Called before poll
       (prepare method void)

       (want_read simple int)
       ; Called if poll indicates that data can be read. 
       (read method void)

       (want_write simple int)
       ; Called if poll indicates that data can be written.
       (write method void)

       ; (close_now simple int)
       (really_close method void)))
*/

#define PREPARE_FD(fd) ((fd)->prepare((fd)))
#define READ_FD(fd) ((fd)->read((fd)))
#define WRITE_FD(fd) ((fd)->write((fd)))
#define REALLY_CLOSE_FD(fd) ((fd)->really_close((fd)))

/* GABA:
   (class
     (name io_fd)
     (super lsh_fd)
     (vars
       ; Reading 
       (handler object read_handler)
       ; Writing 
       (buffer object write_buffer)))
*/

/* Passed to the listen callback, and to other functions and commands
 * dealing with addresses. */
/* GABA:
   (class
     (name address_info)
     (vars
       ; An ipnumber, in decimal dot notation, ipv6 format, or
       ; a dns name.
       (ip string)
       ; The port number here is always in host byte order
       (port . UINT32))) */

/* GABA:
   (class
     (name fd_listen_callback)
     (vars
       (f method int int "struct address_info *")))
*/
#define FD_LISTEN_CALLBACK(c, fd, a) ((c)->f((c), (fd), (a)))

/* GABA:
   (class
     (name listen_fd)
     (super lsh_fd)
     (vars
       (callback object fd_listen_callback)))
*/

/* GABA:
   (class
     (name connect_fd)
     (super lsh_fd)
     (vars
       (callback object fd_callback)))
*/

#if 0
struct callout
{
  struct lsh_object header;
  
  struct callout *next;
  struct callback *callout;
  time_t when;
};
#endif

/* GABA:
   (class
     (name io_backend)
     (vars
       ; Linked list of fds. 
       (files object lsh_fd)
       ; Callouts
       ;; (callouts object callout)
       ))
*/

void init_backend(struct io_backend *b);

int io_iter(struct io_backend *b);
void io_run(struct io_backend *b);

int blocking_read(int fd, struct read_handler *r);

int get_inaddr(struct sockaddr_in	* addr,
	       const char		* host,
	       const char		* service,
	       const char		* protocol);

int get_portno(const char *s, const char *protocol);

int tcp_addr(struct sockaddr_in *sin,
	     UINT32 length,
	     UINT8 *addr,
	     UINT32 port);

struct address_info *make_address_info_c(const char *host,
					 const char *port);

struct address_info *make_address_info(struct lsh_string *host, 
				       UINT32 port);

struct address_info *sockaddr2info(size_t addr_len UNUSED,
				   struct sockaddr *addr);

int address_info2sockaddr_in(struct sockaddr_in *sin,
			     struct address_info *a);

int write_raw(int fd, UINT32 length, UINT8 *data);
int write_raw_with_poll(int fd, UINT32 length, UINT8 *data);

void io_set_nonblocking(int fd);
void io_set_close_on_exec(int fd);
void io_init_fd(int fd);

struct io_fd *make_io_fd(struct io_backend *b,
			 int fd);

struct connect_fd *io_connect(struct io_backend *b,
			      struct sockaddr_in *remote,
			      struct sockaddr_in *local,
			      struct fd_callback *f);

struct listen_fd *io_listen(struct io_backend *b,
			    struct sockaddr_in *local,
			    struct fd_listen_callback *callback);

struct io_fd *io_read_write(struct io_fd *fd,
			    struct read_handler *read_callback,
			    UINT32 block_size,
			    struct close_callback *close_callback);

struct io_fd *io_read(struct io_fd *fd,
		      struct read_handler *read_callback,
		      struct close_callback *close_callback);

struct io_fd *io_write(struct io_fd *fd,
		       UINT32 block_size,
		       struct close_callback *close_callback);

/* Marks a file for close, without touching the close_reason field. */
void kill_fd(struct lsh_fd *fd);

void close_fd(struct lsh_fd *fd, int reason);


#endif /* LSH_IO_H_INCLUDED */
