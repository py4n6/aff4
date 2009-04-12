#include "zip.h"

static AFFObject Link_Con(AFFObject self, char *urn, char mode) {
  Link this = (Link)self;
  FileLikeObject result;

  if(urn) {
    char *target = CALL(oracle, resolve, urn, "aff2:target");
    if(!target) {
      RaiseError(ERuntimeError, "%s unable to resolve the aff2:link_to property?", urn);
      goto error;
    };

    result = CALL(oracle, open, self, target, mode);
    if(!result) goto error;

    // Copy our urn to the result
    URNOF(result) = talloc_strdup(result, URNOF(self));

    return result;
  } else {
    this->__super__->Con(self, urn, mode);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};

static AFFObject Link_finish(AFFObject self) {
  return self;
};

// A convenience function to set up a link between a target urn to a
// friendly name.
static void Link_link(Link self, Resolver oracle, char *storage_urn,
	       char *target, char *friendly_name) {
  AFFObject this = (AFFObject)self;
  if(storage_urn) {
    ZipFile zipfile = (ZipFile)CALL(oracle, open, self, storage_urn, 'w');
    char tmp[BUFF_SIZE];
    FileLikeObject fd;
    char *properties;

    if(!zipfile) {
      RaiseError(ERuntimeError, "Unable to get storage container %s", storage_urn);
      return;
    };

    // Add a reverse connection (The link urn is obviously not unique).
    CALL(oracle, add, friendly_name, AFF4_TARGET, target);
    CALL(oracle, add, friendly_name, AFF4_TYPE, AFF4_LINK);

    snprintf(tmp, BUFF_SIZE, "%s/properties", friendly_name);

    fd = CALL((ZipFile)zipfile, open_member, tmp, 'w', NULL, 0, ZIP_STORED);
    if(fd) {
      properties = CALL(oracle, export_urn, friendly_name);
      CALL(fd, write, ZSTRING_NO_NULL(properties));
      talloc_free(properties);

      CALL(fd, close);
    };
    CALL(oracle, cache_return, (AFFObject)zipfile);
  };
};

VIRTUAL(Link, AFFObject)
     VMETHOD(super.Con) = Link_Con;
     VMETHOD(super.finish) = Link_finish;
     VMETHOD(link) = Link_link;
END_VIRTUAL
