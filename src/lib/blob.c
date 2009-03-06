#include "zip.h"

/** This is the implementation for the blob AFFObject */
AFFObject Blob_Con(AFFObject self, char *urn) {
  Blob this = (Blob)self;

  /** If urn was provided it means we need to go get ourselves */
  if(urn) {
    FIFFile fiffile;
    char *url;

    // First step - we need to ask the oracle about where the blob is
    // actually stored:
    url = CALL(oracle, resolve, urn, "aff2:location");
    if(!url) {
      RaiseError(ERuntimeError, "Unable to resolve volume storing blob %s", urn);
      goto error;
    }

    // Now we ask the oracle to open the volume:
    fiffile = CALL(oracle, open, url);
    if(!fiffile) {
      goto error;
    };

    // Make our own private copy so we can keep it around in case the
    // zipfile cache is expired.
    this->data = fiffile->super.read_member((ZipFile)fiffile, urn, &this->length);
    this->data = talloc_memdup(self, this->data, this->length);
    
    self->urn = talloc_strdup(self, urn);

  } else {
    this->__super__->Con(self, urn);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};

VIRTUAL(Blob, AFFObject)
     VMETHOD(super.Con) = Blob_Con;
END_VIRTUAL
