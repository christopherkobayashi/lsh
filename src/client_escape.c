/* client_escape.h
 *
 * Escape char handling.
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1998, 1999, 2000, 2001 Niels M�ller
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

#include "client.h"

#include "format.h"
#include "werror.h"
#include "xalloc.h"

#include <assert.h>
#include <string.h>

#include <unistd.h>

#include "client_escape.c.x"

static void
do_suspend(struct lsh_callback *self UNUSED)
{
  pid_t group = getpgid(0);

  if (group < 0)
    werror("do_suspend: getpgid failed (errno = %i): %z\n",
	   errno, STRERROR(errno));
  else if (kill(getpgid(0), SIGTSTP) < 0)
    werror("do_suspend: kill failed (errno = %i): %z\n",
	   errno, STRERROR(errno));
}

/* FIXME: Use const? */
static struct lsh_callback
escape_suspend = { STATIC_HEADER, do_suspend };
  
struct escape_info *
make_escape_info(UINT8 escape)
{
  NEW(escape_info, self);
  unsigned i;

  self->escape = escape;
  
  for (i = 0; i<0x100; i++)
    self->dispatch[i] = NULL;

  /* C-z */
  self->dispatch[26] = &escape_suspend;

  return self;
}

/* GABA:
   (class
     (name escape_handler)
     (super abstract_write_pipe)
     (vars
       (info object escape_info)
       ; Number of characters of the prefix NL escape
       ; that have been read.
       (state . int)))
*/

#define GOT_NONE 0
#define GOT_NEWLINE 1
#define GOT_ESCAPE 2

/* Search for NEWLINE ESCAPE, starting at pos. If successful, returns
 * 1 and returns the index of the escape char. Otherwise, returns
 * zero. */
static UINT32
scan_escape(struct lsh_string *packet, UINT32 pos, UINT8 escape)
{
  for (;;)
    {
      UINT32 left;
      UINT8 *p;

      if (pos + 2 > packet->length)
	return 0;
      
      left = packet->length - pos;

      assert(left >= 2);

      p = memchr(packet->data + pos, '\n', left - 1);
      if (!p)
	return 0;
      else
	{
	  pos = p - packet->data + 1;
	  assert(pos < packet->length);
	  if (packet->data[pos] == escape)
	    return pos;
	}
    }
}

/* Returns 1 for the quote action. */ 
static int
escape_dispatch(struct escape_info *info,
		UINT8 c)
{
  struct lsh_callback *f;

  if (c == info->escape)
    return 1;
  
  f = info->dispatch[c];
  if (f)
    LSH_CALLBACK(f);
  else
    /* FIXME: This message could be improved. werror needs a flag to
     * output a single pretty-printed character. */
    werror("<escape> %x not defined.\n", c);

  return 0;
}

static void
do_escape_handler(struct abstract_write *s, struct lsh_string *packet)
{
  CAST(escape_handler, self, s);
  UINT32 pos;
  UINT32 done;
  
  if (!packet)
    /* EOF. Pass it on */
    A_WRITE(self->super.next, packet);

  assert(packet->length);

  /* done is the length of the prefix of the data that is already
   * consumed in one way or the other, while pos is the next position
   * where it makes sense to look for the escape sequence. */
  done = pos = 0;

  /* Look for an escape at the start of the packet. */
  switch (self->state)
    {
    case GOT_NEWLINE:
      /* We already got newline. Is the next character the escape? */
      if (packet->data[0] == self->info->escape)
	{
	  self->state = GOT_ESCAPE;
	  if (packet->length == 1)
	    {
	      lsh_string_free(packet);
	      return;
	    }
	  else
	    {
	      pos = 2;
	      if (escape_dispatch(self->info, packet->data[1]))
		/* The quote action. Keep the escape. */
		done = 1;
	      else
		done = 2;
	    }
	}
      break;
    case GOT_ESCAPE:
      pos = 1;
      if (escape_dispatch(self->info, packet->data[0]))
	/* The quote action. Keep the escape. */
	done = 0;
      else
	done = 1;
      break;
    }
  
  while ( (pos = scan_escape(packet, pos, self->info->escape)) )
    {
      /* We found another escape char! */

      /* First, pass on the data before the escape.  */
      assert(pos > done);
      A_WRITE(self->super.next,
	      ssh_format("%ls", pos - done, packet->data + done));
      
      /* Point to the action character. */
      pos++;
      if (pos == packet->length)
	{
	  /* Remember how far we got. */
	  self->state = GOT_ESCAPE;
	  lsh_string_free(packet);
	  return;
	}
      if (escape_dispatch(self->info, packet->data[pos]))
	/* Keep escape */
	done = pos;
      else
	done = pos + 1;
      pos++;
    }

  /* Handle any data after the final escape */
  if (done < packet->length)
    {
      /* Rember if the last character is a newline. */
      if (packet->data[packet->length - 1] == '\n')
	self->state = GOT_NEWLINE;
      else
	self->state = GOT_NONE;
      
      /* Send data on */
      if (done)
	{
	  /* Partial packet */
	  A_WRITE(self->super.next,
		  ssh_format("%ls", packet->length - done,
			     packet->data + done));
	  lsh_string_free(packet);
	}
      else
	A_WRITE(self->super.next, packet);
    }
  else
    lsh_string_free(packet);
}

struct abstract_write *
make_handle_escape(struct escape_info *info, struct abstract_write *next)
{
  NEW(escape_handler, self);

  self->super.super.write = do_escape_handler;
  self->super.next = next;
  self->info = info;
  self->state = GOT_NONE;
  
  return &self->super.super;
}
