/* read_packet.c
 *
 */

#include "read_packet.h"

#define WAIT_HEADER 0
#define WAIT_CONTENTS 1
#define WAIT_MAC 2

static struct read_handler *do_read_packet(struct read_packet *closure,
					   struct abstract_read *read)
{
  while(1)
    {
      switch(closure->state)
	{
	case WAIT_HEADER:
	  {
	    UINT32 left = closure->crypto->block_size - closure->pos;
	    int n;
	
	    if (!closure->buffer)
	      {
		closure->buffer = lsh_string_alloc(crypt->block_size);
		closure->pos = 0;
	      }
	    n = A_READ(read, closure->buffer + closure->pos, left);
	    switch(n)
	      {
	      case 0:
		return closure;
	      case A_FAIL:
		werror("do_read_packet: read() failed, %s\n", strerror(errno));
		/* Fall through */
	      case A_EOF:
		/* FIXME: Free associated resources! */
		return 0;
	      }
	    closure->pos += n;

	    /* Read a complete block? */
	    if (n == left)
	      {
		UINT32 length;

		CRYPT(closure->crypto,
		      closure->crypto->block_size,
		      closure->buffer->data,
		      closure->buffer->data);

		length = READ_UINT32(closure->buffer->data);
		if (length > closure->max_packet)
		  return 0;

		if ( (length < 12)
		     || (length < (closure->block_size - 4))
		     || ( (length + 4) % closure->block_size))
		  return 0;

		/* Process this block before the length field is lost. */
		if (closure->mac)
		  {
		    UINT8 s[4];
		    WRITE_UINT32(s, closure->sequence_number);
		    
		    UPDATE(closure->mac, 4, s);
		    UPDATE(closure->mac,
			   closure->buffer->length,
			   closure->buffer->data);
		  }

		/* Allocate full packet */
		closure->buffer = ssh_format("%ls%lr",
					     closure->block_size - 4,
					     closure->buffer->data + 4,
					     length, &closure->crypt_pos);

		/* FIXME: Is this needed anywhere? */
		closure->buffer->sequence_number = closure->sequence_number++;

		closure->pos = 4;
		closure->state = WAIT_CONTENTS;
		/* Fall through */
	      }
	    else
	      /* Try reading some more */
	      break;
	  }
	case WAIT_CONTENTS:
	  {
	    UINT32 left = closure->buffer->length - closure->pos;
	    int n = A_READ(read, closure->buffer->data + closure->pos, left);

	    switch(n)
	      {
	      case 0:
		return closure;
	      case A_FAIL:
		werror("do_read_packet: read() failed, %s\n", strerror(errno));
		/* Fall through */
	      case A_EOF:
		/* FIXME: Free associated resources! */
		return 0;
	      }
	    closure->pos += n;

	    /* Read a complete packet? */
	    if (n == left)
	      {
		CRYPT(closure->crypto,
		      closure->buffer->length - closure->crypt_pos,
		      closure->buffer->data + closure->crypt_pos,
		      closure->buffer->data + closure->crypt_pos);		      

		if (closure->mac)
		  {
		    UPDATE(closure->mac,
			   closure->buffer->length - closure->crypt_pos,
			   closure->buffer->data + closure->crypt_pos);
		    DIGEST(closure->mac,
			   closure->computed_mac);
		  }
		closure->state = WAIT_MAC;
		closure->pos = 0;
		/* Fall through */
	      }
	    else
	      /* Try reading some more */
	      break;
	  }
	case WAIT_MAC:
	  if (closure->mac_size)
	    {
	      UINT32 left = closure->mac->mac_size - closure->pos;
	      UINT8 *mac = alloca(left);

	      int n = A_READ(read, mac, left);

	      switch(n)
		{
		case 0:
		  return closure;
		case A_FAIL:
		  werror("do_read_packet: read() failed, %s\n",
			 strerror(errno));
		  /* Fall through */
		case A_EOF:
		  /* FIXME: Free associated resources! */
		  return 0;
	      }

	      if (!memcpy(mac,
			  closure->computed_mac + closure->pos,
			  n))
		/* FIXME: Free resources */
		return 0;

	      closure->pos += n;

	      if (n < left)
		/* Try reading more */
		break;
	    }
	  /* MAC was ok, send packet on */
	  if (!A_WRITE(closure->handler, closure->buffer))
	    /* FIXME: What now? */
	    return 0;
	  
	  closure->buffer = NULL;
	  state = WAIT_HEADER;
	  break;
	  
	default:
	  fatal("Internal error\n");
	}
    }
}
