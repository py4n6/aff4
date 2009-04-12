/** An identity is someone who makes statements about other AFFObjects in
    the universe.
    
    AFF4 implements signing by use of identities. The identity makes
    statements about the hashes of various other objects, and then
    signes a statement about it. If the identity is trusted, the
    statesments can be believed and compared with the true
    hashes. This allows us to detect modification or manipulation of
    evidence.
*/
#include "zip.h"

static void Identity_Resolver_add(Resolver self, char *uri, char *attribute, char *value) {
  /* we only care about hashes here */
  if(strstr(attribute, AFF4_SHA) == attribute) {
    // Call the proper resolver add method
    __Resolver.add(self, uri, attribute, value);
  };
};

static AFFObject Identity_Con(AFFObject self, char *uri, char mode) {
  Identity this = (Identity)self;

  if(uri) {
    // Find the resolver in the oracle if possible
    Resolver i;
    
    this->info = NULL;
    list_for_each_entry(i, &oracle->identities, identities) {
      if(!strcmp(URNOF(i), uri)) {
	this->info = i;
	break;
      };
    };

    self->urn = talloc_strdup(self, uri);
  } else {
    this->__super__->Con(self, uri, mode);
  };

  if(!this->info) {
    this->info = CONSTRUCT(Resolver, Resolver, Con, oracle);
    // We place a filtering hook for this identity.
    this->info->add = Identity_Resolver_add;

    URNOF(this->info) = talloc_strdup(this->info, self->urn);
    list_add_tail(&this->info->identities, &oracle->identities);
  };

  return self;
};

static void Identity_store(Identity self, char *volume_urn) {
  ZipFile volume = (ZipFile)CALL(oracle, open, NULL, volume_urn, 'w');
  char *text;
  char filename[BUFF_SIZE];
  int i=0;
  char *fqn = fully_qualified_name(URNOF(self), volume_urn);

  if(!volume) {
    RaiseError(ERuntimeError, "Unable to open volume %s", volume_urn);
    return;
  };

  // Try to find a signing segment
  while(1) {
    snprintf(filename, BUFF_SIZE, "%08d", i);
    if(!CALL(oracle, is_set, fqn, AFF4_STATEMENT, filename))
      break;
    i++;
  };

  CALL(oracle, add, fqn, AFF4_TYPE, AFF4_IDENTITY);
  CALL(oracle, add, fqn, AFF4_STATEMENT, filename);
  PrintError();

  snprintf(filename, BUFF_SIZE, "%s/%08d", URNOF(self), i);
  text = CALL(self->info, export_all);
  CALL(volume, writestr,
       filename, ZSTRING(text),
       NULL, 0, 
       ZIP_STORED);

  talloc_free(text);

  // Make a properties file
  fqn = fully_qualified_name(URNOF(self), volume_urn);
  text = CALL(oracle, export_urn, fqn);
  if(text) {
    snprintf(filename, BUFF_SIZE, "%s/properties", URNOF(self));
    CALL(volume, writestr, filename, ZSTRING_NO_NULL(text),
	 NULL, 0, ZIP_STORED);

    talloc_free(text);
  };
};

VIRTUAL(Identity, AFFObject)
     VMETHOD(super.Con) = Identity_Con;
     VMETHOD(store) = Identity_store;
END_VIRTUAL
