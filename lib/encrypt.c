#include <openssl/ssl.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include "aff4.h"

#define OpenSSL_error                           \
  RaiseError(ERuntimeError, "%s", ERR_error_string(ERR_get_error(), NULL))

// Abstract type which is a base type for all ciphers.
VIRTUAL(AFF4Cipher, RDFValue) {
} END_VIRTUAL

SecurityProvider AFF4_SECURITY_PROVIDER=NULL;

/* The key cache is a store of keys for each encrypted stream. We need
   this because some cipher encoders have no way to decode the key
   (e.g. when encrypting we only need to the x509 pub key and may not
   have the private key.

   When decoding or encoding the key, all Ciphers will look in the key
   cache first for the required key. If it is found they will not
   make a new key but reuse the old one.

   This technique allows two different ciphers to encode the same key
   in different ways. For example suppose a new Encrypted stream is
   created:

   cipher = oracle.new_rdfvalue(AFF4_AES256_PASSWORD)
   oracle.set_value(encoded_urn, AFF4_CIPHER, cipher,0)

   Since there is no key set at this point, the cipher will make a new
   key and serialise the key using the password protocol. If we wish
   to encode the same key using x509 now:

   cipher = oracle.new_rdfvalue(AFF4_AES256_X509)
   oracle.set_value(encoded_urn, AFF4_CIPHER, cipher,0)

   This will now ensure that the very same key is encoded using the
   cert scheme because the key is already in cache. A new key will not
   be generated and the same key is reused.

   Similarly, suppose an Encrypted stream is encoded using
   AFF4_AES256_PASSWORD scheme already. We have a public x509 cert and
   we wish to encode using that:

   encrypted = oracle.open(urn, 'r')

   This will open the encrypted stream, and unlock the key (using the
   passphrase interface for SecurityProvider. Now we just do:

   cipher = oracle.new_rdfvalue(AFF4_AES256_X509)
   encrypted.add(AFF4_CIPHER, cipher)

   Since the key is already unlocked and in the cache above, the new
   cipher will just get it from the cache and encode it using the cert
   scheme.
*/
static Cache KeyCache = NULL;

static Key Key_Con(Key self, char *type, RDFURN subject, int create) {
  Object iter = CALL(KeyCache, iter, ZSTRING(subject->value));

  while(iter) {
    Key key = (Key)CALL(KeyCache, next, &iter);
    if(key && !strcmp(type, key->type)) {
      return key;
    };
  };

  if(!create) goto error;

  // If we get here there is no key in the cache:
  // Make a new key which should be big enough for everything
  self->data.dsize = EVP_MAX_KEY_LENGTH;
  self->data.dptr = talloc_size(self, self->data.dsize);
  self->iv.dsize = EVP_MAX_KEY_LENGTH;
  self->iv.dptr = talloc_size(self, self->iv.dsize);
  self->type = talloc_strdup(self, type);

  if(RAND_bytes(self->data.dptr, self->data.dsize) !=1 &&
     RAND_pseudo_bytes(self->data.dptr, self->data.dsize) != 1) {
    RaiseError(ERuntimeError, "Unable to make random key");
    goto error;
  };

  if(RAND_bytes(self->iv.dptr, self->iv.dsize) !=1 &&
     RAND_pseudo_bytes(self->iv.dptr, self->iv.dsize) != 1) {
    RaiseError(ERuntimeError, "Unable to make random key");
    goto error;
  };

  // Now add ourselves to the key cache
  CALL(KeyCache, put, ZSTRING(subject->value), (Object)self);

  return self;

 error:
  talloc_free(self);
  return NULL;
};

VIRTUAL(Key, Object) {
  VMETHOD(Con) = Key_Con;
} END_VIRTUAL


/** The following is information which should be serialised - its
    public and not secret at all */
struct aff4_cipher_data_t {
  unsigned char iv[AES_BLOCK_SIZE];
  unsigned char nonce[AES_BLOCK_SIZE];

  /* The nonce is the master key encrypted using the
     PKCS5_PBKDF2_HMAC_SHA1 generated key
  */
  unsigned char key[AES256_KEY_SIZE];
};

/* When we serialise the keys we need to encrypt them with the
   passphrase.

   We first come up with a random IV, then derive a round number. A
   new password key is derived using PKCS5_PBKDF2_HMAC_SHA1. Finally
   the password key is used to encrypt the master key and a nonce of
   zeros.

   We serialize the struct:
   unsigned char iv[16]
   unsigned char nonce[16]
   unsigned char key[32]

*/
static char *AES256Password_serialise(RDFValue self, RDFURN subject) {
  AES256Password this = (AES256Password)self;
  AFF4Cipher pthis = (AFF4Cipher)self;
  char *result;
  char *passphrase;
  uint32_t round_number;
  struct aff4_cipher_data_t encoding;
  unsigned char tmp_key[AES256_KEY_SIZE];
  AES_KEY password_key;

  if(!pthis->master_key)
    pthis->master_key = CONSTRUCT(Key, Key, Con, self,                  \
                                  ((AFF4Cipher)self)->type, subject, 1);

  // Set our encryption keys from the master key:
  AES_set_encrypt_key(pthis->master_key->data.dptr, AES256_KEY_SIZE * 8, &this->ekey);
  AES_set_decrypt_key(pthis->master_key->data.dptr, AES256_KEY_SIZE * 8, &this->dkey);

  // Start fresh
  memset(&encoding, 0, sizeof(encoding));

  // We just reuse the same IV as the master key
  memcpy(encoding.iv, pthis->master_key->iv.dptr, sizeof(encoding.iv));

  passphrase = CALL(AFF4_SECURITY_PROVIDER, passphrase, self->dataType, subject);
  if(!passphrase) {
    RaiseError(ERuntimeError, "No password provided");
    goto error;
  };

  // Now make the key
  round_number = ((uint32_t)encoding.iv[0]) << 8;
  PKCS5_PBKDF2_HMAC_SHA1(ZSTRING_NO_NULL(passphrase),
                         encoding.iv, sizeof(encoding.iv),
                         round_number,
                         sizeof(tmp_key), tmp_key);

  AES_set_encrypt_key(tmp_key, AES256_KEY_SIZE * 8, &password_key);

  // Encrypt the master key with the password key
  AES_cbc_encrypt(pthis->master_key->data.dptr, encoding.key,
                  AES256_KEY_SIZE, &password_key,
                  encoding.iv, AES_ENCRYPT);

  // Recopy the iv back (the iv is updated in CBC mode)
  memcpy(encoding.iv, pthis->master_key->iv.dptr, sizeof(encoding.iv));

  // The nonce is just a bunch of zeros encrypted using the master key
  {
    unsigned char buff[AES256_KEY_SIZE];
    memset(buff, 0, AES256_KEY_SIZE);

    AES_cbc_encrypt(buff, encoding.nonce,
                    sizeof(encoding.nonce), &password_key,
                    encoding.iv, AES_ENCRYPT);

    memcpy(encoding.iv, pthis->master_key->iv.dptr, sizeof(encoding.iv));
  };

  // We just serialise a base64 encoded version of the encoding struct:
  result = talloc_zero_size(self, sizeof(encoding) * 2);
  encode64((unsigned char *)&encoding, sizeof(encoding),
           result, sizeof(encoding) * 2);

  return result;

 error:
  return NULL;
};

/** We do not consider the universal TDB resolver a secure
    storage. This is because the TDB files just live in the user's
    home directory unencrypted. Therefore we only store the IV and
    nonce in the resolver, and re-derive the key from the passphrase
    each time we decode it.

    Hopefully this wont be too slow as we should not be needed to
    retrieve the cipher from the resolver that often. Just in case we
    also cache it locally in the key_cache;
*/
static int AES256Password_decode(RDFValue self, char *data, int length,
                                 RDFURN subject) {
  AES256Password this = (AES256Password)self;
  AFF4Cipher pthis = (AFF4Cipher)self;
  char *passphrase;
  uint32_t round_number;
  struct aff4_cipher_data_t encoding;
  unsigned char tmp_key[AES256_KEY_SIZE];
  AES_KEY password_key;
  unsigned char iv[AES256_KEY_SIZE];

  // First try to pull the master key from the key cache - Note we do
  // not create it here if it does not exist.
  if(!pthis->master_key)
    pthis->master_key = CONSTRUCT(Key, Key, Con, self,                  \
                                  ((AFF4Cipher)self)->type, subject, 0);

  // If we dont have a cached copy:
  if(!pthis->master_key) {
    // Now decode the encoding struct
    length = decode64(data, length,
                      (unsigned char *)&encoding, sizeof(encoding));
    if(length != sizeof(struct aff4_cipher_data_t)) {
      RaiseError(ERuntimeError, "Invalid data to decode");
      goto error;
    };

    // Make a copy of the iv
    memcpy(iv, encoding.iv, sizeof(encoding.iv));

    passphrase = CALL(AFF4_SECURITY_PROVIDER, passphrase, self->dataType, subject);
    if(!passphrase) {
      RaiseError(ERuntimeError, "No password provided");
      goto error;
    };

    // Now make the password key
    round_number = ((uint32_t)encoding.iv[0]) << 8;
    PKCS5_PBKDF2_HMAC_SHA1(ZSTRING_NO_NULL(passphrase),
                           encoding.iv, sizeof(encoding.iv),
                           round_number,
                           sizeof(tmp_key), tmp_key);

    AES_set_decrypt_key(tmp_key, AES256_KEY_SIZE * 8, &password_key);

    //Check the nonce now:
    {
      unsigned char buff[sizeof(encoding.nonce)];
      int i;

      AES_cbc_encrypt(encoding.nonce, buff, sizeof(buff), &password_key,
                      encoding.iv, AES_DECRYPT);

      for(i=0; i<sizeof(encoding.nonce); i++) {
        if(buff[i] != 0) {
        RaiseError(ERuntimeError,"Nonce does not decrypt");
        goto error;
        };
      };
    };

    // If we get here - it all checks ok. We create a new master_key
    // in the key cache and update its key. Note that the Key
    // constructor will put this very object in the cache for us so we
    // can modify this very object to update the cache.
    pthis->master_key = CONSTRUCT(Key, Key, Con, self,
                                  ((AFF4Cipher)self)->type, subject, 1);

    // Copy the IV from the encoding
    memcpy(pthis->master_key->iv.dptr, iv, pthis->master_key->iv.dsize);

    // Decrypt the master key using the password_key - note this
    // updates the iv due to the cbc mode:
    AES_cbc_encrypt(encoding.key, pthis->master_key->data.dptr, AES256_KEY_SIZE,
                    &password_key, iv, AES_DECRYPT);
  };

  // Set our master key from the cached copy
  AES_set_encrypt_key(pthis->master_key->data.dptr, AES256_KEY_SIZE * 8, &this->ekey);
  AES_set_decrypt_key(pthis->master_key->data.dptr, AES256_KEY_SIZE * 8, &this->dkey);

  return AES256_KEY_SIZE;

 error:
  return 0;
};

int AES256Password_encrypt(AFF4Cipher self, int chunk_number,
                           unsigned char *inbuff,
                           unsigned long int inlen,
                           unsigned char *outbuff,
                           unsigned long int length) {
  AFF4Cipher this = (AFF4Cipher)self;
  AES256Password pthis = (AES256Password)self;
  unsigned char iv[AES_BLOCK_SIZE];

  memcpy(iv, this->master_key->iv.dptr, sizeof(iv));

  // The block IV is made by xoring the iv with the chunk number
  *(uint32_t *)iv ^= chunk_number;

  AES_cbc_encrypt(inbuff, outbuff, length, &pthis->ekey, iv, AES_ENCRYPT);

  return length;
};

int AES256Password_decrypt(AFF4Cipher self, int chunk_number,
                           unsigned char *inbuff,
                           unsigned long int inlen,
                           unsigned char *outbuf,
                           unsigned long int length) {
  AFF4Cipher this = (AFF4Cipher)self;
  AES256Password pthis = (AES256Password)self;
  unsigned char iv[AES_BLOCK_SIZE];

  memcpy(iv, this->master_key->iv.dptr, sizeof(iv));

  // The block IV is made by xoring the iv with the chunk number
  *(uint32_t *)iv ^= chunk_number;

  AES_cbc_encrypt(inbuff, outbuf, length, &pthis->dkey, iv, AES_DECRYPT);

  return length;
};

VIRTUAL(AES256Password, AFF4Cipher) {
     VMETHOD_BASE(RDFValue, dataType) = AFF4_AES256_PASSWORD;
     VMETHOD_BASE(RDFValue, flags) = RESOLVER_ENTRY_ENCODED_SAME_AS_SERIALIZED;
     VMETHOD_BASE(RDFValue, id) = 0;

     VMETHOD_BASE(RDFValue, serialise) = AES256Password_serialise;
     VMETHOD_BASE(RDFValue, decode) = AES256Password_decode;

     VMETHOD_BASE(AFF4Cipher, type) = "aes256";
     VMETHOD_BASE(AFF4Cipher, blocksize) = AES_BLOCK_SIZE;
     VMETHOD_BASE(AFF4Cipher, encrypt) = AES256Password_encrypt;
     VMETHOD_BASE(AFF4Cipher, decrypt) = AES256Password_decrypt;
} END_VIRTUAL


static int check_keys(EVP_PKEY *privkey, EVP_PKEY *pubkey) {
  char ptext[16];                     /* plaintext of a 128-bit
                                         message */
  unsigned char sig[1024];            /* signature; bigger than needed
                                         */
  unsigned int siglen = sizeof(sig);  /* length of signature */
  EVP_MD_CTX md;                      /* EVP message digest */

  /* make the plaintext message */
  memset(ptext,0,sizeof(ptext));
  strcpy(ptext,"Test Message");
  EVP_SignInit(&md,EVP_sha256());
  EVP_SignUpdate(&md,ptext,sizeof(ptext));
  EVP_SignFinal(&md,sig,&siglen,privkey);

  /* Verify the message */
  EVP_VerifyInit(&md,EVP_sha256());
  EVP_VerifyUpdate(&md,ptext,sizeof(ptext));
  if(EVP_VerifyFinal(&md,sig,siglen,pubkey)!=1){
    return 0;
  }

  return 1;
};

static int open_buffer(AES256X509 self, unsigned char *buff, int in_size, unsigned char *key) {
  char *pkey_pem;
  EVP_PKEY *privkey = NULL;
  BIO *bio;

  // Obtain the private key
  {
    // Now ask the SecurityProvider for the private key for this
    pkey_pem = CALL(AFF4_SECURITY_PROVIDER, x509_private_key, self->authority->name,
                    NULL);

    if(!pkey_pem) {
      RaiseError(ERuntimeError, "Unable to get private key for cert %s",
                 self->authority->name);
      goto error;
    };

    // Now try to parse it as an X509 private key
    bio = BIO_new_mem_buf(ZSTRING(pkey_pem));
    if(!bio) goto error;
    privkey = PEM_read_bio_PrivateKey(bio,NULL,0,0);
    BIO_free(bio);

    if(!privkey) {
      RaiseError(ERuntimeError, "SecurityProvider did not provide a valid private key");
      goto error;
    };
  };

  // Try to decode the sealed master key
  if(privkey) {
    unsigned char *iv = buff;
    unsigned char *i = buff + EVP_MAX_IV_LENGTH;
    EVP_CIPHER_CTX cipher_ctx;
    int size = in_size - EVP_MAX_IV_LENGTH;

    size = EVP_PKEY_size(privkey);
    if(!EVP_OpenInit(&cipher_ctx, EVP_aes_256_cbc(),i,
                     size, iv, privkey)) {
      OpenSSL_error;
      goto error;
    };

    i += size;
    size = in_size - (i-buff);
    if(!EVP_OpenUpdate(&cipher_ctx, key,
                       &size,i , size)) {
      OpenSSL_error;
      goto error;
    };

    i+=size;
    size = in_size - (i-buff);
    if(!EVP_OpenFinal(&cipher_ctx, i, &size)) {
      OpenSSL_error;
      goto error;
    };
  };

  return 1;

 error:
  return 0;
};

/* Utility function to seal a key (encrypt it using the public key) */
static int seal_buffer(AES256X509 self, EVP_PKEY *pubkey,
                       unsigned char *inbuff, int in_size,
                       unsigned char *outbuff, int *outbuff_size) {
  EVP_CIPHER_CTX cipher_ctx;
  unsigned char *ek_array[2];
  int ek_size = *outbuff_size;
  unsigned char *wrt_ptr = outbuff + EVP_MAX_IV_LENGTH;
  unsigned char *iv_array = outbuff;

  ek_array[0] = wrt_ptr;
  if(!EVP_SealInit(&cipher_ctx, EVP_aes_256_cbc(),ek_array,&ek_size,
                   iv_array, &pubkey,1)) {
    OpenSSL_error;
    goto error;
  };

  wrt_ptr += ek_size;
  ek_size = in_size;

  // Now encrypt the master key
  if(!EVP_SealUpdate(&cipher_ctx,
                     wrt_ptr, &ek_size,
                     inbuff, in_size)) {
    OpenSSL_error;
    goto error;
  };

  wrt_ptr += ek_size;
  ek_size = *outbuff_size - (wrt_ptr - outbuff);

  // Finish up
  if(!EVP_SealFinal(&cipher_ctx, wrt_ptr, &ek_size)) {
    OpenSSL_error;
    goto error;
  };

  wrt_ptr += ek_size;
  *outbuff_size = wrt_ptr - outbuff;

  return *outbuff_size;

 error:
  return 0;
};

/**
   This uses X509 sealing to seal the private key.  Sealing is
   performed with openssl EVP_Seal* API. The data structure we produce
   is:

   unsigned char *IV;
   unsigned char *sealing_iv;
   unsigned char *sealed_buffer;

   The IV is a constant IV which is the basis of the block encryption
   (and is the same for all cipher objects and encodings). The act of
   sealing creates a sealing IV and stores the envelope in a sealed
   buffer.

   The encoded result is "cert_url#base_64_encoded_data"
*/
static char *AES256X509_serialise(RDFValue self, RDFURN subject) {
  AES256X509 xthis = (AES256X509)self;
  AFF4Cipher pthis = (AFF4Cipher)self;
  EVP_PKEY *pubkey;
  unsigned char buff[BUFF_SIZE];
  int buff_size = BUFF_SIZE;

  if(!xthis->authority) {
    RaiseError(ERuntimeError, "No certificate set - you must call set_authority() before setting this RDFValue");
    goto error;
  };

  // Get the key for encrypting this stream
  if(!pthis->master_key)
    pthis->master_key = CONSTRUCT(Key, Key, Con, self,                  \
                                  ((AFF4Cipher)self)->type, subject, 1);

  /** We serialise like this:
      unsigned char master_iv[]
      unsigned char sealing_iv[]
      unsigned char sealed_buffer[]
  */
  memset(buff, 0, sizeof(buff));
  memcpy(buff, pthis->master_key->iv.dptr, pthis->master_key->iv.dsize);

  pubkey = X509_get_pubkey(xthis->authority);
  if(!pubkey) {
    RaiseError(ERuntimeError, "Unable to get public key from cert %s", xthis->authority->name);
    goto error;
  };

  if(!seal_buffer(xthis, pubkey,
                  pthis->master_key->data.dptr,
                  pthis->master_key->data.dsize,
                  buff + EVP_MAX_IV_LENGTH, &buff_size))
    goto error;

  // Serialise into the location
  {
    char out_buff[BUFF_SIZE];
    int out_buff_size = BUFF_SIZE;

    encode64(buff, EVP_MAX_IV_LENGTH + buff_size, out_buff, out_buff_size);

    xthis->location->parser->fragment = talloc_strdup(xthis->location->parser, out_buff);
  };

  return CALL(xthis->location->parser, string, NULL);

 error:
  return NULL;
};

static int AES256X509_decode(RDFValue self, char *data, int length,
                                 RDFURN subject) {
  AES256X509 xthis = (AES256X509)self;
  AFF4Cipher pthis = (AFF4Cipher)self;
  RDFURN cert_URN = new_RDFURN(self);
  unsigned char buff[BUFF_SIZE];
  int buff_length;
  char *pkey_pem;
  EVP_PKEY *privkey;
  BIO *bio;

  memset(buff, 0, sizeof(buff));
  CALL(cert_URN, set, data);

  // Decode the fragment into the buffer:
  buff_length = decode64(ZSTRING_NO_NULL(cert_URN->parser->fragment), buff, sizeof(buff)-1);

  // Now remove the fragment and set our public key from this URL
  cert_URN->parser->fragment = "";
  {
    char *new_location = CALL(cert_URN->parser, string, cert_URN);
    int res;
    RDFURN new_urn = new_RDFURN(new_location);

    CALL(new_urn, set, new_location);
    res = CALL(xthis, set_authority, new_urn);

    if(!res) goto error;
  };

  // Get the key for encrypting this stream from cache
  if(!pthis->master_key)
    pthis->master_key = CONSTRUCT(Key, Key, Con, self,                  \
                                  ((AFF4Cipher)self)->type, subject, 0);

  if(pthis->master_key) return length;

  // Now ask the SecurityProvider for the private key for this
  pkey_pem = CALL(AFF4_SECURITY_PROVIDER, x509_private_key, xthis->authority->name,
                  subject);

  if(!pkey_pem) {
    RaiseError(ERuntimeError, "Unable to get private key for cert %s",
               xthis->authority->name);
    goto error;
  };

  // Now try to parse it as an X509 private key
  bio = BIO_new_mem_buf(ZSTRING(pkey_pem));
  if(!bio) goto error;
  privkey = PEM_read_bio_PrivateKey(bio,NULL,0,0);
  BIO_free(bio);

  if(!privkey) {
    RaiseError(ERuntimeError, "SecurityProvider did not provide a valid private key");
    goto error;
  };

  // Create a new Key:
  pthis->master_key = CONSTRUCT(Key, Key, Con, self,    \
                                ((AFF4Cipher)self)->type, subject, 1);

  // Set the master key
  memcpy(pthis->master_key->iv.dptr, buff, pthis->master_key->iv.dsize);
  if(!open_buffer(xthis, buff + EVP_MAX_IV_LENGTH, buff_length - EVP_MAX_IV_LENGTH,
                  pthis->master_key->data.dptr))
    goto error;

  talloc_free(cert_URN);
  return length;

 error:
  talloc_free(cert_URN);
  return 0;
};


static int AES256X509_set_authority(AES256X509 self, RDFURN location) {
  FileLikeObject fd = (FileLikeObject)CALL(oracle, open, location, 'r');
  BIO *bio;
  char *buff;

  // Make sure the file is a reasonable size
  if(!fd) goto error;
  if(fd->size->value > 1024*1024) goto msg_error;

  buff = CALL(fd, get_data);
  if(!buff) goto error;

  // Now try to parse it as an X509 cert
  bio = BIO_new_mem_buf(buff, fd->size->value);
  if(!bio) goto error;
  self->authority = PEM_read_bio_X509(bio,NULL,0,0);
  BIO_free(bio);

  if(!self->authority) goto msg_error;

  self->location = CALL(location, copy, self);

  return 1;

  msg_error:
    RaiseError(ERuntimeError, "Location %s does not appear to contain a PEM encoded X509 certificate", location->value);

 error:
  return 0;
};

  /** X509 base certs */
VIRTUAL(AES256X509, AES256Password) {
  VMETHOD_BASE(RDFValue, dataType) = AFF4_AES256_X509;
  VMETHOD_BASE(RDFValue, id) = 0;
  VMETHOD_BASE(RDFValue, raptor_literal_datatype) = NULL;

  VMETHOD_BASE(RDFValue, serialise) = AES256X509_serialise;
  VMETHOD_BASE(RDFValue, decode) = AES256X509_decode;

  VMETHOD_BASE(AES256X509, set_authority) = AES256X509_set_authority;
} END_VIRTUAL

  /** Following is the implementation of the Encrypted stream */
static AFFObject Encrypted_Con(AFFObject self, RDFURN uri, char mode) {
  Encrypted this = (Encrypted)self;

  // Call our baseclass
  self = SUPER(AFFObject, FileLikeObject, Con, uri, mode);

  if(self && uri) {
    URNOF(self) = CALL(uri, copy, self);

    this->chunk_size = new_XSDInteger(self);
    this->stored = new_RDFURN(self);
    this->backing_store = new_RDFURN(self);
    this->block_buffer = CONSTRUCT(StringIO, StringIO, Con, self);

    // Some defaults
    this->chunk_size->value = 4*1024;

    if(!CALL(oracle, resolve_value, uri, AFF4_TARGET, (RDFValue)this->backing_store)) {
      RaiseError(ERuntimeError, "Encrypted stream must have a backing store?");
      goto error;
    } else {
      //Make sure we can open it
      FileLikeObject fd = (FileLikeObject)CALL(oracle, open, this->backing_store, mode);
      if(!fd) goto error;
      CALL((AFFObject)fd, cache_return);
    };

    if(!CALL(oracle, resolve_value, uri, AFF4_STORED, (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "Encrypted stream must be stored somewhere?");
      goto error;
    };

    this->cipher = (AFF4Cipher)CALL(oracle, resolve_alloc, self, uri, AFF4_CIPHER);
    if(!this->cipher) {
      RaiseError(ERuntimeError, "Unable to resolve a cipher for Encrypted stream");
      goto error;
    };

    // Add ourselves to our volume
    CALL(oracle, add_value, this->stored, AFF4_VOLATILE_CONTAINS, (RDFValue)URNOF(self),0);

    CALL(oracle, resolve_value, URNOF(self), AFF4_CHUNK_SIZE,
	 (RDFValue)this->chunk_size);

    CALL(oracle, resolve_value, URNOF(self), AFF4_SIZE,
	 (RDFValue)((FileLikeObject)this)->size);
  };

  return self;

 error:
  talloc_unlink(NULL, self);
  return NULL;
};

int Encrypted_write(FileLikeObject self, char *buffer, unsigned long int len) {
  Encrypted this = (Encrypted)self;
  int offset = 0;
  int chunk_size = this->chunk_size->value;
  int chunk_id = self->readptr / chunk_size;

  CALL(this->block_buffer, seek, 0, SEEK_END);
  CALL(this->block_buffer, write, buffer, len);
  self->readptr += len;

  if(this->block_buffer->size >= chunk_size) {
    unsigned char buff[chunk_size];
    FileLikeObject backing_store = (FileLikeObject)CALL(oracle,
                                 open, this->backing_store, 'w');
    if(!backing_store) {
      RaiseError(ERuntimeError, "Unable to open backing store %s",
                 this->backing_store->value);
      goto error;
    };

    // Now take chunks off the buffer and write them to the backing
    // stream:
    for(offset = 0; this->block_buffer->size - offset >= chunk_size;
        offset += chunk_size) {

      CALL(this->cipher, encrypt, chunk_id,
           (unsigned char *)this->block_buffer->data + offset,
           chunk_size, buff, chunk_size);
      chunk_id ++;

      CALL(backing_store, write,(char *) buff, chunk_size);
    };

    CALL(oracle, cache_return, (AFFObject)backing_store);
  };

  // Clear the data we used up
  CALL(this->block_buffer, skip, offset);

  return len;
 error:
  return 0;
};

static int Encrypted_partialread(FileLikeObject self, char *buff, unsigned long int len) {
  Encrypted this = (Encrypted)self;
  uint64_t chunk_size = this->chunk_size->value;
  uint32_t chunk_id = self->readptr / chunk_size;
  int chunk_offset = self->readptr % chunk_size;
  int available_to_read = min(len, chunk_size - chunk_offset);
  unsigned char cbuff[chunk_size];
  unsigned char dbuff[chunk_size];
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, this->backing_store, 'r');
  int res;

  if(!target) {
    goto error;
  };

  CALL(target, seek, chunk_id * chunk_size, SEEK_SET);
  res = CALL(target, read, (char *)cbuff, chunk_size);
  if(res < 0 || res < chunk_size){
    PUSH_ERROR_STATE;
    CALL((AFFObject)target, cache_return);
    POP_ERROR_STATE;
    goto error;
  };
  CALL(oracle, cache_return, (AFFObject)target);

  // Now decrypt the buffer in place
  if(!CALL(this->cipher, decrypt, chunk_id, cbuff, chunk_size, dbuff, chunk_size))
    goto error;

  // Return the available data
  memcpy(buff, dbuff + chunk_offset, available_to_read);

  self->readptr += available_to_read;

  return available_to_read;

 error:
  return -1;
};

static int Encrypted_read(FileLikeObject self, char *buff, unsigned long int length) {
  int read_length;
  int offset=0;

  // Just execute as many partial reads as are needed to satisfy the
  // length requested
  while(offset < length ) {
    int available_to_read = min(length, self->size->value - self->readptr);

    if(available_to_read == 0) break;

    read_length = Encrypted_partialread(self, buff + offset, available_to_read);
    if(read_length <0) return -1;
    if(read_length ==0) break;
    offset += read_length;
    self->readptr += read_length;
  };

  return offset;
};

static int Encrypted_close(AFFObject aself) {
  FileLikeObject self = (FileLikeObject)aself;
  Encrypted this = (Encrypted)self;
  int chunk_size = this->chunk_size->value;
  char buff[chunk_size];
  int to_pad = chunk_size - self->readptr % chunk_size;

  // Pad the last chunk but do not adjust the size
  self->size->value = self->readptr;

  memset(buff, 0, chunk_size);
  if(to_pad > 0) {
    Encrypted_write(self, buff, to_pad);
  };

  CALL(oracle, set_value, URNOF(this), AFF4_TYPE,
       rdfvalue_from_urn(this, AFF4_ENCRYTED),0);

  CALL(oracle, set_value, URNOF(self), AFF4_SIZE,
       (RDFValue)((FileLikeObject)self)->size,0);

  {
    XSDDatetime time = new_XSDDateTime(this);

    gettimeofday(&time->value,NULL);
    CALL(oracle, set_value, URNOF(this), AFF4_TIMESTAMP,
	 (RDFValue)time,0);
  };

  // Finally we flush the key from the Key_Cache. If we reopen it for
  // reading we will need to re-decode it now.
  {
    Key key = (Key)CALL(KeyCache, get, ZSTRING(URNOF(this)->value));
    if(key) talloc_unlink(NULL, key);
  };

  return SUPER(AFFObject, FileLikeObject, close);
};


VIRTUAL(Encrypted, FileLikeObject) {
  VMETHOD_BASE(AFFObject, Con) = Encrypted_Con;
  VMETHOD_BASE(AFFObject, dataType) = AFF4_ENCRYTED;

  VMETHOD_BASE(FileLikeObject, write) = Encrypted_write;
  VMETHOD_BASE(FileLikeObject, read) = Encrypted_read;
  VMETHOD_BASE(AFFObject, close) = Encrypted_close;
} END_VIRTUAL;


/* This is the default security provider - Try to be somewhat useful */
static SecurityProvider SecurityProvider_Con(SecurityProvider self) {
  return self;
};

static char *SecurityProvider_passphrase(SecurityProvider self, char *cipher_type,
                                         RDFURN subject) {
  char *pass = getenv(AFF4_ENV_PASSPHRASE);

  if(pass) {
    /*
    AFF4_LOG(AFF4_LOG_MESSAGE, AFF4_SERVICE_CRYPTO_SUBSYS,
             subject,
             "Reading passphrase from enviornment");
    */
    return talloc_strdup(NULL, pass);
  } else {
    AFF4_LOG(AFF4_LOG_NONFATAL_ERROR, AFF4_SERVICE_ENCRYPTED_STREAM,
             subject, "No password set in environment variable "        \
             AFF4_ENV_PASSPHRASE);
    RaiseError(ERuntimeError, "No password set?");
  };

  return NULL;
};

static char *SecurityProvider_x509_private_key(SecurityProvider self,
                                               char *cert_name, RDFURN subject) {
  return "";
};

VIRTUAL(SecurityProvider, Object) {
  VMETHOD(Con) = SecurityProvider_Con;
  VMETHOD(passphrase) = SecurityProvider_passphrase;
  VMETHOD(x509_private_key) = SecurityProvider_x509_private_key;
} END_VIRTUAL

void encrypt_init() {
  // Initialise the SSL library must be done once:
  SSL_load_error_strings();
  SSL_library_init();

  INIT_CLASS(AFF4Cipher);
  INIT_CLASS(AES256Password);
  INIT_CLASS(AES256X509);
  INIT_CLASS(Encrypted);
  INIT_CLASS(Key);

  register_type_dispatcher(AFF4_ENCRYTED, (AFFObject *)GETCLASS(Encrypted));
  register_rdf_value_class((RDFValue)GETCLASS(AES256Password));
  register_rdf_value_class((RDFValue)GETCLASS(AES256X509));

  // Initialise the provider with the default:
  AFF4_SECURITY_PROVIDER = CONSTRUCT(SecurityProvider, SecurityProvider, Con, NULL);

  KeyCache = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 0);
};
