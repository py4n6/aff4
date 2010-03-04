#include <openssl/ssl.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include "aff4.h"

VIRTUAL(AFF4Cipher, RDFValue) {
} END_VIRTUAL

CLASS(KeyCache, Object)
     unsigned char key[AES256_KEY_SIZE];

     KeyCache METHOD(KeyCache, Con, char *passphrase, int round_number,
                     unsigned char *iv, int len);
END_CLASS

KeyCache KeyCache_Con(KeyCache self, char *passphrase, int round_number,
                      unsigned char *iv, int len) {
  // Now make the key
  PKCS5_PBKDF2_HMAC_SHA1(ZSTRING_NO_NULL(passphrase),
                         iv, len,
                         round_number,
                         sizeof(self->key), self->key);

  return self;
};

VIRTUAL(KeyCache, Object) {
  VMETHOD(Con) = KeyCache_Con;
} END_VIRTUAL


static Cache key_cache;

  // Create a new cipher
static RDFValue AES256Password_Con(RDFValue self) {
  AES256Password this = (AES256Password)self;
  // Make up some random keys and IVs
  if(RAND_bytes(this->pub.iv, sizeof(this->pub.iv)) !=1 &&
     RAND_pseudo_bytes(this->pub.iv, sizeof(this->pub.iv)) != 1) {
    goto error;
  };

  return self;

 error:
  RaiseError(ERuntimeError, "Unable to make random key");
  talloc_free(self);
  return NULL;
};

// We set the password, we basically just copy it here for later
static RDFValue AES256Password_set(AES256Password self, char *passphrase) {
  KeyCache key = (KeyCache)CALL(key_cache, get,
                                (char *)self->pub.iv, sizeof(self->pub.iv));

  // Not cached - make it:
  if(!key) {
    uint32_t round_number;

    // Round number is between 2**8 and 2**24
    round_number = (*(uint32_t *)self->pub.iv & 0xFF) << 8;

    key = CONSTRUCT(KeyCache, KeyCache, Con, self, passphrase, round_number,
                    self->pub.iv, sizeof(self->pub.iv));
  };

  AES_set_encrypt_key(self->key, sizeof(self->key) * 8, &self->ekey);
  AES_set_decrypt_key(self->key, sizeof(self->key) * 8, &self->dkey);

  // We now cache the key for that IV
  CALL(key_cache, put, (char *)self->pub.iv, sizeof(self->pub.iv),
       (Object)key);

  return (RDFValue)self;
};

/* When we serialise the keys we need to encrypt them with the
   passphrase.
*/
static char *AES256Password_serialise(RDFValue self) {
  AES256Password this = (AES256Password)self;
  char encoded_key[sizeof(struct aff4_cipher_data_t) * 2];

  // Update the nonce
  CALL((AFF4Cipher)this, encrypt, this->pub.iv,
       sizeof(this->pub.iv),
       this->pub.nonce, sizeof(this->pub.nonce), 0);

  // Encode the key
  encode64((unsigned char *)&this->pub, sizeof(this->pub), (unsigned char *)encoded_key,
           sizeof(encoded_key));

  return talloc_strdup(self, encoded_key);
};

// We store the most important part of this object
static TDB_DATA *AES256Password_encode(RDFValue self, RDFURN subject) {
  AES256Password this = (AES256Password)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)&this->pub;
  result->dsize = sizeof(this->pub);

  return result;
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
  unsigned char *key;

  memcpy((char *)&this->pub, data, length);

  // Try to pull the key from the cache
  key = CALL(key_cache, borrow, (void *)&this->pub.iv,
             sizeof(this->pub.iv));
  if(key) {
    memcpy(this->key, key, sizeof(this->key));
  } else {
    if(!CALL(this, fetch_password_cb, subject))
      goto error;

    key = CALL(key_cache, borrow, (void *)&this->pub.iv,
               sizeof(this->pub.iv));
  };

  AES_set_encrypt_key(this->key, sizeof(this->key) * 8, &this->ekey);
  AES_set_decrypt_key(this->key, sizeof(this->key) * 8, &this->dkey);

  return 1;
 error:
  return 0;
};

static int AES256Password_parse(RDFValue self, char *serialised,
                                RDFURN subject) {
  AES256Password this = (AES256Password)self;
  decode64((unsigned char *)ZSTRING_NO_NULL(serialised), (unsigned char *)&this->pub, sizeof(this->pub));

  return 1;
};

int AES256Password_fetch_password_cb(AES256Password self, RDFURN subject) {
  // Try to use the password from the environment to decrypt it
  char *password = getenv(AFF4_VOLATILE_PASSPHRASE);
  unsigned char buff[sizeof(self->pub.nonce)];

  if(password) AES256Password_set(self, password);
  else {
    AFF4_LOG(AFF4_LOG_NONFATAL_ERROR, AFF4_SERVICE_ENCRYPTED_STREAM,
             subject, "No password set in environment variable "\
             AFF4_VOLATILE_PASSPHRASE);
    RaiseError(ERuntimeError, "No password set?");
    goto error;
  };

  // Now check the key by encrypting the IV with the key
  CALL((AFF4Cipher)self, encrypt, self->pub.iv,
       sizeof(self->pub.iv),
       buff, sizeof(self->pub.nonce), 0);

  if(memcmp(buff, self->pub.nonce, sizeof(self->pub.nonce))) {
    RaiseError(ERuntimeError, "Key is invalid");
    goto error;
  };

  return 1;
 error:
  return 0;
};

int AES256Password_encrypt(AFF4Cipher self, unsigned char *inbuff,
                           int inlen,
                           unsigned char *outbuf,
                           int length, int chunk_number) {
  AES256Password this = (AES256Password)self;
  unsigned char iv[sizeof(this->pub.iv)];

  memcpy(iv, this->pub.iv, sizeof(this->pub.iv));

  // The block IV is made by xoring the iv with the chunk number
  *(uint32_t *)iv ^= chunk_number;

  AES_cbc_encrypt(inbuff, outbuf, length, &this->ekey, iv, AES_ENCRYPT);

  return length;
};

int AES256Password_decrypt(AFF4Cipher self, unsigned char *inbuff,
                           int inlen,
                           unsigned char *outbuf,
                           int length, int chunk_number) {
  AES256Password this = (AES256Password)self;
  unsigned char iv[sizeof(this->pub.iv)];

  memcpy(iv, this->pub.iv, sizeof(this->pub.iv));

  // The block IV is made by xoring the iv with the chunk number
  *(uint32_t *)iv ^= chunk_number;

  AES_cbc_encrypt(inbuff, outbuf, length, &this->dkey, iv, AES_DECRYPT);

  return length;
};

VIRTUAL(AES256Password, AFF4Cipher) {
     VMETHOD_BASE(RDFValue, dataType) = AFF4_AES256_PASSWORD;

     VMETHOD_BASE(RDFValue, Con) = AES256Password_Con;
     VMETHOD_BASE(RDFValue, encode) = AES256Password_encode;
     VMETHOD_BASE(RDFValue, decode) = AES256Password_decode;
     VMETHOD_BASE(RDFValue, serialise) = AES256Password_serialise;
     VMETHOD_BASE(RDFValue, parse) = AES256Password_parse;

     VMETHOD_BASE(AFF4Cipher, blocksize) = AES_BLOCK_SIZE;
     VMETHOD_BASE(AFF4Cipher, encrypt) = AES256Password_encrypt;
     VMETHOD_BASE(AFF4Cipher, decrypt) = AES256Password_decrypt;

     VMETHOD_BASE(AES256Password, set) = AES256Password_set;
     VMETHOD_BASE(AES256Password, fetch_password_cb) = AES256Password_fetch_password_cb;
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
      RaiseError(ERuntimeError, "Encrypted stream has no cipher?");
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
    FileLikeObject backing_store = (FileLikeObject)CALL(oracle,
                                 open, this->backing_store, 'w');
    char buff[chunk_size];

    if(!backing_store) {
      RaiseError(ERuntimeError, "Unable to open backing store %s",
                 this->backing_store->value);
      goto error;
    };

    // Now take chunks off the buffer and write them to the backing
    // stream:
    for(offset = 0; this->block_buffer->size - offset >= chunk_size;
        offset += chunk_size) {
      
      CALL(this->cipher, encrypt, (unsigned char *)this->block_buffer->data + offset,
           chunk_size,
           (unsigned char *)buff, chunk_size, chunk_id);
      chunk_id ++;

      CALL(backing_store, write, buff, chunk_size);
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
  uint64_t chunk_id = self->readptr / chunk_size;
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

  // Now decrypt the buffer
  CALL(this->cipher, decrypt, cbuff, chunk_size,
       dbuff, chunk_size, chunk_id);

  // Return the available data
  memcpy(buff, dbuff + chunk_offset, available_to_read);

  self->readptr += available_to_read;

  return available_to_read;

 error:
  return -1;
};

static int Encrypted_read(FileLikeObject self, char *buff, unsigned long int length) {
  int read_length;
  int len = 0;
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

static void Encrypted_close(FileLikeObject self) {
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

  SUPER(FileLikeObject, FileLikeObject, close);
};


VIRTUAL(Encrypted, FileLikeObject) {
  VMETHOD_BASE(AFFObject, Con) = Encrypted_Con;
  VMETHOD_BASE(AFFObject, dataType) = AFF4_ENCRYTED;

  VMETHOD_BASE(FileLikeObject, write) = Encrypted_write;
  VMETHOD_BASE(FileLikeObject, read) = Encrypted_read;
  VMETHOD_BASE(FileLikeObject, close) = Encrypted_close;
} END_VIRTUAL;

void encrypt_init() {
  // Initialise the SSL library must be done once:
  SSL_load_error_strings();
  SSL_library_init();

  INIT_CLASS(AFF4Cipher);
  INIT_CLASS(AES256Password);
  INIT_CLASS(Encrypted);

  register_type_dispatcher(AFF4_ENCRYTED, (AFFObject *)GETCLASS(Encrypted));
  register_rdf_value_class((RDFValue)GETCLASS(AES256Password));

  // The key cache is a local cache mapping keys to IVs
  key_cache = CONSTRUCT(Cache, Cache, Con, NULL, 100, 0);
};
