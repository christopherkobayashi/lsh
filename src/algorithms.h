/* algorithms.h
 *
 * Translate algorithm identifiers (or names) to algorithm objects.
 *
 * $Id$ */

#ifndef LSH_ALGORITHMS_H_INCLUDED
#define LSH_ALGORITHMS_H_INCLUDED

#include "abstract_compress.h"
#include "alist.h"
#include "lsh_argp.h"
#include "randomness.h"

#define GABA_DECLARE
#include "algorithms.h.x"
#undef GABA_DECLARE

/* GABA:
   (class
     (name algorithms_options)
     (vars
       (algorithms object alist)
       ; (pubkey_algorithms object alist)
       
       (crypto_algorithms object int_list)
       (mac_algorithms object int_list)
       (compression_algorithms object int_list)
       (hostkey_algorithms object int_list)))
*/

struct alist *all_symmetric_algorithms(void);
struct alist *all_signature_algorithms(struct randomness *r);

struct int_list *
filter_algorithms(struct alist *algorithms,
		  const struct int_list *candidates);

int
lookup_crypto(struct alist *algorithms, const char *name,
	      struct crypto_algorithm **ap);
int
lookup_mac(struct alist *algorithms, const char *name,
	   struct mac_algorithm **ap);
int
lookup_compression(struct alist *algorithms, const char *name,
		   struct compress_algorithm **ap);

int
lookup_hostkey_algorithm(const char *name);

int lookup_hash(struct alist *algorithms, const char *name,
		struct hash_algorithm **ap,
		int none_is_valid);

#if 0
struct int_list *default_crypto_algorithms(void);
struct int_list *all_crypto_algorithms(void);
struct int_list *default_mac_algorithms(void);
struct int_list *default_compression_algorithms(void);
struct int_list *prefer_compression_algorithms(void);
#endif

#if 0
void
vlist_algorithms(const struct argp_state *state,
		 struct alist *algorithms,
		 unsigned n,
		 va_list args);

void
list_algorithms(const struct argp_state *state,
		struct alist *algorithms,
		char *prefix,
		unsigned n, ...);
#endif

void
list_crypto_algorithms(const struct argp_state *state,
		       struct alist *algorithms);
void
list_mac_algorithms(const struct argp_state *state,
		    struct alist *algorithms);
void
list_compression_algorithms(const struct argp_state *state,
			    struct alist *algorithms);

void
list_hostkey_algorithms(const struct argp_state *state);

void init_algorithms_options(struct algorithms_options *self,
			     struct alist *algorithms);

extern const struct argp algorithms_argp;

#endif /* LSH_ALGORITHMS_H_INCLUDED */
