/* unpad.c
 *
 */

#include "unpad.h"
#include "xalloc.h"
#include "format.h"

static int do_unpad(struct abstract_write **w,
		    struct lsh_string *packet)
{
  struct abstract_write_pipe *closure = (struct abstract_write_pipe *) *w;
  
  UINT8 padding_length;
  UINT32 payload_length;
  struct lsh_string *new;
  
  if (packet->length < 1)
    return 0;
  
  padding_length = packet->data[0];

  if ( (padding_length < 4)
       || (padding_length >= packet->length) )
    return 0;

  payload_length = packet->length - 1 - padding_length;
  
  new = ssh_format("%ls", payload_length, packet->data + 1);

  /* Keep sequence number */
  new->sequence_number = packet->sequence_number;

  lsh_string_free(packet);

  return A_WRITE(closure->next, new);
}

struct abstract_write *
make_packet_unpad(struct abstract_write *continuation)
{
  struct abstract_write_pipe *closure =
    xalloc(sizeof(struct abstract_write_pipe));

  closure->super.write = do_unpad;
  closure->next = continuation;

  return &closure->super;
}
