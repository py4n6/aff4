#include "openssl/aes.h"
#include "zip.h"

static AFFObject Encrypted_AFFObject_Con(AFFObject self, char *urn, char mode) {
  Encrypted this = (Encrypted)self;
  FileLikeObject fd;
  char *value;

  if(urn) {
    URNOF(self) = talloc_strdup(self, urn);

    value = resolver_get_with_default(oracle, self->urn, AFF4_BLOCKSIZE, "4k");
    this->block_size = parse_int(value);

    this->target_urn = CALL(oracle, resolve, URNOF(self), AFF4_TARGET);
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
    ((FileLikeObject)self)->size = parse_int(resolver_get_with_default(oracle, self->urn, NAMESPACE "size", "0"));

    this->volume = CALL(oracle, resolve, URNOF(self), AFF4_STORED);
    if(!this->volume) {
      RaiseError(ERuntimeError, "No idea where im stored?");
      goto error;
    };

    CALL(self, set_property, AFF4_TYPE, AFF4_ENCRYTED);
    CALL(self, set_property, AFF4_INTERFACE, AFF4_STREAM);
    this->block_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
    // FIXME
    this->key = "hello world";
    this->salt = "12345678";
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

static int Encrypted_read(FileLikeObject self, char *buffer, unsigned long int length) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, this->target_urn, 'r');
  int result;

  result = CALL(target, read, buffer, length);
  CALL(oracle, cache_return, (AFFObject)target);

  self->readptr += result;

  return result;
};

static int Encrypted_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, this->target_urn, 'w');
  int result;

  result = CALL(target, write, buffer, length);
  CALL(oracle, cache_return, (AFFObject)target);

  self->readptr += result;
  self->size = max(self->size, self->readptr);
  return result;
};

static void Encrypted_close(FileLikeObject self) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, this->target_urn, 'w');
  CALL(target, close);
  CALL(oracle, cache_return, (AFFObject)target);

  // Now make a new properties file for us:
  dump_stream_properties(self);
};

VIRTUAL(Encrypted, FileLikeObject)
     VMETHOD(super.super.Con) = Encrypted_AFFObject_Con;
     VMETHOD(super.super.finish) = Encrypted_finish;

     VMETHOD(super.read) = Encrypted_read;
     VMETHOD(super.write) = Encrypted_write;
     VMETHOD(super.close) = Encrypted_close;
END_VIRTUAL
