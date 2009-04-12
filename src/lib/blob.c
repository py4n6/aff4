/** FIXME - This needs to be merged with ZipFileStream to be a real
    FileLikeObject 
*/
#include "zip.h"

/** This is the implementation for the blob AFFObject */
static AFFObject Blob_Con(AFFObject self, char *urn, char mode) {
  Blob this = (Blob)self;

  /** If urn was provided it means we need to go get ourselves */
  if(urn) {
    ZipFile zipfile;
    char *url;

    // First step - we need to ask the oracle about where the blob is
    // actually stored:
    url = CALL(oracle, resolve, urn, AFF4_STORED);
    if(!url) {
      RaiseError(ERuntimeError, "Unable to resolve volume storing blob %s", urn);
      goto error;
    }

    // Now we ask the oracle to open the volume (same mode as ourselves):
    zipfile = (ZipFile)CALL(oracle, open, self, url, mode);
    if(!zipfile) {
      goto error;
    };

    // Keep a copy of the data
    this->data = zipfile->read_member((ZipFile)zipfile, this, urn, &this->length);
    CALL(oracle, cache_return, (AFFObject)zipfile);

    self->urn = talloc_strdup(self, urn);
  } else {
    this->__super__->Con(self, urn, mode);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};

VIRTUAL(Blob, AFFObject)
     VMETHOD(super.Con) = Blob_Con;
END_VIRTUAL
