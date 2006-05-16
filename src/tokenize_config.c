/* tokenize_config.c
 *
 * Tokenizing of configuration files. */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 2002, 2005 Niels M�ller
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>

#include "tokenize_config.h"

#include "format.h"
#include "lsh_string.h"
#include "werror.h"

#define BUFFER (&(self->buffer))
#include "parse_macros.h"

void
config_tokenizer_init(struct config_tokenizer *self,
		      const char *file,
		      unsigned length, const unsigned char *data)
{
  simple_buffer_init(&self->buffer, length, data);
  self->file = file;
  self->lineno = 1;
}


enum config_token_type
config_tokenizer_next(struct config_tokenizer *self)
{
  static const char char_class[0x100] =
    {
      /* HT, LF, VT, FF, CR */
      0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      /* SPACE */
      1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      /* '{', '}' */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,2,0,2,0,0,

      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    };
#define IS_SPACE(c) (char_class[c] & 1)
#define IS_SEPARATOR(c) (char_class[c] & 3)
  for (;;)
    {
      while (LEFT && IS_SPACE(*HERE))
	{
	  if (*HERE == '\n')
	    self->lineno++;
	  ADVANCE(1);
	}

      if (!LEFT)
	self->type = TOK_EOF;
      else switch(*HERE)
	{
	case '{':
	  self->type = TOK_BEGIN_GROUP;
	  ADVANCE(1);
	  break;
	case '}':
	  self->type = TOK_END_GROUP;
	  ADVANCE(1);
	  break;
	case '#':
	  /* comment */
	  while (LEFT && *HERE != '\n')
	    ADVANCE(1);
	  continue;
	  
	default:
	  {
	    unsigned i;
	    self->type = TOK_STRING;
	
	    self->token = HERE;
	
	    for (i = 0; i<LEFT && !IS_SEPARATOR(HERE[i]); i++)
	      ;
	    self->token_length = i;
	    ADVANCE(i);
	  }
	}
  
      return self->type;
    }
}

void
config_tokenizer_error(struct config_tokenizer *self, const char *msg)
{
  werror("%z:%i: %z\n",
	 self->file, self->lineno, msg);
}

int
config_tokenizer_looking_at(struct config_tokenizer *self, const char *word)
{
  unsigned length = strlen(word);

  if (self->type == TOK_STRING
      && length == self->token_length
      && !memcmp(self->token, word, length))
    {
      config_tokenizer_next(self);
      return 1;
    }
  else
    return 0;
}

struct lsh_string *
config_tokenizer_get_string(struct config_tokenizer *self)
{
  struct lsh_string *s;
  if (self->type != TOK_STRING)
    { config_tokenizer_error(self, "expected word"); return NULL; }

  s = ssh_format("%ls", self->token_length, self->token);
  config_tokenizer_next(self);
  return s;
}

int
config_tokenizer_skip_token(struct config_tokenizer *self, enum config_token_type type)
{
  if (self->type == type)
    {
      config_tokenizer_next(self);
      return 1;
    }
  else
    { config_tokenizer_error(self, "syntax error"); return 0; }
}
