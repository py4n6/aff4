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

     /* This method should return the location (URL) of the
        certificate that will be used to encrypt the data. */
     char *METHOD(SecurityProvider, x509_cert,  \
                  char *cipher_type, struct RDFURN_t *subject);

END_CLASS

     /** Allow the user to make their own class */
PROXY_CLASS(SecurityProvider);

extern SecurityProvider AFF4_SECURITY_PROVIDER;

/************************************************************
  An implementation of the encrypted Stream.

  This stream encrypts and decrypts data from a target stream in
  blocks determined by the "block_size" attribute. The IV consists of
  the block number (as 32 bit int in little endian) appended to an 8
  byte salt.
*************************************************************/
#include <openssl/aes.h>

CLASS(Key, Object)
    char *type;
    TDB_DATA data;
    TDB_DATA iv;
    /** Create a new key for the subject specified of the type
        specified.

        If create is specified we create a new key if it is not found
        in the cache.
    */
    Key METHOD(Key, Con, char *type, RDFURN subject, int create);
END_CLASS

/** This is the abstract cipher class - all ciphers must implement
    these methods.
*/
CLASS(AFF4Cipher, RDFValue)
       int blocksize;

       // A type string which identifies the cipher
       char *type;

       // This contains the key for this cipher
       Key master_key;

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

struct aff4_cipher_data_t;

/** The following are some default ciphers */

/**
   This cipher uses AES256. The session key is derived from a
   password using the PBKDF2 algorithm. The round number is derived
   from the iv: round_number = (unsigned char)iv[0] << 8

   The key is obtained from PKCS5_PBKDF2_HMAC_SHA1(password, iv, round_number).

   Password is obtained from the security manager.

   The serialised form is a base64 encoded version struct
   aff4_cipher_data_t (above). Where the nonce is the iv encrypted
   using the key.
**/
CLASS(AES256Password, AFF4Cipher)
  AES_KEY ekey;
  AES_KEY dkey;
END_CLASS

/**
   This cipher uses AES256.


   The serialised form is the url of the x509 certificate that was
   used to encrypt the key, with the base64 encoded struct
   aff4_cipher_data_t being encoded as the query string. The security
   manager is used to get both the certificate and the private keys.
**/
CLASS(AES256X509, AES256Password)
/**
    This method is used to set the authority (X509 certificate) of
    this object. The location is a URL for the X509 certificate.

    return 1 on success, 0 on failure.
**/
     X509 *authority;
     RDFURN location;
     int METHOD(AES256X509, set_authority, RDFURN location);
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
