#include <openssl/ssl.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include "aff4.h"

// Abstract type which is a base type for all ciphers.
VIRTUAL(AFF4Cipher, RDFValue) {
} END_VIRTUAL

SecurityProvider provider=NULL;

/* When we serialise the keys we need to encrypt them with the
   passphrase.
*/
static char *AES256Password_serialise(RDFValue self, RDFURN subject) {
  AES256Password this = (AES256Password)self;
  char *result;
  unsigned char key[AES256_KEY_SIZE];

  if(!this->pub) {
    char *passphrase;
    uint32_t round_number;

    this->pub = talloc(self, struct aff4_cipher_data_t);

    // Make up some random keys and IVs
    if(RAND_bytes(this->pub->iv, sizeof(this->pub->iv)) !=1 &&
       RAND_pseudo_bytes(this->pub->iv, sizeof(this->pub->iv)) != 1) {
      RaiseError(ERuntimeError, "Unable to make random key");
      goto error;
    };

    passphrase = CALL(provider, passphrase, self->dataType, subject);
    if(!passphrase) {
      if(!_global_error) RaiseError(ERuntimeError, "No password provided");
      goto error;
    };

    // Now make the key
    round_number = ((uint32_t)this->pub->iv[0]) << 8;
    PKCS5_PBKDF2_HMAC_SHA1(ZSTRING_NO_NULL(passphrase),
                           this->pub->iv, sizeof(this->pub->iv),
                           round_number,
                           sizeof(key), key);

    AES_set_encrypt_key(key, sizeof(key) * 8, &this->ekey);
    AES_set_decrypt_key(key, sizeof(key) * 8, &this->dkey);

    // The nonce is the iv encrypted using the key from the password
    CALL((AFF4Cipher)self, encrypt, 0,
         this->pub->iv, sizeof(this->pub->iv),
         this->pub->nonce, sizeof(this->pub->nonce));
  };

  // Make enough room for encoding the iv and nonce:
  result = talloc_zero_size(self, sizeof(*this->pub) * 2);
  encode64((unsigned char *)this->pub, sizeof(*this->pub),
           (unsigned char *)result,
           sizeof(*this->pub) * 2);

  return result;

 error:
  talloc_free(this->pub);
  this->pub = NULL;
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
  unsigned char key[AES256_KEY_SIZE];
  char *passphrase;
  uint32_t round_number;
  unsigned char buff[sizeof(this->pub->nonce)];

  if(this->pub) {
    talloc_free(this->pub);
  };

  this->pub = talloc(self, struct aff4_cipher_data_t);

  // Decode the data into the pub:
  if(decode64(data, length, (unsigned char *)this->pub,
              sizeof(*this->pub)) != sizeof(*this->pub)) goto error;

  passphrase = CALL(provider, passphrase, self->dataType, subject);
  if(!passphrase) {
    if(!_global_error) RaiseError(ERuntimeError, "No password provided");
    goto error;
  };

  // Now make the key
  round_number = ((uint32_t)this->pub->iv[0]) << 8;
  PKCS5_PBKDF2_HMAC_SHA1(ZSTRING_NO_NULL(passphrase),
                         this->pub->iv, sizeof(this->pub->iv),
                         round_number,
                         sizeof(key), key);

  // Set up keys before we can encrypt anything
  AES_set_encrypt_key(key, sizeof(key) * 8, &this->ekey);
  AES_set_decrypt_key(key, sizeof(key) * 8, &this->dkey);

  // Now check that the key is right:
  CALL((AFF4Cipher)self, encrypt, 0,
       this->pub->iv, sizeof(this->pub->iv),
       buff, sizeof(this->pub->nonce));
  if(memcmp(buff, this->pub->nonce, sizeof(buff))) {
    RaiseError(ERuntimeError, "Password does not match");
    goto error;
  };

  return 1;
 error:
  return 0;
};

int AES256Password_encrypt(AFF4Cipher self, int chunk_number,
                           unsigned char *inbuff,
                           unsigned long int inlen,
                           unsigned char *outbuff,
                           unsigned long int length) {
  AES256Password this = (AES256Password)self;
  unsigned char iv[sizeof(this->pub->iv)];

  if(!this->pub) goto error;

  memcpy(iv, this->pub->iv, sizeof(this->pub->iv));

  // The block IV is made by xoring the iv with the chunk number
  *(uint32_t *)iv ^= chunk_number;

  AES_cbc_encrypt(inbuff, outbuff, length, &this->ekey, iv, AES_ENCRYPT);

  return length;
 error:
  RaiseError(ERuntimeError, "No key initialised??");
  return 0;
};

int AES256Password_decrypt(AFF4Cipher self, int chunk_number,
                           unsigned char *inbuff,
                           unsigned long int inlen,
                           unsigned char *outbuf,
                           unsigned long int length) {
  AES256Password this = (AES256Password)self;
  unsigned char iv[sizeof(this->pub->iv)];

  if(!this->pub) goto error;

  memcpy(iv, this->pub->iv, sizeof(this->pub->iv));

  // The block IV is made by xoring the iv with the chunk number
  *(uint32_t *)iv ^= chunk_number;

  AES_cbc_encrypt(inbuff, outbuf, length, &this->dkey, iv, AES_DECRYPT);

  return length;

 error:
  RaiseError(ERuntimeError, "No key initialised??");
  return 0;
};

VIRTUAL(AES256Password, AFF4Cipher) {
     VMETHOD_BASE(RDFValue, dataType) = AFF4_AES256_PASSWORD;
     VMETHOD_BASE(RDFValue, flags) = RESOLVER_ENTRY_ENCODED_SAME_AS_SERIALIZED;

     VMETHOD_BASE(RDFValue, serialise) = AES256Password_serialise;
     VMETHOD_BASE(RDFValue, decode) = AES256Password_decode;

     VMETHOD_BASE(AFF4Cipher, blocksize) = AES_BLOCK_SIZE;
     VMETHOD_BASE(AFF4Cipher, encrypt) = AES256Password_encrypt;
     VMETHOD_BASE(AFF4Cipher, decrypt) = AES256Password_decrypt;
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
    this->cipher = (AFF4Cipher)new_rdfvalue(self, AFF4_AES256_PASSWORD);

    // Some defaults
    this->chunk_size->value = 4*1024;

    if(!CALL(oracle, resolve_value, uri, AFF4_TARGET, (RDFValue)this->backing_store)) {
      RaiseError(ERuntimeError, "Encrypted stream must have a backing store?");
      goto error;
    };

    if(!CALL(oracle, resolve_value, uri, AFF4_STORED, (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "Encrypted stream must be stored somewhere?");
      goto error;
    };

    if(!CALL(oracle, resolve_value, uri, AFF4_CIPHER, (RDFValue)this->cipher)) {
      goto error;
    };

    // Add ourselves to our volume
    CALL(oracle, add_value, this->stored, AFF4_VOLATILE_CONTAINS, (RDFValue)URNOF(self));

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
  int available_to_read = chunk_size - chunk_offset;
  unsigned char cbuff[chunk_size];
  unsigned char dbuff[chunk_size];
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, this->backing_store, 'r');

  if(!target) {
    RaiseError(ERuntimeError, "Unable to open the backing stream %s", this->backing_store->value);
    goto error;
  };

  CALL(target, seek, chunk_id * chunk_size, SEEK_SET);
  if(CALL(target, read, (char *)cbuff, chunk_size) < chunk_size) 
    goto error;

  CALL(oracle, cache_return, (AFFObject)target);

  // Now decrypt the buffer in place
  if(!CALL(this->cipher, decrypt, chunk_id, cbuff, chunk_size, dbuff, chunk_size))
    goto error;

  // Return the available data
  memcpy(buff, cbuff + chunk_offset, available_to_read);

  self->readptr += available_to_read;

  return available_to_read;

 error:
  return -1;
};

static int Encrypted_read(FileLikeObject self, char *buff, unsigned long int length) {
  int read_length;
  int offset=0;

  // Clip the read to the stream size
  if(self->readptr > self->size->value) return 0;

  length = min(length, self->size->value - self->readptr);

  // Just execute as many partial reads as are needed to satisfy the
  // length requested
  while(offset < length ) {
    read_length = Encrypted_partialread(self, buff + offset, length - offset);
    if(read_length <=0) break;
    offset += read_length;
  };

  return length;
};

static int Encrypted_close(FileLikeObject self) {
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
       rdfvalue_from_urn(this, AFF4_ENCRYTED));

  CALL(oracle, set_value, URNOF(self), AFF4_SIZE,
       (RDFValue)((FileLikeObject)self)->size);

  {
    XSDDatetime time = new_XSDDateTime(this);

    gettimeofday(&time->value,NULL);
    CALL(oracle, set_value, URNOF(this), AFF4_TIMESTAMP,
	 (RDFValue)time);
  };

  return SUPER(FileLikeObject, FileLikeObject, close);
};


VIRTUAL(Encrypted, FileLikeObject) {
  VMETHOD_BASE(AFFObject, Con) = Encrypted_Con;
  VMETHOD_BASE(AFFObject, dataType) = AFF4_ENCRYTED;

  VMETHOD_BASE(FileLikeObject, write) = Encrypted_write;
  VMETHOD_BASE(FileLikeObject, read) = Encrypted_read;
  VMETHOD_BASE(FileLikeObject, close) = Encrypted_close;
} END_VIRTUAL;


/* This is the default security provider - Try to be somewhat useful */
SecurityProvider SecurityProvider_Con(SecurityProvider self) {
  return self;
};

char *SecurityProvider_passphrase(SecurityProvider self, char *cipher_type,
                                RDFURN subject) {
  char *pass = getenv(AFF4_ENV_PASSPHRASE);

  if(pass) {
    AFF4_LOG(AFF4_LOG_MESSAGE, AFF4_SERVICE_CRYPTO_SUBSYS,
             subject,
             "Reading passphrase from enviornment");

    return talloc_strdup(NULL, pass);
  } else {
    AFF4_LOG(AFF4_LOG_NONFATAL_ERROR, AFF4_SERVICE_ENCRYPTED_STREAM,
             subject, "No password set in environment variable "        \
             AFF4_ENV_PASSPHRASE);
    RaiseError(ERuntimeError, "No password set?");
  };

  return NULL;
};

VIRTUAL(SecurityProvider, Object) {
  VMETHOD(Con) = SecurityProvider_Con;
  VMETHOD(passphrase) = SecurityProvider_passphrase;
} END_VIRTUAL

void encrypt_init() {
  // Initialise the SSL library must be done once:
  SSL_load_error_strings();
  SSL_library_init();

  INIT_CLASS(AFF4Cipher);
  INIT_CLASS(AES256Password);
  INIT_CLASS(Encrypted);

  register_type_dispatcher(AFF4_ENCRYTED, (AFFObject *)GETCLASS(Encrypted));
  register_rdf_value_class((RDFValue)GETCLASS(AES256Password));

  // Initialise the provider with the default:
  provider = CONSTRUCT(SecurityProvider, SecurityProvider, Con, NULL);
};
