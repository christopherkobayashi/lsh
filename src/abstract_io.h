/* abstract_io.h
 *
 * This is the layer separating protocol processing from actual io.
 */

#ifndef LSH_ABSTRACT_IO_H_INCLUDED
#define LSH_ABSTRACT_IO_H_INCLUDED

#include "lsh_types.h"

/* A read-function returning n means:
 *
 * n > 0: n bytes were read successfully.
 * n = 0: No more data available, without blocking.
 * n = -1: Read failed.
 * n = -2: EOF.
 */
#define A_FAIL -1
#define A_EOF -2

struct abstract_read
{
  int (*read)(struct abstract_read **r,
	      UINT8 *buffer, UINT32 length);
};

#define A_READ(f, buffer, length) (f)->read(&(f), (buffer), (length))

/* May store a new handler into *h. */
struct read_handler
{
  int (*handler)(struct read_handler **h,
		 struct abstract_read *read);
};

#define READ_HANDLER(h, read) ((h)->handler(&(h), (read)))

/* FIXME: What should writers return? */

/* May store a new handler into *w. */
struct abstract_write
{
  int (*write)(struct abstract_write **w,
	       struct lsh_string *packet);
};

#define A_WRITE(f, packet) ((f)->write(&(f), (packet)))

/* A processor that passes its result on to another processor */
struct abstract_write_pipe
{
  struct abstract_write super;
  struct abstract_write *next;
};

#endif /*LSH_ABSTRACT_IO_H_INCLUDED */
