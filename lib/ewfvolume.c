/** This file implements support for ewf volumes */
#include "aff4.h"

// All access to libewf must be protected by mutex since libewf is
// not threadsafe
pthread_mutex_t LIBEWF_LOCK;

// Global error handler for libewf
static __thread libewf_error_t *ewf_error;

#if 0
#define LOCK_EWF pthread_mutex_lock(&LIBEWF_LOCK)
#define UNLOCK_EWF pthread_mutex_unlock(&LIBEWF_LOCK)
#else
#define LOCK_EWF
#define UNLOCK_EWF
#endif

static int EWFVolume_destructor(void *this) {
  EWFVolume self = (EWFVolume)this;

  if(self->handle) {
    LOCK_EWF;
    libewf_handle_close(&self->handle, &ewf_error);
    UNLOCK_EWF;
  };

  return 0;
};

static AFFObject EWFVolume_AFFObject_Con(AFFObject self, RDFURN urn, char mode) {
  EWFVolume this = (EWFVolume)self;

  self = SUPER(AFFObject, AFF4Volume, Con, urn, mode);

  if(mode == 'w') {
    RaiseError(ERuntimeError, "EWF files can only be opened for reading.");
    goto error;
  };

  if(self && urn) {
    this->stored = new_RDFURN(self);
    if(!CALL(oracle, resolve_value, urn, AFF4_STORED, (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "EWF volume is not stored anywhere?");
      goto error;
    };

    if(!CALL((AFF4Volume)this, load_from, this->stored, 'r')) {
      goto error;
    };
  };

  talloc_set_destructor((void *)self, EWFVolume_destructor);
  return self;

 error:
  talloc_free(self);
  return NULL;
};

int EWFVolume_load_from(AFF4Volume self, RDFURN urn, char mode) {
  char **filenames=NULL;
  int amount_of_filenames=0;
  EWFVolume this = (EWFVolume)self;
  RDFURN stream;
  uint64_t media_size;
  XSDInteger size = new_XSDInteger(self);
  XSDString string = new_XSDString(size);

  if(strlen(urn->parser->scheme) > 0 &&
     strcmp(urn->parser->scheme, "file")) {
    RaiseError(ERuntimeError, "EWF volumes can only be "
               "opened from file URIs, not '%s'", urn->value);
    goto error;
  };

  LOCK_EWF;
  if(libewf_glob(ZSTRING_NO_NULL(urn->parser->query),
                 LIBEWF_FORMAT_UNKNOWN,
                 &filenames,
                 &amount_of_filenames, &ewf_error) < 1) {
    goto libewf_error;
  };

  if(libewf_handle_open(&this->handle, filenames,
                        amount_of_filenames, LIBEWF_OPEN_READ,
                        &ewf_error)==0) {
    goto libewf_error;
  };
  UNLOCK_EWF;

  if(!this->handle) {
    RaiseError(ERuntimeError, "Unable to open EWF volume %s", urn->value);
    goto error;
  };

  LOCK_EWF;
  if(-1==libewf_handle_get_media_size(&this->handle, &media_size, &ewf_error)) {
    goto libewf_error;
  };
  UNLOCK_EWF;

  // Only seems to exist in experimental API
  //  libewf_glob_free(filenames, amount_of_filenames

  CALL(size, set, media_size);

  // Make a new unique URI for the EWF volume based on the URI of its file
  {
    char *buff = talloc_strdup(size, urn->value);
    int i = strlen(buff);

    for(;i>0;i--) {
      if(buff[i]=='.') {
        buff[i] = 0;
        break;
      };
    };

    if(i==0) {
      RaiseError(ERuntimeError, "EWF filename should have an extension");
      goto error;
    };

    CALL(URNOF(self), set, buff);
    // Change the scheme to ewf
    URNOF(self)->parser->scheme = "ewf";
    URNOF(self)->value = CALL(URNOF(self)->parser, string, URNOF(self));
  };

  // We successfully opened it, now populate the resolver with some
  // information about it
  stream = new_RDFURN(self);

  CALL(stream, set, STRING_URNOF(self));
  CALL(stream, add, "stream");

  CALL(string, set, ZSTRING_NO_NULL(AFF4_EWF_STREAM));

  // Set the file we are stored in
  CALL(oracle, set_value, urn, AFF4_VOLATILE_CONTAINS, (RDFValue)URNOF(self),0);
  CALL(oracle, set_value, URNOF(self), AFF4_STORED, (RDFValue)urn,0);

  // To support the EWF file we add a single stream URI to the
  // resolver to emulate a full AFF4 volume
  CALL(oracle, set_value, URNOF(self), AFF4_VOLATILE_CONTAINS, (RDFValue)stream,0);
  CALL(oracle, set_value, stream, AFF4_STORED, (RDFValue)URNOF(self),0);
  CALL(oracle, set_value, stream, AFF4_TYPE, (RDFValue)string,0);
  CALL(oracle, set_value, stream, AFF4_SIZE, (RDFValue)size,0);

  // Set our own type so we can be reopened
  CALL(string, set, ZSTRING_NO_NULL(AFF4_EWF_VOLUME));
  CALL(oracle, set_value, URNOF(self), AFF4_TYPE, (RDFValue)string,0);

  talloc_free(size);
  return 1;

 libewf_error:
  RaiseError(ERuntimeError, "libewf error: %s", ewf_error);

 error:
  // We were unable to load this volume - invalidate anything we know
  // about it (remove streams etc).
  ((AFFObject)self)->delete(URNOF(self));

  talloc_free(size);
  return 0;
};

static int EWFVolume_close(AFFObject self) {
  talloc_free(self);

  return 1;
};

VIRTUAL(EWFVolume, AFF4Volume) {
  VMETHOD_BASE(AFFObject, dataType) = AFF4_EWF_VOLUME;
  VMETHOD_BASE(AFFObject, Con) = EWFVolume_AFFObject_Con;
  VMETHOD_BASE(AFF4Volume, load_from) = EWFVolume_load_from;
  VMETHOD_BASE(AFFObject, close) = EWFVolume_close;

  INIT_CLASS(FileLikeObject);
  VMETHOD_BASE(AFFObject, delete) = ((AFFObject)GETCLASS(FileLikeObject))->delete;

  UNIMPLEMENTED(AFF4Volume, writestr);
} END_VIRTUAL

static AFFObject EWFStream_AFFObject_Con(AFFObject self, RDFURN urn, char mode) {
  EWFStream this = (EWFStream)self;

  self = SUPER(AFFObject, FileLikeObject, Con, urn, mode);

  if(mode == 'w') {
    RaiseError(ERuntimeError, "EWF files can only be opened for reading.");
    goto error;
  };

  if(self && urn) {
    this->stored = new_RDFURN(self);
    CALL(URNOF(self), set, urn->value);

    if(!CALL(oracle, resolve_value, urn, AFF4_STORED, (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "EWF Stream %s has no AFF4_STORED attribute?", urn);
      goto error;
    };

    CALL(oracle, resolve_value, urn, AFF4_SIZE,
         (RDFValue)((FileLikeObject)self)->size);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};

static int EWFStream_read(FileLikeObject self, char *buffer, unsigned long int length) {
  EWFStream this = (EWFStream)self;
  EWFVolume volume = (EWFVolume)CALL(oracle, open, this->stored, 'r');
  int res;

  if(!volume) {
    goto error;
  };

  LOCK_EWF;
  res = libewf_handle_read_random(&volume->handle, buffer, length,
                                  self->readptr, &ewf_error);
  UNLOCK_EWF;
  if(res < 0) {
    RaiseError(ERuntimeError, "libewf read error");
    goto error;
  };

  self->readptr+=res;
  self->size->value = max(self->size->value, self->readptr);

  CALL((AFFObject)volume, cache_return);
  return res;

 error:
  if(volume)
    CALL((AFFObject)volume, cache_return);

  return -1;
};

static int EWFStream_close(AFFObject self) {
  talloc_free(self);

  return 0;
};

VIRTUAL(EWFStream, FileLikeObject) {
  VMETHOD_BASE(AFFObject, dataType) = AFF4_EWF_STREAM;

  VMETHOD_BASE(AFFObject, Con) = EWFStream_AFFObject_Con;
  VMETHOD_BASE(FileLikeObject, read) = EWFStream_read;
  VMETHOD_BASE(AFFObject, close) = EWFStream_close;
} END_VIRTUAL

void EWF_init() {
  INIT_CLASS(EWFVolume);
  INIT_CLASS(EWFStream);

  register_type_dispatcher(AFF4_EWF_VOLUME, (AFFObject *)GETCLASS(EWFVolume));
  register_type_dispatcher(AFF4_EWF_STREAM, (AFFObject *)GETCLASS(EWFStream));

  pthread_mutex_init(&LIBEWF_LOCK, NULL);
};
