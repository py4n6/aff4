/** This file implemnts the resolver */
#include "fif.h"
#include <uuid/uuid.h>

/** This is a dispatcher of stream classes depending on their name.
*/
struct dispatch_t {
  char *type;
  AFFObject class_ptr;
};

static struct dispatch_t dispatch[] = {
  { "blob", (AFFObject)&__Blob },
  { "volume", (AFFObject)&__FIFFile },
  { "file://", (AFFObject)&__FileBackedObject },
  { NULL, NULL}
};

/** We need to call these initialisers explicitely or the class
    references wont work.
*/
static void init_streams(void) {
  FileLikeObject_init();
  AFFFD_init();
  Image_init();
  MapDriver_init();
  Blob_init();
};

static Resolver Resolver_Con(Resolver self) {
  init_streams();
  return self;
};

char *Resolver_resolve(Resolver self, char *urn, char *attribute) {
  Cache i=CALL(self->urn, get, urn);
  Cache j;
  char *result;

  if(!i || !i->data) return NULL;
  i=(Cache)i->data;

  j = CALL(i, get, attribute);
  if(!j) return NULL;

  return (char *)j->data;
};

AFFObject Resolver_open(Resolver self, char *urn, char *class_name) {
  int i;
  char *stream_type;
  char buff[BUFF_SIZE];
  struct dispatch_t *dispatch_ptr=NULL;
  AFFObject result;

  // OK Maybe the type is encoded into the URN:
  for(i=0; dispatch[i].type !=NULL; i++) {
    if(strlen(urn)>7 && !memcmp(urn, ZSTRING_NO_NULL(dispatch[i].type))) {
      dispatch_ptr = &dispatch[i];
      break;
    };
  };

  // Nope - maybe its stated explicitely
  if(!dispatch_ptr) {
    stream_type = CALL(self, resolve, urn, "aff2:type");
    if(stream_type) {
      // Find it in the dispatcher struct and instantiate it
      for(i=0; dispatch[i].type !=NULL; i++) {
	if(!strcmp(dispatch[i].type, stream_type)) {
	  dispatch_ptr = &dispatch[i];
	  break;
	};
      };
    };
  };

  // Gee no idea what this is
  if(!dispatch_ptr) {
    if(stream_type) {
      RaiseError(ERuntimeError, "Unable to open %s: This implementation can not open objects of type %s?", urn, stream_type);
    } else {
      RaiseError(ERuntimeError, "Unable to open %s - protocol not supported?", urn);
    };
    return NULL;
  };
  
  // A special constructor from a class reference
  result = CONSTRUCT_FROM_REFERENCE(dispatch[i].class_ptr, 
				    Con, self, urn);
  
  return result; 
};

/** uri, attribute and value must be talloced instances which will be
    stolen 
*/
void Resolver_add(Resolver self, char *uri, char *attribute, char *value) {
  Cache tmp, tmp2;
  printf("Adding to resolver: %s %s=%s\n", uri, attribute, (char *)value);
  fflush(stdout);
  
  tmp = CALL(self->urn, get, uri);
  if(!tmp || !tmp->data) {
    // Create a new URI
    tmp = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 0);
    talloc_set_name(tmp, "Node %s", uri);
    CALL(self->urn, put, uri, tmp, sizeof(*tmp));
  } else {
    // Dont need that
    talloc_free(uri);
    tmp = (Cache)tmp->data;
  };
  
  CALL(tmp, put, attribute, ZSTRING_NO_NULL(value));
};

/** Format all the attributes of the object specified by urn */
char *Resolver_export(Resolver self, char *urn) {
  char *result=talloc_strdup(NULL, "");
  Cache i = CALL(self->urn, get, urn);
  Cache j;

  if(!i) return result;

  // The data returned is really a second dictionary:
  i = (Cache)i->data;

  list_for_each_entry(j, &i->cache_list, cache_list) {
    result = talloc_asprintf_append(result, "%s %s=%s\n", urn, 
				    (char *)j->key, (char *)j->data);
  };

  return result;
};


/** Here we implement the resolver */
VIRTUAL(Resolver, Object)
     VMETHOD(Con) = Resolver_Con;

/* The cache is a class attribute so all instances use a singleton
 cache. We never expire this cache. The cache contains global
 attributes in urn notation. Its basically a dictionary with urn
 attributes as key and the attribute values as values. URNs may be
 resolved via an external service as well. Currently supported is a
 HTTP resolver which can be set by the AFF2_HTTP_RESOLVER environment
 variable.
*/
     __Resolver.urn = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 1e6);
     VMETHOD(resolve) = Resolver_resolve;
     VMETHOD(add)  = Resolver_add;
     VMETHOD(export) = Resolver_export;
     VMETHOD(open) = Resolver_open;
END_VIRTUAL

/************************************************************
  AFFObject - This is the base class for all other objects
************************************************************/
AFFObject AFFObject_Con(AFFObject self, char *uri) {
  uuid_t uuid;
  char *uuid_str;
  
  if(!uri) {
    // This function creates a new stream from scratch so the stream
    // name will be a new UUID
    uuid_generate(uuid);
    uuid_str = talloc_size(self, 40);
    uuid_unparse(uuid, uuid_str);

    uri = talloc_asprintf(self, "urn:aff2:%s", uuid_str);
  };
  self->uri = uri;

  return self;
};

VIRTUAL(AFFObject, Object)
     VMETHOD(Con) = AFFObject_Con;
END_VIRTUAL
