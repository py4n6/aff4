#include "openssl/aes.h"
#include "zip.h"

AFFObject Encrypted_AFFObject_Con(AFFObject self, char *urn) {
  Encrypted this = (Encrypted)self;
  char *value;

  if(urn) {
    URNOF(self) = talloc_strdup(self, urn);

    value = resolver_get_with_default(oracle, self->urn, "aff2:block_size", "4k");
    this->block_size = parse_int(value);

    this->target_urn = CALL(oracle, resolve, URNOF(self), "aff2:target");
    if(!this->target_urn) {
      RaiseError(ERuntimeError, "No target stream specified");
      goto error;
    };

    this->volume = CALL(oracle, resolve, URNOF(self), "aff2:stored");
    if(!this->volume) {
      RaiseError(ERuntimeError, "No idea where im stored?");
      goto error;
    };

    CALL(self, set_property, "aff2:type", "encrypted");
    this->block_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
    // FIXME
    this->key = "hello world";
    this->salt = "12345678";
  } else {
    this->__super__->super.Con(self, urn);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};

AFFObject Encrypted_finish(AFFObject self) {
  return self->Con(self, URNOF(self));
};

int Encrypted_read(FileLikeObject self, char *buffer, unsigned long int length) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, NULL, this->target_urn);
  int result;

  result = CALL(target, read, buffer, length);
  CALL(oracle, cache_return, (AFFObject)target);

  self->readptr += result;

  return result;
};

int Encrypted_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = (FileLikeObject)CALL(oracle, open, NULL, this->target_urn);
  int result;

  result = CALL(target, write, buffer, length);
  CALL(oracle, cache_return, (AFFObject)target);

  self->readptr += result;
  self->size = max(self->size, self->readptr);
  return result;
};

void Encrypted_close(FileLikeObject self) {
  Encrypted this = (Encrypted)self;
  FileLikeObject target = CALL(oracle, open, NULL, this->target_urn);
  CALL(target, close);
  CALL(oracle, cache_return, (AFFObject)target);

  // Now make a new properties file for us:
  dump_stream_properties(self, this->volume);
};

VIRTUAL(Encrypted, FileLikeObject)
     VMETHOD(super.super.Con) = Encrypted_AFFObject_Con;
     VMETHOD(super.super.finish) = Encrypted_finish;

     VMETHOD(super.read) = Encrypted_read;
     VMETHOD(super.write) = Encrypted_write;
     VMETHOD(super.close) = Encrypted_close;
     VATTR(super.super.type) = "Encrypted";
END_VIRTUAL
