/* unpad.h
 *
 * Processor for unpadding and formatting ssh-packets
 */

#ifndef LSH_UNPAD_H_INCLUDED
#define LSH_UNPAD_H_INCLUDED

#include "transport.h"

/* Input to the processor is a padded payload. */
struct packet_unpad
{
  struct abstract_write_pipe c;
};

struct abstract_write *
make_packet_unpad(struct abstract_write *continuation);

#endif /* LSH_UNPAD_H_INCLUDED */
