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

       int METHOD(AFF4Cipher, encrypt, unsigned char *inbuff, \
                  int inlen,\
                  OUT unsigned char *outbuf, int outlen, int chunk_number);
       int METHOD(AFF4Cipher, decrypt, unsigned char *inbuff, \
                  int inlen,\
                  OUT unsigned char *outbuf, int outlen, int chunk_number);
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
    password using the PBKDF2 algorithm.
*/
CLASS(AES256Password, AFF4Cipher)
  AES_KEY ekey;
  AES_KEY dkey;
  struct aff4_cipher_data_t pub;

  unsigned char key[AES256_KEY_SIZE];
  char encoded_key[sizeof(struct aff4_cipher_data_t) * 2];

  // Set the password for this object. Should only be called once
  // before using.
  BORROWED RDFValue METHOD(AES256Password, set, char *passphrase);

  // This callback can be overridden to fetch password to decode the
  // IV from. By default, we look in the AFF4_VOLATILE_PASSPHRASE
  // environment variable.
  int METHOD(AES256Password, fetch_password_cb);
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
