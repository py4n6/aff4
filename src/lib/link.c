#include "fif.h"
#include "zip.h"

AFFObject Link_Con(AFFObject self, char *urn) {
  Link this = (Link)self;

  if(urn) {
    char *target = CALL(oracle, resolve, urn, "aff2:target");
    if(!target) {
      RaiseError(ERuntimeError, "%s unable to resolve the aff2:link_to property?", urn);
      goto error;
    };

    return CALL(oracle, open, target);
  } else {
    this->__super__->Con(self, urn);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};

AFFObject Link_finish(AFFObject self) {
  return self;
};

// A convenience function to set up a link between a target urn to a
// friendly name.
void Link_link(Link self, Resolver oracle, char *storage_urn,
	       char *target, char *friendly_name) {
  AFFObject this = (AFFObject)self;
  if(storage_urn) {
    FIFFile fiffile = (FIFFile)CALL(oracle, open, storage_urn);
    char tmp[BUFF_SIZE];
    FileLikeObject fd;
    char *properties;

    if(!fiffile) {
      RaiseError(ERuntimeError, "Unable to get storage container %s", storage_urn);
      return;
    };

    // Add a reverse connection (The link urn is obviously not unique).
    CALL(oracle, add, friendly_name, "aff2:target", target);
    CALL(oracle, add, friendly_name, "aff2:type", "link");

    snprintf(tmp, BUFF_SIZE, "%s/properties", friendly_name);

    fd = CALL((ZipFile)fiffile, open_member, tmp, 'w', NULL, 0, ZIP_STORED);
    properties = CALL(oracle, export, friendly_name);
    CALL(fd, write, ZSTRING_NO_NULL(properties));
    talloc_free(properties);

    CALL(fd, close);
  };
};

VIRTUAL(Link, AFFObject)
     VMETHOD(super.Con) = Link_Con;
     VMETHOD(super.finish) = Link_finish;
     VMETHOD(link) = Link_link;
END_VIRTUAL