/* packet_dispatch.c
 *
 */

static int do_dispatch(struct dispatch_processor *closure,
		       struct simple_packet *packet)
{
  unsigned start;
  unsigned end;
  unsigned msg;

  if (!packet->length)
    return 0;

  msg = packet->data[0];

  /* Do a binary serch. The number of valid message types should be rather small.
   */

  start = 0;
  end = closure->table_size;

  while(1)
    {
      unsigned middle = (start + end) / 2;
      unsigned middle_msg = closure->dispatch_table[middle]->msg;
      if (middle_msg == msg)
	{
	  /* Found right method */
	  return apply_processor(closure->dispatch_table[middle]->f,
				 packet);
	}
      if (middle == start)
	/* Not found */
	break;
      
      if (middle_id < msg)
	start = middle;
      else
	end = middle;
    }

  if (closure->default)
    return apply_processor(closure->default, packet);
  else
    return 0;
}

struct packet_processor *
make_dispatch_processor(unsigned size,
			struct dispatch_assoc *table,
			struct packet_processor *default)
{
  struct dispatch_processor *closure = xalloc(sizeof(struct dispatch_processor));
  unsigned i;

  /* Check that message numbers are increasing */
  for(i = 0; i+1 < size; i++)
    if (table[i]->msg >= table[i+1]->msg)
      fatal("make_dispatch_processor: Table out of order");
  
  closure->p->f = (raw_processor_function) do_dispatch;
  closure->default = default;
  closure->table_size = size;
  closure->dispatch_table = table;

  return (struct packet_processor *) closure;
}


      
