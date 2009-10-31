#include <openssl/ssl.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include "zip.h"

/** Saves the key in the resolver, using all the methods which are
    available:

    Passphrase is a secret password to encrypt the key with.
    The following steps are taken:

    1) A random number between 2^16 and 2^24 is used as round number
       for passphrase fortification. This parameter is saved in the
       resolver as AFF4_CRYPTO_FORTIFICATION_COUNT.

    2) The password is hashed this many times using SHA256. This
       produces a new 256 bit key.

    3) The stream key is then encrypted using the above key
       with AES, and the IV.

    4) We save the IV in the resolver using AFF4_CRYPTO_IV
*/
static int prepare_passphrase(Encrypted self, OUT unsigned char *key, int key_length) {
  char *passphrase = (char *)CALL(oracle, resolve, self, CONFIGURATION_NS, 
				  AFF4_VOLATILE_PASSPHRASE,
				  RESOLVER_DATA_STRING);
  
  if(passphrase) {
    int passphrase_len = strlen(passphrase);
    uint64_t fortification_count = *(uint64_t *)CALL(oracle, resolve, self, URNOF(self),
						     AFF4_CRYPTO_FORTIFICATION_COUNT,
						     RESOLVER_DATA_UINT64);
    EVP_MD_CTX digest;
    unsigned char buffer[BUFF_SIZE];
    unsigned int buffer_length=BUFF_SIZE;
    int i;

    EVP_DigestInit(&digest, EVP_sha256());

    if(fortification_count == 0) {
      // Some random data for the fortification_count
      RAND_pseudo_bytes((unsigned char *)&fortification_count, 
			sizeof(fortification_count));
      fortification_count &= 0xFFFF;
      fortification_count <<= 8;

      // Store it
      CALL(oracle, set, URNOF(self), AFF4_CRYPTO_FORTIFICATION_COUNT, 
	   &fortification_count, RESOLVER_DATA_UINT64);
    };

    /* Fortify the passphrase (This is basically done in order to
       increase the cost of checking the passphrase by a huge factor
       making dictionary attacks very ineffective). 
    */
    for(i=0; i<fortification_count; i++) {
      EVP_DigestUpdate(&digest, passphrase, passphrase_len);
    };

    memset(buffer, 0, buffer_length);
    EVP_DigestFinal(&digest, buffer, &buffer_length); 

    // Return the key material
    memcpy(key, buffer, key_length);

    return 1;
  };

  return 0;
};

static int save_AES_key(Encrypted self, unsigned char affkey[32]) {
  // Is there a passphrase specified?
  int saved = 0;
  unsigned char key[32];
  unsigned char iv[AES_BLOCK_SIZE];
  unsigned char encrypted_key[32];
  unsigned char buffer[BUFF_SIZE];
  AES_KEY aes_key;
  
  // This places an encryption key in key derived from the passphrase:
  if(prepare_passphrase(self, key, sizeof(key))) {
    TDB_DATA iv_data, encrypted_key_data;

    // Make a new iv
    RAND_pseudo_bytes(iv, sizeof(iv));
    
    iv_data.dptr = iv;
    iv_data.dsize = sizeof(iv);
    
    // And we store them in the resolver
    CALL(oracle, set, URNOF(self), AFF4_CRYPTO_IV, &iv_data, RESOLVER_DATA_TDB_DATA);
    
    // The passphrase key is now used to encrypt the stream key
    AES_set_encrypt_key(key, 256, &aes_key);
    AES_cbc_encrypt(affkey, encrypted_key, 32, &aes_key, iv, AES_ENCRYPT);
    
    encode64((unsigned char *)encrypted_key, 32, buffer, sizeof(buffer));
    encrypted_key_data.dptr = encrypted_key;
    encrypted_key_data.dsize = 32;

    CALL(oracle, set, URNOF(self), AFF4_CRYPTO_PASSPHRASE_KEY, 
	 &encrypted_key_data, RESOLVER_DATA_TDB_DATA);

    CALL(oracle, set, URNOF(self), AFF4_CRYPTO_ALGORITHM, 
	 AFF4_CRYPTO_ALGORITHM_AES_SHA254, RESOLVER_DATA_STRING);
    
    saved = 1;
  };

  return saved;
};

/*
  Make a new random key. Note that we use 256 bits (32 bytes)
  of randomness, but for convenience we base64 encode it so
  it can be stored in the resolver easily.

  Note that we store the key in the resolver for other instances to
  use.
*/
static unsigned char *generate_AES_key(Encrypted self) {
  char buff[BUFF_SIZE];  
  unsigned char *affkey = talloc_size(self, 32);
  TDB_DATA affkey_data;

  int r = RAND_bytes(affkey,32); // makes a random key; with REAL random bytes
  if(r!=1) r = RAND_pseudo_bytes(affkey,32); // true random not supported
  if(r!=1) {
    RaiseError(ERuntimeError, "Cant make random data");
    return NULL;
  };
  
  encode64((unsigned char *)affkey, 32, (unsigned char*)buff, sizeof(buff));
  // Set the key
  affkey_data.dptr = affkey;
  affkey_data.dsize = 32;

  CALL(oracle, set, URNOF(self), AFF4_VOLATILE_KEY, &affkey_data,
       RESOLVER_DATA_TDB_DATA);

  // Now we have a key - we need to initialise our AES_key:
  AES_set_encrypt_key(affkey, 256, &self->ekey);
  AES_set_decrypt_key(affkey, 256, &self->dkey);

  return affkey;
};

/* Go through the various options for loading the key from various
   sources. This is basically the mirror image of save_AES_key.
*/
static int load_AES_key(Encrypted this) {
  unsigned char affkey[32];
  // This is an encoded representation of the key (so it in suitable
  // to go in the resolver).
  TDB_DATA *key;

  // Is the key in the resolver already?
  key = (TDB_DATA *)CALL(oracle, resolve, this, URNOF(this), AFF4_VOLATILE_KEY,
			 RESOLVER_DATA_TDB_DATA);

  // Maybe its a passphrase
  if(!key) {
    unsigned char passphrase_key[32];
    unsigned char iv[AES_BLOCK_SIZE];
    unsigned char buffer[BUFF_SIZE];
    
    if(prepare_passphrase(this, passphrase_key, sizeof(passphrase_key))) {
      TDB_DATA *encoded_iv = (TDB_DATA *)CALL(oracle, resolve, 
					      this, URNOF(this), 
					      AFF4_CRYPTO_IV,
					      RESOLVER_DATA_TDB_DATA);

      if(encoded_iv) {
	TDB_DATA *encoded_key = (TDB_DATA *)CALL(oracle, 
						 resolve, this,  
						 URNOF(this), 
						 AFF4_CRYPTO_PASSPHRASE_KEY,
						 RESOLVER_DATA_TDB_DATA);
	AES_KEY aes_key;
      
	// The passphrase key is now used to decrypt the stream key
	AES_set_decrypt_key(passphrase_key, 256, &aes_key);

	// Now this should be the actual key
	AES_cbc_encrypt(affkey, affkey, 32, &aes_key, iv, AES_DECRYPT);

	// FIXME:
	// We put it in the volatile namespace:
	encode64((unsigned char*)affkey, 32, buffer, sizeof(buffer));
	//CALL(oracle, set, URNOF(this), AFF4_VOLATILE_KEY,  *)buffer);

	key = (char *)buffer;
      };
    };
  };

  if(key) {
    // Now we have a key - we need to initialise our AES_key:
    AES_set_encrypt_key(affkey, 256, &this->ekey);
    AES_set_decrypt_key(affkey, 256, &this->dkey);
    
    return 1;
  };
  
  return 0;
};

static AFFObject Encrypted_AFFObject_Con(AFFObject self, char *urn, char mode) {
  Encrypted this = (Encrypted)self;
  FileLikeObject fd;
  char *value;

  if(urn) {
    URNOF(self) = talloc_strdup(self, urn);
    
    this->block_size = *(uint64_t *)resolver_get_with_default
      (oracle, self->urn, AFF4_BLOCKSIZE, "4k",
       RESOLVER_DATA_UINT64);

    this->target_urn = (char *)CALL(oracle, resolve, self,  URNOF(self), 
				    AFF4_TARGET, RESOLVER_DATA_URN);
    if(!this->target_urn) {
      RaiseError(ERuntimeError, "No target stream specified");
      goto error;
    };

    fd = (FileLikeObject)CALL(oracle, open, this->target_urn, mode);
    if(!fd) {
      RaiseError(ERuntimeError, "Unable to open target %s", this->target_urn);
      goto error;
    };
    CALL(oracle, cache_return, (AFFObject)fd);

    // Set our size 
    ((FileLikeObject)self)->size = *(uint64_t *)resolver_get_with_default
      (oracle, self->urn, NAMESPACE "size", "0", RESOLVER_DATA_UINT64);

    this->volume = (char *)CALL(oracle, resolve, self, URNOF(self), AFF4_STORED,
				RESOLVER_DATA_URN);
    if(!this->volume) {
      RaiseError(ERuntimeError, "No idea where im stored?");
      goto error;
    };

    CALL(self, set_property, AFF4_TYPE, AFF4_ENCRYTED);
    CALL(self, set_property, AFF4_INTERFACE, AFF4_STREAM);
    this->block_buffer = CONSTRUCT(StringIO, StringIO, Con, self);

    // If we want to read the stream, we really must be able to grab
    // the key from somewhere
    if(mode == 'r') {
      if(!load_AES_key(this)) {
	RaiseError(ERuntimeError, "Unable to decrypt encrypted stream %s", URNOF(self));
	goto error;
      };
      // But if we create a new stream we need to create a new key
    } else if(mode=='w') {
      // Nope - we dont have a key yet - make a new one:
      unsigned char *affkey=generate_AES_key(this);

      /* No point proceeding if we can not save the encryption key (no
	 one will be able to load this stream later). At least one
	 supported method needs to be available here.
      */
      if(!save_AES_key(this, affkey)) {
	RaiseError(ERuntimeError, "Unable to save Encryption Key");
	goto error;
      };
      if(!load_AES_key(this)) {
	RaiseError(ERuntimeError, "Unable to generate new keys for %s", URNOF(self));
	goto error;	
      };
    };
  } else {
    this->__super__->super.Con(self, urn, mode);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};

static AFFObject Encrypted_finish(AFFObject self) {
  return self->Con(self, URNOF(self), 'w');
};

// Read as much as possible from this block after decrypting it
static int partial_read(FileLikeObject self, char *buffer, int length, 
			FileLikeObject target) {
  Encrypted this = (Encrypted)self;
  int block_number = self->readptr / this->block_size;
  int block_offset = self->readptr % this->block_size;
  unsigned char data[this->block_size];
  int datalen = this->block_size;
  unsigned char iv[AES_BLOCK_SIZE];
  int available = min(length, this->block_size - block_offset);
  
  memset(iv, 0, sizeof(iv));
  
  // The IV is derived from the block number
  snprintf((char *)iv, AES_BLOCK_SIZE, "%u", block_number);

  CALL(target, seek, block_number * this->block_size, SEEK_SET);
  length = CALL(target, read, (char *)data, this->block_size);
  
  // Now decrypt into the buffer
  AES_cbc_encrypt(data,data, datalen, &this->dkey, iv, AES_DECRYPT);

  // And copy to the output
  memcpy(buffer, data + block_offset, available);

  return available;
};

static int Encrypted_read(FileLikeObject self, char *buffer, unsigned long int length) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, this->target_urn, 'r');
  int result=0;
  int total_read=0;

  // How much is available to read here?
  length = min(length, ((int64_t)self->size) - self->readptr);

  while(length > 0) {
    result = partial_read(self, buffer, length, target);
    if(result <=0) break;
    self->readptr += result;
    buffer += result;
    length -= result;
    total_read += result;
  };

  CALL(oracle, cache_return, (AFFObject)target);

  return total_read;
};

static int write_encrypted_block(Encrypted this, char *data,
				 FileLikeObject target) {
    unsigned char iv[AES_BLOCK_SIZE];
    unsigned char new_data[this->block_size];
    int block_number = target->readptr / this->block_size;

    memset(iv, 0, sizeof(iv));

    // The IV is derived from the block number
    snprintf((char *)iv, AES_BLOCK_SIZE, "%u", block_number);
    // FIXME - resolver needs to keep track here
    this->block_number++;

    // Encrypt the block:
    AES_cbc_encrypt((const unsigned char *)data,
		    new_data,this->block_size,
		    &this->ekey, iv, AES_ENCRYPT);

    return CALL(target, write, (char *)new_data, this->block_size);
};

/** We assume that writing is never random */
static int Encrypted_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target=NULL;
  int result;
  int dumped_offset=0;

  // Write the data to the end of our buffer:
  CALL(this->block_buffer, seek, 0, SEEK_END);
  result = CALL(this->block_buffer, write, buffer, length);
  self->readptr += result;
  self->size = max(self->size, self->readptr);

  target = (FileLikeObject)CALL(oracle, open, this->target_urn, 'w');

  // If the buffer is too large, we see if we can flush any blocks. We
  // assume that block_size is a whole multiple of AES_BLOCK_SIZE
  while(this->block_buffer->size - dumped_offset > this->block_size) {
    write_encrypted_block(this, this->block_buffer->data + dumped_offset, target);
    dumped_offset += this->block_size;
  };

  CALL(this->block_buffer, skip, dumped_offset);
  CALL(oracle, cache_return, (AFFObject)target);

  return result;
};

static void Encrypted_close(FileLikeObject self) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, this->target_urn, 'w');
  char buffer[this->block_size];
  int pad = max(this->block_size - this->block_buffer->size, 0);

  memset(buffer, 0, this->block_size);
  // Pad
  CALL(this->block_buffer, seek, 0, SEEK_END);
  CALL(this->block_buffer, write, buffer, pad);
   
  // Flush the last block
  write_encrypted_block(this, this->block_buffer->data, target);

  // Now make a new properties file for us:
  dump_stream_properties(self);

  // Now close the target - we are done here
  CALL(target, close);
  talloc_free(self);
};

VIRTUAL(Encrypted, FileLikeObject)
     VMETHOD_BASE(AFFObject, Con) = Encrypted_AFFObject_Con;
     VMETHOD_BASE(AFFObject, finish) = Encrypted_finish;
 
     VMETHOD_BASE(FileLikeObject, read) = Encrypted_read;
     VMETHOD_BASE(FileLikeObject, write) = Encrypted_write;
     VMETHOD_BASE(FileLikeObject, close) = Encrypted_close;

// Initialise the SSL library must be done once:
     SSL_load_error_strings();
     SSL_library_init();

END_VIRTUAL
