m4_dnl LSH testsuite driver
m4_dnl (progn (modify-syntax-entry ?� "(�") (modify-syntax-entry ?� ")�"))
m4_dnl (progn (modify-syntax-entry 187 "(�") (modify-syntax-entry 170 ")�"))
m4_dnl (progn (modify-syntax-entry ?{ "(}") (modify-syntax-entry ?} "){"))
m4_changequote(�, �)
m4_changecom(/*, */)
m4_define(�TS_DEFINE�, m4_defn(�m4_define�))

TS_DEFINE(�TS_WRITE�, �fputs("$1", stderr);�)
TS_DEFINE(�TS_MESSAGE�, �TS_WRITE($1... )�)
TS_DEFINE(�TS_OK�, �TS_MESSAGE(ok.\n)�)
TS_DEFINE(�TS_FAIL�, �{ TS_MESSAGE(failed.\n); exit(1); }�)

TS_DEFINE(�TS_TEST_STRING_EQ�, �
  {
    struct lsh_string *a, *b;
    TS_MESSAGE($1)
    a = $2;
    b = $3;
    if (!lsh_string_eq(a, b))
      TS_FAIL
    TS_OK
    lsh_string_free(a);
    lsh_string_free(b);
  }
�)

TS_DEFINE(�TS_STRING�,
�m4_ifelse(m4_index(�$1�, �"�), 0,
  �ssh_format("%lz", $1)�, �simple_decode_hex("m4_translit(�$1�, �0-9a-zA-Z 	#�, �0-9a-zA-Z�)")�) �)

m4_dnl TS_TEST_HASH(name, algorithm, data, digest)
TS_DEFINE(�TS_TEST_HASH�,
  �TS_TEST_STRING_EQ(�$1�, hash_string(�$2�, TS_STRING(�$3�)), TS_STRING(�$4�))�)

m4_dnl TS_TEST_CRYPTO(name, algorithm, key, clear, cipher)
TS_DEFINE(�TS_TEST_CRYPTO�, �
  {
    struct crypto_algorithm *algorithm = �$2�;
    struct lsh_string *key = TS_STRING(�$3�);

    assert(key->length == algorithm->key_size);
    assert(!algorithm->iv_size);

    TS_TEST_STRING_EQ(�Encrypting with $1�,
         �crypt_string(MAKE_ENCRYPT(algorithm, key->data, NULL),
		       TS_STRING(�$4�), 0)�,
         �TS_STRING(�$5�)�);
    TS_TEST_STRING_EQ(�Decrypting with $1�,
         �crypt_string(MAKE_DECRYPT(algorithm, key->data, NULL),
		       TS_STRING(�$5�), 0)�,
         �TS_STRING(�$4�)�);
    lsh_string_free(key);
  }
�)    

m4_divert(1)
  return 0;
} m4_divert

m4_dnl C code
#include "lsh.h"

#include "crypto.h"
#include "digits.h"
#include "format.h"
#include "xalloc.h"

#include <assert.h>
#include <stdio.h>

int main(int argc UNUSED, char **argv UNUSED)
{
