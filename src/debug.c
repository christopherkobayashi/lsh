/* debug.c
 *
 */

#include "debug.h"
#include "xalloc.h"

static int do_debug(struct debug_processor *closure,
		    struct lsh_string *packet)
{
  UINT32 i;
  
  fprintf(closure->output, "DEBUG: (packet size %d = 0x%x)\n",
	  packet->length, packet->length);

  for(i=0; i<packet->length; i++)
  {
    if (! i%16)
      fprintf(closure->output, "\n%08x: ", i);
    
    fprintf(closure->output, "%02x ", packet->data[i]);
  }

  fprintf(closure->output, "\n");

  return apply_processor(closure->c.next, packet);
}

struct abstract_write *make_debug_processor(FILE *output,
					      struct abstract_write *continuation)
{
  struct debug_processor *closure = xalloc(sizeof(struct debug_processor));

  closure->c.p.f = (abstract_write_f) do_debug;
  closure->c.next = continuation;
  closure->output = output;

  return (struct abstract_write *) closure;
}


