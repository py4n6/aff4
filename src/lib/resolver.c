/** This file implemnts the resolver */
#include "zip.h"
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
  { "link", (AFFObject)&__Link },
  { "image", (AFFObject)&__Image },
  { "file://", (AFFObject)&__FileBackedObject },
  { NULL, NULL}
};

/** This is the global oracle - it knows everything about everyone. */
Resolver oracle = NULL;

/** We need to call these initialisers explicitely or the class
    references wont work.
*/
void AFF2_Init(void) {
  FileLikeObject_init();
  FileBackedObject_init();
  FIFFile_init();
  Image_init();
  //  MapDriver_init();
  Blob_init();
  Resolver_init();
  Link_init();

  // Make a global oracle
  if(oracle) {
    printf("detroying the existing oracle\n");
    talloc_free(oracle);
  };

  oracle =CONSTRUCT(Resolver, Resolver, Con, NULL);
};

static Resolver Resolver_Con(Resolver self) {
  self->urn = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);

  // This is a cache for frequently used objects
  self->cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 50);
  return self;
};

char *Resolver_resolve(Resolver self, char *urn, char *attribute) {
  Cache i=CALL(self->urn, get_item, urn);

  if(!i) {
    RaiseError(ERuntimeError, "Unable to locate attribute %s for urn %s",
	       attribute, urn);
    return NULL;
  };

  return CALL(i, get_item, attribute);
};

AFFObject Resolver_open(Resolver self, char *urn) {
  int i;
  char *stream_type;
  struct dispatch_t *dispatch_ptr=NULL;
  AFFObject result;

  if(!urn) return NULL;

  // Is this object cached?
  result = (AFFObject)CALL(self->cache, get_item, urn);
  if(result) return result;

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
  
  // Cache it
  CALL(self->cache, put, talloc_strdup(NULL, urn),
       result, sizeof(*result));
  return result; 
};

void Resolver_add(Resolver self, char *uri, char *attribute, char *value) {
  Cache tmp;
  printf("Adding to resolver: %s %s=%s\n", uri, attribute, (char *)value);
  fflush(stdout);
  
  tmp = CALL(self->urn, get_item, uri);
  if(!tmp) {
    // Create a new URI
    tmp = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 0);
    CALL(self->urn, put, talloc_strdup(NULL, uri),
	 tmp, sizeof(*tmp));
  };
  
  // Make a copy for us to keep
  value = talloc_strdup(NULL, value);
  CALL(tmp, put, talloc_strdup(NULL, attribute), 
       ZSTRING_NO_NULL(value));
};

/** Format all the attributes of the object specified by urn */
char *Resolver_export(Resolver self, char *urn) {
  char *result=talloc_strdup(NULL, "");
  Cache i = CALL(self->urn, get_item, urn);
  Cache j;

  if(!i) return result;

  list_for_each_entry(j, &i->cache_list, cache_list) {
    result = talloc_asprintf_append(result, "%s %s=%s\n", urn, 
				    (char *)j->key, (char *)j->data);
  };

  return result;
};

AFFObject Resolver_create(Resolver self, AFFObject *class_reference) {
  AFFObject result;
  if(!class_reference) return NULL;

  result = CONSTRUCT_FROM_REFERENCE((*class_reference), Con, self, NULL);

  // Cache it
  CALL(self->cache, put, talloc_strdup(NULL, result->urn),
       result, sizeof(*result));

  return result;
};

void Resolver_del(Resolver self, char *uri, char *attribute) {
  Cache tmp,j;
  tmp = CALL(self->urn, get_item, uri);
  if(!tmp) return;

  while(1) {
    j = CALL(tmp, get, attribute);
    if(!j) break;
    talloc_free(j);
  };
};

void Resolver_set(Resolver self, char *uri, char *attribute, char *value) {
  CALL(self, del, uri, attribute);
  CALL(self, add, uri, attribute, value);
};

/** Here we implement the resolver */
VIRTUAL(Resolver, Object)
     VMETHOD(Con) = Resolver_Con;
     VMETHOD(create) = Resolver_create;

     VMETHOD(resolve) = Resolver_resolve;
     VMETHOD(add)  = Resolver_add;
     VMETHOD(export) = Resolver_export;
     VMETHOD(open) = Resolver_open;
     VMETHOD(set) = Resolver_set;
     VMETHOD(del) = Resolver_del;
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
  self->urn = uri;

  return self;
};


void AFFObject_set_property(AFFObject self, char *attribute, char *value) {
  CALL(oracle, add,
       self->urn,
       attribute, value);
};

// Prepares an object to be used
AFFObject AFFObject_finish(AFFObject self) {
  return self;
};

VIRTUAL(AFFObject, Object)
     VMETHOD(finish) = AFFObject_finish;
     VMETHOD(set_property) = AFFObject_set_property;

     VMETHOD(Con) = AFFObject_Con;
END_VIRTUAL

/** Some useful helper functions */
char *resolver_get_with_default(Resolver self, char *urn, char *attribute, char *default_value) {
  char *result = CALL(self, resolve, urn, attribute);

  if(!result) {
    CALL(self, add, urn, attribute, default_value);
    result = default_value;
  };
  return result;
};
