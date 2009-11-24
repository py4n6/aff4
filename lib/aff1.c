#include "zip.h"
#include <fcntl.h>
#include <uuid/uuid.h>

AFFObject AFFObject_AFF1Volume_Con(AFFObject self, char *urn, char mode) {
  // Where are we stored?
  char *filename = (char *)CALL(oracle, resolve, self, urn, AFF4_STORED,
				RESOLVER_DATA_URN);

  if(filename) { 
    return (AFFObject)CALL((ZipFile)self, Con, filename, mode);
  };

  RaiseError(ERuntimeError, "No "AFF4_STORED" property for Volume URN %s", urn);
  talloc_free(self);
  return NULL;
};

// Attach a destructor to ensure AFFLIB handle is properly closed
int AFF1Volume_destructor(void *self) {
  AFF1Volume this = (AFF1Volume)self;

  if(this->handle) {
    af_close(this->handle);
    this->handle = NULL;
  };

  return 0;
};

/*** 
  This is a handler for legacy AFF1 volumes.

  strategy: We basically open the AFF1 volume with afflib. If this
  succeeds we use the GID from the AFF1 volume to populate the file
  storage in the resolver for this volume - the new AFF4 compliant
  volume name is derived from the GID.

  We then create a psuedo stream of type AFF4_LIBAFF_STREAM which is
  stored in this AFF4_LIBAFF_VOLUME. The Stream handler can then use
  this information to load the afflib handle directly for use.

  Note that for efficiency we maintain the AFFLIB handle in cache
  rather than reloading from the file each time.
**/
ZipFile AFF1Volume_Con(ZipFile self, char *filename, char mode) {
  AFF1Volume this = (AFF1Volume)self;
  unsigned char buffer[BUFF_SIZE];
  unsigned int len = BUFF_SIZE;

  if(mode == 'w') {
    RaiseError(ERuntimeError, "Unable to write AFF1 legacy volumes");
    goto error;
  };

  if(!startswith(filename, FILE_NS)) {
    RaiseError(ERuntimeError, "Legacy AFF1 support is only available for files (not %s)", filename);
    goto error;
  };

  this->handle = af_open(filename+strlen(FILE_NS), 0, O_RDONLY);
  if(!this->handle) {
    RaiseError(ERuntimeError, "Unable to open AFF1 legacy volume");
    goto error;
  };

  // What is our UUID?
  if(af_get_seg(this->handle, AF_IMAGE_GID, 0, buffer, &len) < 0 || 
     // We expect exactly 16 bytes UUID here:
     len != 16 ){
    // The segment has no UUID - we make a bodgy one - this is NOT good!!!
    CALL((AFFObject)self, Con, NULL, mode);
  } else {
    char tmp[BUFF_SIZE];
    uuid_unparse(buffer, tmp);
    URNOF(self) = talloc_asprintf(self, FQN ":%s", tmp);
  };

  // OK - now we fill the resolver with various facts
  CALL(oracle, set, URNOF(self), AFF4_STORED, filename, RESOLVER_DATA_URN);
  CALL(oracle, set, URNOF(self), AFF4_TYPE, AFF4_LIBAFF_VOLUME, RESOLVER_DATA_STRING);
  CALL(oracle, set, filename, AFF4_VOLATILE_CONTAINS, URNOF(self), RESOLVER_DATA_URN);

  { //This is information about the psuedo stream we represent
    char buffer[BUFF_SIZE];
    int64_t size = af_get_imagesize(this->handle);

    if(size < 0) {
      RaiseError(ERuntimeError, "Image has invalid size");
      goto error;
    };

    snprintf(buffer, BUFF_SIZE, "%s/default", URNOF(self));
    CALL(oracle, set, buffer, AFF4_STORED, URNOF(self), RESOLVER_DATA_URN);
    CALL(oracle, set, buffer, AFF4_TYPE, AFF4_LIBAFF_STREAM, RESOLVER_DATA_STRING);
    CALL(oracle, set, buffer, AFF4_SIZE, &size, RESOLVER_DATA_UINT64);
    CALL(oracle, add, URNOF(self), AFF4_VOLATILE_CONTAINS, buffer, RESOLVER_DATA_URN, 1);
  };

  ((AFFObject)self)->mode = mode;

  return self;
 error:
  talloc_free(self);
  return NULL;
};

VIRTUAL(AFF1Volume, ZipFile)
     VMETHOD_BASE(AFFObject, Con) = AFFObject_AFF1Volume_Con;
     VMETHOD_BASE(ZipFile, Con) = AFF1Volume_Con;
END_VIRTUAL

AFFObject AFFObject_AFF1Stream_Con(AFFObject self, char *urn, char mode) {
  AFF1Stream this = (AFF1Stream)self;
  AFF1Volume fd;
  
  if(!urn || mode=='w') {
    RaiseError(ERuntimeError, "This implementation does not allow "
	       "writing to legacy AFF1 volumes");
    goto error;
  };
  
  this->volume_urn = (char *)CALL(oracle, resolve, self, urn, AFF4_STORED,
				  RESOLVER_DATA_URN);
  if(!this->volume_urn) {
    RaiseError(ERuntimeError, "No "AFF4_STORED" property for URN %s", urn);
    goto error;
  };

  // Try to open the underlying volume to ensure we can do so (the
  // file may have disappeared for example).
  fd = (AFF1Volume)CALL(oracle, open, this->volume_urn, mode);
  if(!fd) goto error;
  CALL(oracle, cache_return, (AFFObject)fd);

  URNOF(self) = talloc_strdup(self, urn);
  self->mode = mode;
  
  // Find out our size
  CLASS_ATTR(self, FileLikeObject, size) = *(uint64_t *)CALL(oracle, resolve,self,  
							     URNOF(self), AFF4_SIZE,
							     RESOLVER_DATA_UINT64);
  
  return self;

 error:
  talloc_free(self);
  return NULL;
};

int AFF1Stream_read(FileLikeObject self, char *buffer, unsigned long int length) {
  AFF1Stream this = (AFF1Stream)self;
  AFF1Volume fd = (AFF1Volume)CALL(oracle, open, this->volume_urn, 'r');
  int result;

  // Cant open volume
  if(!fd) {
    RaiseError(ERuntimeError, "Cant open volume %s", this->volume_urn);
    return -1;
  };

  // Set the readptr at the appropriate place:
  af_seek(fd->handle, self->readptr, SEEK_SET);

  result= af_read(fd->handle, buffer, length); 
  CALL(oracle, cache_return, (AFFObject)fd);

  return result;
};

VIRTUAL(AFF1Stream, FileLikeObject)
     VMETHOD_BASE(AFFObject, Con) = AFFObject_AFF1Stream_Con;
     VMETHOD_BASE(FileLikeObject, read) = AFF1Stream_read;
END_VIRTUAL
