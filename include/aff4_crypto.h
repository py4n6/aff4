/*
** aff4_crypto.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Wed Dec 16 13:48:12 2009 mic
** Last update Wed Dec 16 13:48:30 2009 mic
*/

#ifndef   	AFF4_CRYPTO_H_
# define   	AFF4_CRYPTO_H_

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include "stringio.h"

/** This object represents an identify - like a person for example. 

An identity is someone who makes statements about other AFFObjects in
the universe.
*/
PRIVATE CLASS(Identity, AFFObject)
       Resolver info;

       EVP_PKEY *priv_key;
       EVP_PKEY *pub_key;
       X509 *x509;

       Identity METHOD(Identity, Con, char *cert, char *priv_key, char mode);
       void METHOD(Identity, store, char *volume_urn);
  /** This method asks the identity to verify its statements. This
       essentially populates our Resolver with statements which can be
       verified from our statements. Our Resolver can then be
       compared to the oracle to see which objects do not match.
  */
       void METHOD(Identity, verify, int (*cb)(uint64_t progress, char *urn));
END_CLASS

       /** This class implements a security provider. This is used by
           AFF4 ciphers to request passwords or certificates. Users of
           the library should implement their own security provider
           and install it using the
           Resolver.install_security_provider() function. When a
           cipher needs to obtain a passphrase or certificate they
           call here to obtain it - the user code can choose how to
           manage their own password (e.g. bring up an interactive
           GUI, use the OS secure storage facility etc).

           There is currently only a single global security provider.
       */
CLASS(SecurityProvider, Object)
     SecurityProvider METHOD(SecurityProvider, Con);

/* This is called when we need to obtain a passphrase for a subject
     stream. Returns 0 on error, and the length of the passphrase in
     len (which must initially contain the length of the passphrase
     buffer).
*/
     char *METHOD(SecurityProvider, passphrase,              \
                  char *cipher_type, struct RDFURN_t *subject);
END_CLASS

     /** Allow the user to make their own class */
PROXY_CLASS(SecurityProvider);

/************************************************************
  An implementation of the encrypted Stream.

  This stream encrypts and decrypts data from a target stream in
  blocks determined by the "block_size" attribute. The IV consists of
  the block number (as 32 bit int in little endian) appended to an 8
  byte salt.
*************************************************************/
#include <openssl/aes.h>

/** This is the abstract cipher class - all ciphers must implement
    these methods.
*/
CLASS(AFF4Cipher, RDFValue)
       int blocksize;

       int METHOD(AFF4Cipher, encrypt, int chunk_number,         \
                  unsigned char *inbuff,                         \
                  unsigned long int inlen,                       \
                  OUT unsigned char *outbuff,                     \
                  unsigned long int length);

       int METHOD(AFF4Cipher, decrypt, int chunk_number,         \
                  unsigned char *inbuff,                         \
                  unsigned long int inlen,                       \
                  OUT unsigned char *outbuff,                    \
                  unsigned long int length);
END_CLASS

#define AES256_KEY_SIZE 32

/** The following is information which should be serialised - its
    public and not secret at all */
struct aff4_cipher_data_t {
  unsigned char iv[AES_BLOCK_SIZE];

  // The nonce is the IV encrypted using the key
  unsigned char nonce[AES_BLOCK_SIZE];
};

/** The following are some default ciphers */

/** This cipher uses AES256. The session key is derived from a
    password using the PBKDF2 algorithm. The round number is derived
    from the iv: round_number = (unsigned char)iv[0] << 24

    Password is obtained from the security manager.
*/
CLASS(AES256Password, AFF4Cipher)
  AES_KEY ekey;
  AES_KEY dkey;
  struct aff4_cipher_data_t *pub;
END_CLASS

/**
   Encryption is handled via two components, the encrypted stream and
   the cipher. An encrypted stream is an FileLikeObject with the
   following AFF4 attributes:

   AFF4_CHUNK_SIZE = Data will be broken into these chunks and
                     encrypted independantly.

   AFF4_STORED     = Data will be stored on this backing stream.

   AFF4_CIPHER     = This is an RDFValue which extends the AFF4Cipher
                     class. More on that below.

   When opened, the FileLikeObject is created by instantiating a
   cipher from the AFF4_CIPHER attribute. Data is then divided into
   chunks and each chunk is encrypted using the cipher, and then
   written to the backing stream.

   A cipher is a class which extends the AFF4Cipher baseclass.

   Of course a valid cipher must also implement valid serialization
   methods and also some way of key initialization. Chunks must be
   whole multiples of blocksize.
*/
CLASS(Encrypted, FileLikeObject)
       StringIO block_buffer;
       AFF4Cipher cipher;
       RDFURN backing_store;
       RDFURN stored;
       XSDInteger chunk_size;
END_CLASS

void encrypt_init();

#endif 	    /* !AFF4_CRYPTO_H_ */
