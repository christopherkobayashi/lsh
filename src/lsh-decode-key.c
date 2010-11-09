/* lsh-decode-key.c
 *
 * Decode ssh2 keys.
 *
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <unistd.h>

#include "nettle/sexp.h"

#include "crypto.h"
#include "format.h"
#include "io.h"
#include "lsh_argp.h"
#include "lsh_string.h"
#include "parse.h"
#include "spki.h"
#include "version.h"
#include "werror.h"
#include "xalloc.h"

#include "lsh-decode-key.c.x"

/* Option parsing */

const char *argp_program_version
= "lsh-decode-key (" PACKAGE_STRING ")";

const char *argp_program_bug_address = BUG_ADDRESS;

/* GABA:
   (class
     (name lsh_decode_key_options)
     (super werror_config)
     (vars
       ; Output filename
       (file string)

       ; Assume input is base64
       (base64 . int)))
*/

static struct lsh_decode_key_options *
make_lsh_decode_key_options(void)
{
  NEW(lsh_decode_key_options, self);
  init_werror_config(&self->super);

  self->file = NULL;
  self->base64 = 0;

  return self;
}

static const struct argp_option
main_options[] =
{
  /* Name, key, arg-name, flags, doc, group */
  { "output-file", 'o', "Filename", 0, "Default is stdout", 0 },
  { "base64", 'b', NULL, 0, "Input is base64 encoded", 0 },
  { NULL, 0, NULL, 0, NULL, 0 }
};

static const struct argp_child
main_argp_children[] =
{
  { &werror_argp, 0, "", 0 },
  { NULL, 0, NULL, 0}
};

static error_t
main_argp_parser(int key, char *arg, struct argp_state *state)
{
  CAST(lsh_decode_key_options, self, state->input);

  switch(key)
    {
    default:
      return ARGP_ERR_UNKNOWN;

    case ARGP_KEY_INIT:
      state->child_inputs[0] = &self->super;
      break;
      
    case ARGP_KEY_END:
      if (!werror_init(&self->super))
	argp_failure(state, EXIT_FAILURE, errno, "Failed to open log file");
      break;
      
    case 'b':
      self->base64 = 1;
      break;
      
    case 'o':
      self->file = make_string(arg);
      break;
    }
  return 0;
}

static const struct argp
main_argp =
{ main_options, main_argp_parser, 
  NULL,
  ( "Converts a raw OpenSSH/ssh2 public key to sexp-format.\v"
    "Usually invoked by the ssh-conv script."),
  main_argp_children,
  NULL, NULL
};


static struct lsh_string *
lsh_decode_key(struct lsh_string *contents)
{
  struct simple_buffer buffer;
  enum lsh_atom type;
  struct verifier *v;	

  simple_buffer_init(&buffer, STRING_LD(contents));

  if (!parse_atom(&buffer, &type))
    {
      werror("Invalid (binary) input data.\n");
      return NULL;
    }

  switch (type)
    {
    case ATOM_SSH_DSS:
      {
        werror("Reading key of type ssh-dss...\n");

        v = parse_ssh_dss_public(&buffer);
        
        if (!v)
          {
            werror("Invalid dsa key.\n");
            return NULL;
          }
	break;
      }

    case ATOM_SSH_DSA_SHA256_LOCAL:
      {
        werror("Reading key of type ssh-dsa-sha256...\n");

        v = parse_ssh_dsa_sha256_public(&buffer);
        
        if (!v)
          {
            werror("Invalid dsa-sha256 key.\n");
            return NULL;
          }
	break;
      }
    case ATOM_SSH_RSA:
      {
	werror("Reading key of type ssh-rsa...\n");

	v = parse_ssh_rsa_public(&buffer);

	if (!v)
	  {
	    werror("Invalid rsa key.\n");
	    return NULL;
	  }

	break;
      }
    default:
      werror("Unknown key type.");
      return NULL;
    }
  return PUBLIC_SPKI_KEY(v, 1);
}


int main(int argc, char **argv)
{
  struct lsh_decode_key_options *options = make_lsh_decode_key_options();
  struct lsh_string *input;
  struct lsh_string *output;
  
  int out = STDOUT_FILENO;
  
  argp_parse(&main_argp, argc, argv, 0, NULL, options);

  if (options->file)
    {
      out = open(lsh_get_cstring(options->file),
                 O_WRONLY | O_CREAT, 0666);
      if (out < 0)
        {
          werror("Failed to open file `%S' for writing: %e.\n",
                 options->file, errno);
          return EXIT_FAILURE;
        }
    }

  input = io_read_file_raw(STDIN_FILENO, 3000);
  if (!input)
    {
      werror("Failed to read stdin: %e.\n", errno);
      return EXIT_FAILURE;
    }

  if (options->base64)
    {
      if (!lsh_string_base64_decode(input))
	{
          werror("Invalid base64 encoding.\n");

	  lsh_string_free(input);

          return EXIT_FAILURE;
        }
    }
  
  output = lsh_decode_key(input);
  lsh_string_free(input);

  if (!output)
    {
      werror("Invalid ssh2 key.\n");
      return EXIT_FAILURE;
    }
    
  if (!write_raw(out, STRING_LD(output)))
    {
      werror("Write failed: %e.\n", errno);
      return EXIT_FAILURE;
    }
  lsh_string_free(output);
  
  gc_final();
  
  return EXIT_SUCCESS;
}
