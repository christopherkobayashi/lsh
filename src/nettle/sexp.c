/* sexp.c
 *
 * Parsing s-expressions.
 */

/* nettle, low-level cryptographics library
 *
 * Copyright (C) 2002 Niels M�ller
 *  
 * The nettle library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 * 
 * The nettle library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with the nettle library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "sexp.h"

#include <stdlib.h>
#include <string.h>

/* Initializes the iterator. You have to call next to get to the first
 * element. */
static void
sexp_iterator_init(struct sexp_iterator *iterator,
		   unsigned length, const uint8_t *input)
{
  iterator->length = length;
  iterator->buffer = input;
  iterator->pos = 0;
  iterator->level = 0;
  iterator->type = SEXP_START;
  iterator->display_length = 0;
  iterator->display = NULL;
  iterator->atom_length = 0;
  iterator->atom = NULL;

  /* FIXME: For other than canonical syntax,
   * skip white space here. */
}

int
sexp_iterator_first(struct sexp_iterator *iterator,
		    unsigned length, const uint8_t *input)
{
  sexp_iterator_init(iterator, length, input);
  return sexp_iterator_next(iterator);
}

#define EMPTY(i) ((i)->pos == (i)->length)
#define NEXT(i) ((i)->buffer[(i)->pos++])

static int
sexp_iterator_simple(struct sexp_iterator *iterator,
		     unsigned *size,
		     const uint8_t **string)
{
  unsigned length = 0;
  uint8_t c;
  
  if (EMPTY(iterator)) return 0;
  c = NEXT(iterator);
  if (EMPTY(iterator)) return 0;

  if (c >= '1' && c <= '9')
    do
      {
	length = length * 10 + (c - '0');
	if (length > (iterator->length - iterator->pos))
	  return 0;

	if (EMPTY(iterator)) return 0;
	c = NEXT(iterator);
      }
    while (c >= '0' && c <= '9');

  else if (c == '0')
    /* There can be only one */
    c = NEXT(iterator);
  else 
    return 0;

  if (c != ':')
    return 0;

  *size = length;
  *string = iterator->buffer + iterator->pos;
  iterator->pos += length;

  return 1;
}

/* All these functions return 1 on success, 0 on failure */
int
sexp_iterator_next(struct sexp_iterator *iterator)
{
  if (iterator->type == SEXP_END)
    return 1;
  
  if (EMPTY(iterator))
    {
      if (iterator->level)
	return 0;
      
      iterator->type = SEXP_END;
      return 1;
    }
  switch (iterator->buffer[iterator->pos])
    {
    case '(': /* A list */
      if (iterator->type == SEXP_LIST)
	/* Skip this list */
	return sexp_iterator_enter_list(iterator)
	  && sexp_iterator_exit_list(iterator)
	  && sexp_iterator_next(iterator);
      else
	{
	  iterator->type = SEXP_LIST;
	  return 1;
	}
    case ')':
      iterator->pos++;
      iterator->type = SEXP_END;
      return 1;
      
    case '[': /* Atom with display type */
      iterator->pos++;
      if (!sexp_iterator_simple(iterator,
				&iterator->display_length,
				&iterator->display))
	return 0;
      if (EMPTY(iterator) || NEXT(iterator) != ']')
	return 0;

      break;

    default:
      /* Must be either a decimal digit or a syntax error.
       * Errors are detected by sexp_iterator_simple. */
      iterator->display_length = 0;
      iterator->display = NULL;

      break;
    }

  iterator->type = SEXP_ATOM;
      
  return sexp_iterator_simple(iterator,
			      &iterator->atom_length,
			      &iterator->atom);
}

/* Current element must be a list. */
int
sexp_iterator_enter_list(struct sexp_iterator *iterator)
{
  if (iterator->type != SEXP_LIST)
    return 0;

  if (EMPTY(iterator) || NEXT(iterator) != '(')
    /* Internal error */
    abort();

  iterator->level++;
  iterator->type = SEXP_START;
  
  return sexp_iterator_next(iterator);
}

/* Skips the rest of the current list */
int
sexp_iterator_exit_list(struct sexp_iterator *iterator)
{
  if (!iterator->level)
    return 0;

  for (;;)
    {
      if (!sexp_iterator_next(iterator))
	return 0;
      
      if (iterator->type == SEXP_END)
	{
	  iterator->type = SEXP_START;	  
	  iterator->level--;
	  return 1;
	}
    }
}

int
sexp_iterator_check_type(struct sexp_iterator *iterator,
			 const uint8_t *type)
{
  return (sexp_iterator_enter_list(iterator)
	  && iterator->type == SEXP_ATOM
	  && !iterator->display
	  && strlen(type) == iterator->atom_length
	  && !memcmp(type, iterator->atom, iterator->atom_length));
}

const uint8_t *
sexp_iterator_check_types(struct sexp_iterator *iterator,
			  unsigned ntypes,
			  const uint8_t **types)
{
  if (sexp_iterator_enter_list(iterator)
      && iterator->type == SEXP_ATOM
      && !iterator->display)
    {
      unsigned i;
      for (i = 0; i<ntypes; i++)
	if (strlen(types[i]) == iterator->atom_length
	    && !memcmp(types[i], iterator->atom,
		       iterator->atom_length))
	  return types[i];
    }
  return 0;
}
		   

int
sexp_iterator_assoc(struct sexp_iterator *iterator,
		    unsigned nkeys,
		    const uint8_t **keys,
		    struct sexp_iterator *values)
{
  int *found;
  unsigned nfound;
  unsigned i;
  
  found = alloca(nkeys * sizeof(*found));
  for (i = 0; i<nkeys; i++)
    found[i] = 0;

  nfound = 0;
  
  for (;;)
    {
      switch (iterator->type)
	{
	case SEXP_LIST:

	  /* FIXME: Use sexp_iterator_check_type? */
	  if (!sexp_iterator_enter_list(iterator))
	    return 0;
	  
	  if (iterator->type == SEXP_ATOM
	      && !iterator->display)
	    {
	      /* Compare to the given keys */
	      for (i = 0; i<nkeys; i++)
		{
		  /* NOTE: The strlen could be put outside of the
		   * loop */
		  if (strlen(keys[i]) == iterator->atom_length
		      && !memcmp(keys[i], iterator->atom,
				 iterator->atom_length))
		    {
		      if (found[i])
			/* We don't allow duplicates */
			return 0;

		      found[i] = 1;
		      nfound++;
		      
		      /* Record this position. */
		      values[i] = *iterator;

		      break;
		    }
		}
	    }
	  if (!sexp_iterator_exit_list(iterator))
	    return 0;
	  break;
	case SEXP_ATOM:
	  /* Just ignore */
	  break;
	  
	case SEXP_END:
	  return sexp_iterator_exit_list(iterator)
	    && (nfound == nkeys);

	default:
	  abort();
	}
      if (!sexp_iterator_next(iterator))
	return 0;
    }
}
