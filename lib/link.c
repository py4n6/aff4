#include "zip.h"

static AFFObject Link_Con(AFFObject self, char *urn, char mode) {
  Link this = (Link)self;
  FileLikeObject result;

  if(urn) {
    char *target = CALL(oracle, resolve, urn, AFF4_TARGET);
    char *interface = CALL(oracle, resolve, target, AFF4_INTERFACE);

    if(interface)
      CALL(oracle, set, urn, AFF4_INTERFACE, interface);

    if(!target) {
      RaiseError(ERuntimeError, "%s unable to resolve the " AFF4_TARGET " property?", urn);
      goto error;
    };

    result = (FileLikeObject)CALL(oracle, open, target, mode);
    if(!result) goto error;

    return (AFFObject)result;
  } else {
    this->__super__->Con(self, urn, mode);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};

static AFFObject Link_finish(AFFObject self) {

  self->mode = 'w';
  return self;
};

// A convenience function to set up a link between a target urn to a
// friendly name.
static void Link_link(Link self, Resolver oracle, char *storage_urn,
	       char *target, char *friendly_name) {
  if(storage_urn) {
    ZipFile zipfile = (ZipFile)CALL(oracle, open, storage_urn, 'w');
    char tmp[BUFF_SIZE];
    FileLikeObject fd;
    char *properties;
    char *interface = CALL(oracle, resolve, target, AFF4_INTERFACE);
    char *size = CALL(oracle, resolve, target, AFF4_SIZE);

    if(!zipfile) {
      RaiseError(ERuntimeError, "Unable to get storage container %s", storage_urn);
      return;
    };

    friendly_name = fully_qualified_name(self, friendly_name, storage_urn);

    // Note the time when the link was made
    CALL(oracle, set, URNOF(self), AFF4_TIMESTAMP, from_int(time(NULL)));

    // Add a reverse connection
    CALL(oracle, set, friendly_name, AFF4_TARGET, target);
    CALL(oracle, set, friendly_name, AFF4_TYPE, AFF4_LINK);
    if(interface) {
      CALL(oracle, set, friendly_name, AFF4_INTERFACE, interface);
    };

    if(size) {
      CALL(oracle, set, friendly_name, AFF4_SIZE, size);
    };

    snprintf(tmp, BUFF_SIZE, "%s/properties", friendly_name);

    fd = CALL((ZipFile)zipfile, open_member, tmp, 'w', NULL, 0, ZIP_STORED);
    if(fd) {
      properties = CALL(oracle, export_urn, friendly_name, friendly_name);

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
