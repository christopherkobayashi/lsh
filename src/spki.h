/* spki.h
 *
 * An implementation of SPKI certificate checking
 *
 * $Id$ */

/* lsh, an implementation of the ssh protocol
 *
 * Copyright (C) 1999 Bal�zs Scheidler
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

#ifndef LSH_SPKI_H_INCLUDED
#define LSH_SPKI_H_INCLUDED

#include "publickey_crypto.h"
#include "exception.h"
#include "alist.h"

/* Needed by spki.h.x */
/* SPKI validity. No online validity tests supported. */
struct spki_validity
{
  char before_limit; /* Nonzero if not_before was supplied */
  char after_limit;  /* Nonzero if not_after was supplied */
  time_t not_before;
  time_t not_after;
};

struct spki_5_tuple;

#define GABA_DECLARE
#include "spki.h.x"
#undef GABA_DECLARE

/* GABA:
   (class
     (name spki_exception)
     (super exception)
     (vars
       (expr object sexp)))
*/

struct exception *
make_spki_exception(UINT32 type, const char *msg, struct sexp *expr);

int spki_get_type(struct sexp *e, struct sexp_iterator **res);

int spki_check_type(struct sexp *e, int type, struct sexp_iterator **res);

#if 0
/* FIXME: should support keyblobs other than ssh-dss */
struct sexp *keyblob2spki(struct lsh_string *keyblob);
#endif

struct sexp *
make_ssh_hostkey_tag(struct address_info *host);


struct sexp *
dsa_to_spki_public_key(struct dsa_public *p);

extern struct command_simple spki_signer2public;
#define SIGNER2PUBLIC (&spki_signer2public.super.super)

extern struct command_simple spki_parse_private_key_command;
#define SPKI_PARSE_PRIVATE_KEY (&spki_parse_private_key_command.super.super)

#if 0
extern struct spki_hash spki_hash_md5;
#define SPKI_HASH_MD5 (&spki_hash_md5.super.super)
extern struct spki_hash spki_hash_sha1;
#define SPKI_HASH_SHA1 (&spki_hash_sha1.super.super)
#endif

struct command *
make_spki_hash(int name, struct hash_algorithm *algorithm);

struct command *
make_spki_parse_key(struct alist *algorithms);

struct keypair *
read_spki_key_file(const char *name,
		   struct alist *algorithms,
		   struct exception_handler *e);


#if 0
/* Signature algorithms in spki */

/* ;; GABA:
   (class
     (name spki_algorithm)
     (vars
       ;; Called with i pointing to the expression after the algorithm name
       (make_signer method (object signer)
                    "struct sexp_iterator *i")
       (make_verifier method (object verifier)
                      "struct sexp_iterator *i")))
*/

#define SPKI_SIGNER(a, i) ((a)->make_signer((a), (i)))
#define SPKI_VERIFIER(a, i) ((a)->make_verifier((a), (i)))

struct signer *
spki_signer(struct sexp *e, struct alist *algorithms, int *type);

/* FIXME: Currently doesn't handle (hash ...) expressions. */

struct verifier *
spki_verifier(struct sexp *e, struct alist *algorithms, int *type);
#endif


/* At a point in time, not all fields are known; fields may be added
 * later, or computed as needed. This information is not automatically
 * trusted in any way, except that any non-NULL attributes must be
 * consistent with each other. */

/* GABA:
   (class
     (name spki_subject)
     (vars
       ; (public-key ...) expression.
       (key object sexp)

       ; Verifier
       (verifier object verifier)
       (sha1 string)
       (md5 string)))
*/

struct spki_subject *
make_spki_subject(struct sexp *key,
		  struct verifier *verifier,
		  struct lsh_string *sha1,
		  struct lsh_string *md5);

/* Keeps track of spki_subjects and their keys.
 *
 * We try to make sure that subjects within one context can be
 * compared pointer-wise. I.e. if we get several (public-key ...) and
 * (hash ...) expressions representing the same principal, we merge
 * them into a single spki_subject object. However, there is one case
 * in which this failes: If we encounter several (hash ...)
 * expressions with different hash algorithms, before we encounter the
 * non-hashed (public-key ...) expression. */

/* GABA:
   (class
     (name spki_context)
     (vars
       ; Looks up a public-key or hash.
       (lookup method (object spki_subject)
                      "struct sexp *e"
		      ; If non-NULL, use this verifier for
		      ; the subject. Useful for non-SPKI keys.
		      "struct verifier *v")
       (add_tuple method void
                  "struct spki_5_tuple *tuple")
       (authorize method int
                         "struct spki_subject *subject"
			 "struct sexp *access")))
       ;; (clone method (object spki_context))))
*/

#define SPKI_LOOKUP(c, e, v) ((c)->lookup((c), (e), (v)))
#define SPKI_ADD_TUPLE(c, t) ((c)->add_tuple((c), ((t))))
#define SPKI_AUTHORIZE(c, s, a) ((c)->authorize((c), (s), (a)))
#define SPKI_CLONE(c) ((c)->clone((c)))

struct spki_context *
make_spki_context(struct alist *algorithms);

/* 5-tuples */

#define SPKI_TAG_ATOM 1
#define SPKI_TAG_LIST 2
#define SPKI_TAG_SET 3
#define SPKI_TAG_PREFIX 4
#define SPKI_TAG_ANY 5

/* GABA:
   (class
     (name spki_tag)
     (vars
       ; Explicit type field is needed only for computing
       ; intersections
       (type . int)
       ; Returns true iff the resources described by the tag
       ; include the resource described by the sexp.
       (match method int "struct sexp *")))
*/

#define SPKI_TAG_TYPE(t) ((t)->type)
#define SPKI_TAG_MATCH(t, e) ((t)->match((t), (e)))

/* The data in a 5-tuple is always trusted, to the extent a non-NULL
 * issuer field implies that the tuple was derived from a certificate
 * that was properly signed by that issuer. However, no trust in the
 * issuer is assumed. */

/* GABA:
   (class
     (name spki_5_tuple)
     (vars
       ; Principal
       (issuer object spki_subject)
       ; Principal (n-to-k not yet supported)
       (subject object spki_subject)
       ; Non-zero to allow delegation
       (propagate . int)
       ; Authorization, (tag ...) expression
       (authorization object spki_tag)
       ; Validity period
       (validity . "struct spki_validity")))
       
*/

struct spki_5_tuple *
make_spki_5_tuple(struct spki_subject *issuer,
		  struct spki_subject *subject,
		  int propagate,
		  struct spki_tag *authorization,
		  int before_limit, time_t not_before,
		  int after_limit, time_t not_after);


struct spki_tag *
spki_sexp_to_tag(struct sexp *e,
		 /* Some limit on the recursion */
		 unsigned limit);

struct spki_5_tuple *
spki_acl_entry_to_5_tuple(struct spki_context *ctx,
			  struct sexp_iterator *i);

int
spki_add_acl(struct spki_context *ctx,
	     struct sexp *e);

#if 0
struct object_list *
spki_read_acls(struct spki_context *ctx,
	       struct sexp *e);
#endif

#endif /* LSH_SPKI_H_INCLUDED */
