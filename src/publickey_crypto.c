/* publickey_crypto.c
 *
 */

#include "bignum.h"
#include "sha.h"
#include "atoms.h"
#include "publickey_crypto.h"

/* DSS signatures */

struct dss_public
{
  mpz_t p;
  mpz_t q;
  mpz_t g;
  mpz_t y;
};

struct dss_signer
{
  struct signer super;
  struct randomness *random;
  struct dss_public public;
  mpz_t a;
};

struct dss_verifier
{
  struct verifier super;
  struct dss_public public;
};
  
struct dss_algorithm
{
  struct signature_algorithm super;
  struct randomness *random;
};

static void dss_hash(mpz_t h, UINT32 length, UINT8 *msg)
{
  /* Compute hash */
  hash = MAKE_HASH(&sha_algorithm);
  digest = alloca(hash->hash_size);
  HASH_UPDATE(hash, length, data);
  HASH_DIGEST(hash, digest);

  bignum_parse_u(h, digest, hash->hash_size);

  lsh_free(hash);
}

static struct lsh_string *do_dss_sign(struct signer *s,
				      UINT32 length,
				      UINT8 *msg)
{
  struct dss_signer *closure = (struct dss_signer *) s;
  mpz_t k, r, s, tmp;
  struct hash_instance *hash;
  UINT8 *digest;
  struct lsh_string *signature;
  
  /* Select k, 0<k<q, randomly */
  mpz_init_set(tmp, closure->public.q);
  mpz_sub_ui(tmp, tmp, 1);

  mpz_init(k);
  bignum_random(k, closure.random, tmp);
  mpz_add_ui(k, k, 1);

  /* Compute r = (g^k (mod p)) (mod q) */
  mpz_init(r);
  mpz_powm(r, closure->public.g, k, closure->public.p);
  mpz_tdiv_r(r, r, closure->public.q);
  
  /* Compute hash */
  dss_hash(tmp, length, msg);

  /* Compute k^-1 (mod q) */
  if (!mpz_invert(k, k, closure->public.q))
    {
      werror("do_dss_sign: k non-invertible\n");
      mpz_clear(tmp);
      mpz_clear(k);
      mpz_clear(r);
      return NULL;
    }

  /* Compute signature s = k^-1(h + ar) (mod q) */
  mpz_init(s);
  mpz_mul(s, r, closure->secret);
  mpz_tdiv_r(s, s, closure->public.q);
  mpz_add(s, s, tmp);
  mpz_mul(s, s, k);
  mpz_tdiv_r(s, s, closure->public.q);
  
  /* Build signature */
  signature = ssh_format("%a%n%n", ATOM_SSH_DSS, r, s);

  mpz_clear(k);
  mpz_clear(r);
  mpz_clear(s);
  mpz_clear(tmp);

  return signature;
}

ind do_dss_verify(struct verifier *closure,
		  UINT32 length,
		  UINT8 *msg,
		  UINT32 signature_length,
		  UINT8 * signature_data)
{
  struct dss_signer *closure = (struct dss_signer *) s;
  struct simple_buffer buffer;

  int res;
  
  int atom;
  mpz_t r, s;

  mpz_t w, tmp, u, v;
  
  simple_buffer_init(&buffer, signature_length, signature_data);
  if (!parse_atom(&buffer, &atom)
      || (atom != ATOM_SSH_DSS) )
    return 0;

  mpz_init(r);
  mpz_init(s);
  if (! (parse_bignum(&buffer, r)
	 && parse_bignum(&buffer, s)
	 && parse_eod(&buffer)
	 && (mpz_sgn(r) == 1)
	 && (mpz_sgn(s) == 1)
	 && (mpz_cmp(r, closure->public.q) < 0)
	 && (mpz_cmp(s, closure->public.q) < 0) ))
    {
      mpz_clear(r);
      mpz_clear(s);
      return 0;
    }

  /* Compute w = s^-1 (mod q) */
  mpz_init(w);
  if (!mpz_invert(s, closure->public.q))
    {
      werror("do_dss_verify: s non-invertible.\n");
      mpz_clear(r);
      mpz_clear(s);
      mpz_clear(w);
      return 0;
    }

  /* Compute hash */
  mpz_init(tmp);
  dss_hash(tmp, length, msg);

  /* g^{w * h (mod q)} (mod p)  */

  mpz_init(v);

  mpz_mul(tmp, tmp, w);
  mpz_tdiv_r(tmp, tmp, closure->public.q);

  mpz_powm(v, closure->public.g, tmp, closure->public.p);

  /* y^{w * r (mod q) } (mod p) */
  mpz_mul(tmp, r, w);
  mpz_tdiv_r(tmp, tmp, closure->public.q);
  mpz_powm(tmp, closure->public.y, tmp, closure->public.p);

  /* (g^{w * h} * y^{w * r} (mod p) ) (mod q) */
  mpz_mul(v, v, tmp);
  mpz_tdiv_r(v, v, closure->public.q);

  res = mpz_cmp(v, r);

  mpz_clear(r);
  mpz_clear(s);
  mpz_clear(w);
  mpz_clear(tmp);
  mpz_clear(v);

  return !res;
}

int parse_dss_public(struct simple_buffer *buffer, struct dss_public *public)
{
#if 0
  mpz_init(public->p);
  mpz_init(public->q);
  mpz_init(public->g);
  mpz_init(public->y);
#endif
  
  return (parse_bignum(&buffer, public.p)
	  && parse_bignum(&buffer, public->p)
	  && (mpz_sgn(public->p) == 1)
	  && parse_bignum(&buffer, public->q)
	  && (mpz_sgn(public->q) == 1)
	  && (mpz_cmp(public->q, public->p) < 0) /* q < p */ 
	  && parse_bignum(&buffer, public->g)
	  && (mpz_sgn(public->g) == 1)
	  && (mpz_cmp(public->g, public->p) < 0) /* g < p */ 
	  && parse_bignum(&buffer, public->y) 
	  && (mpz_sgn(public->y) == 1)
	  && (mpz_cmp(public->y, public->p) < 0) /* y < p */  );
#if 0
      mpz_clear(public->p);
      mpz_clear(public->q);
      mpz_clear(public->g);
      mpz_clear(public->y);
      return 0;
#endif
}

/* FIXME: Outside of the protocol transactions, keys should be stored
 * in SPKI-style S-expressions. */
struct signer *make_dss_signer(struct signature_algorithm *closure,
			       UINT32 public_length,
			       UINT8 *public,
			       UINT32 secret_length,
			       UINT8 *secret)
{
  struct dss_signer *res;
  struct simple_buffer buffer;
  int atom;

  simple_buffer_init(&buffer, signature_length, signature_data);
  if (!parse_atom(&buffer, &atom)
      || (atom != ATOM_SSH_DSS) )
    return 0;

  res = xalloc(sizeof(struct dss_signer));

  mpz_init(res->public.p);
  mpz_init(res->public.q);
  mpz_init(res->public.g);
  mpz_init(res->public.y);
  mpz_init(res->secret);
  
  if (! (parse_dss_public(&buffer, &res->public)
  	 && parse_bignum(&buffer, res->secret)
	 /* FIXME: Perhaps do some more sanity checks? */
	 && mpz_sign(res->secret) == 1))
    {
      mpz_clear(res->public.p);
      mpz_clear(res->public.q);
      mpz_clear(res->public.g);
      mpz_clear(res->public.y);
      mpz_clear(res->secret);
      lsh_free(res);
      return NULL;
    }
  
  res->super.sign = do_dss_sign;
  return &res->super;
}

struct verifier *make_dss_verifier(struct signature_algorithm *closure,
				   UINT32 public_length,
				   UINT8 *public)
{
  struct dss_verifier *res;
  struct simple_buffer buffer;
  int atom;

  simple_buffer_init(&buffer, public_length, public);
  if (!parse_atom(&buffer, &atom)
      || (atom != ATOM_SSH_DSS) )
    return 0;

  res = xalloc(sizeof(struct dss_verifier));

  mpz_init(res->public.p);
  mpz_init(res->public.q);
  mpz_init(res->public.g);
  mpz_init(res->public.y);
  
  if (!parse_dss_public(&buffer, &res->public))
    /* FIXME: Perhaps do some more sanity checks? */
    {
      mpz_clear(res->public.p);
      mpz_clear(res->public.q);
      mpz_clear(res->public.g);
      mpz_clear(res->public.y);
          lsh_free(res);
      return NULL;
    }

  res->super.verify = do_dss_verify;
  return &res->super;
}

struct make_dss_algorithm(struct randomness *random)
{
  struct dss_algorithm *res = xalloc(sizeof(struct dss_algorithm));

  dss->super.make_signer = make_dss_signer;
  dss->super.make_verifier = make_dss_verifier;
  dss->random = random;

  return &dss->super;
}
    
/* Groups */

struct group_zn  /* Z_n^* */
{
  struct group super;
  mpz_t modulo;
};

static int zn_member(struct group *c, mpz_t x)
{
  struct group_zn *closure = (struct group_zn *) c;

  return ( (mpz_sgn(x) == 1) && (mpz_cmp(x, closure->modulo) < 0) );
}

static void zn_invert(struct group *c, mpz_t res, mpz_t x)
{
  struct group_zn *closure = (struct group_zn *) c;

  if (!mpz_invert(res, x, closure->modulo))
    fatal("zn_invert: element is non-invertible\n");
}

static void zn_combine(struct group *c, mpz_t res, mpz_t a, mpz_t b)
{
  struct group_zn *closure = (struct group_zn *) c;

  mpz_mul(res, a, b);
  mpz_tdiv_r(res, res, closure->modulo);
}

static void zn_power(struct group *c, mpz_t res, mpz_t g, mpz_t e)
{
  struct group_zn *closure = (struct group_zn *) c;

  mpz_powm(res, g, e, closure->modulo);
}

/* Assumes p is a prime number */
struct group *make_zn(mpz_t p)
{
  struct group_zn *res = xalloc(sizeof(struct group_zn));

  res->super.member = zn_member;
  res->super.invert = zn_invert;
  res->super.combine = zn_combine;
  res->super.power = zn_power;     /* Pretty Mutation! Magical Recall! */
  
  mpz_init_set(res->modulo, n);
  mpz_init_set(res->super.order, n);
  return &res->super;
}

/* diffie-hellman */

struct diffie_hellman_instance *
make_diffie_hellman_instance(struct diffie_hellman_method *m,
				    struct connection *c)
{
  struct diffie_hellman_instance *res
    = xalloc(sizeof(struct diffie_hellman_instance));

  mpz_init(res->e);
  mpz_init(res->f);
  mpz_init(res->secret);
  
  res->method = m;
  res->hash = make_hash(m->H);
  HASH_UPDATE(res->hash,
	      c->client_version->length,
	      c->client_version->data);
  HASH_UPDATE(res->hash,
	      c->server_version->length,
	      c->server_version->data);
  return res;
}

struct group *make_dh1(struct randomness *r)
{
  struct diffie_hellman_method *res
    = xalloc(sizeof(struct diffie_hellman_method));

  mpz_t p;
  
  mpz_init_set_str(p,
		   "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		   "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		   "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		   "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		   "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381"
		   "FFFFFFFFFFFFFFFF", 16);

  res->G = make_zn(p);
  mpz_clear(p);

  mpz_init_set_ui(res->generator, 2);

  res->hash_algorithm = &sha_algorithm;
  res->random = r;
  
  return res;
}

void dh_generate_secret(struct diffie_hellman_instance *self,
			mpz_t r)
{
  mpz_t tmp;

  /* Generate a random number, 1 < x < O(G) */
  mpz_init_set(tmp, self->method->G->order);
  mpz_sub_ui(tmp, tmp, 2);
  bignum_random(self->secret, tmp);
  mpz_add_ui(self->secret, self->secret, 2);
  mpz_clear(tmp);

  GROUP_POWER(self->methog->G, r, self->method->generator, self->secret);
}

struct lsh_string *dh_make_client_msg(struct diffie_hellman_instance *self)
{
  dh_generate_secret(self, self->e);
  return ssh_format("%c%n", SSH_MSG_KEXDH_INIT, self->e);
}

int dh_process_client_msg(struct diffie_hellman_instance *self,
			  struct lsh_string *packet)
{
  struct simple_buffer buffer;
  UINT8 msg_number;
  
  simple_buffer_init(&buffer, packet->length, packet->data);

  return (parse_uint8(&buffer, &msg_number)
	  && (msg_number == SSH_MSG_KEXDH_INIT)
	  && parse_bignum(&buffer, self->e)
	  && (mpz_cmp_ui(self->e, 1) > 0)
	  && (mpz_cmp(self->e, self->method->G->order) < 0)
	  && parse_eod(&buffer) );
}

void dh_hash_update(struct diffie_hellman_instance *self,
		    struct lsh_string *packet)
{
  HASH_UPDATE(self->hash, packet->length, packet->data);
}

/* Hashes server key, e and f */
void dh_hash_digest(struct diffie_hellman_instance *self, UINT8 *digest)
{
  struct lsh_string s = ssh_format("%S%n%n",
				   self->server_key,
				   self->e, self->f);

  HASH_UPDATE(self->hash, s->length, s->data);
  lsh_string_free(s);

  HASH_DIGEST(hash, digest);
}

struct lsh_string *dh_make_server_msg(struct diffie_hellman_instance *self,
				      struct signer *s)
{
  UINT8 *digest;
  
  dh_generate_secret(self, self->f);

  digest = alloca(self->hash->hash_size);
  dh_digest(self, digest);

  return ssh_format("%c%S%n%fS",
		    SSH_MSG_KEXDH_REPLY,
		    self->server_key,
		    self->f, SIGN(s, self->hash->hash_size, digest));
}

int dh_process_server_msg(struct diffie_hellman_instance *self,
			  struct lsh_string *packet)
{
  struct simple_buffer buffer;
  UINT8 msg_number;
  
  simple_buffer_init(&buffer, packet->length, packet->data);

  return (parse_uint8(&buffer, &msg_number)
	  && (msg_number == SSH_MSG_KEXDH_REPLY)
	  && (res->server_key = parse_string_copy(&buffer))
	  && (parse_bignum(&buffer, res->f))
	  && (mpz_cmp_ui(self->f, 1) > 0)
	  && (mpz_cmp(self->f, self->method->G->order) < 0)
	  && (res->signature = parse_string_copy(&buffer))
	  && parse_eod(&buffer));
}
	  
int dh_verify_server_msg(struct diffie_hellman_instance *self,
			 struct verifier *v)
{
  UINT8 *digest;
  
  digest = alloca(self->hash->hash_size);
  dh_digest(self, digest);

  return VERIFY(v, self->hash->hash_size, digest,
		self->signature->length, self->signature->data);
}
