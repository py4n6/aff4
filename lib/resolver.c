/** This file implemnts the resolver */
#include "zip.h"
#include <uuid/uuid.h>

/** This is a dispatcher of stream classes depending on their name.
*/
struct dispatch_t {
  // A boolean to determine if this is a scheme or a type
  int scheme;
  char *type;
  AFFObject class_ptr;
};

static struct dispatch_t dispatch[] = {
  // Must be first - fallback position
  { 1, "file://",                 (AFFObject)&__FileBackedObject },

  { 0, AFF4_SEGMENT,              (AFFObject)&__ZipFileStream },
  { 0, AFF4_ZIP_VOLUME,           (AFFObject)&__ZipFile },
  { 0, AFF4_DIRECTORY_VOLUME,     (AFFObject)&__DirVolume },
  { 0, AFF4_LINK,                 (AFFObject)&__Link },
  { 0, AFF4_IMAGE,                (AFFObject)&__Image },
  { 0, AFF4_MAP,                  (AFFObject)&__MapDriver},
  { 0, AFF4_ENCRYTED,             (AFFObject)&__Encrypted},
  { 0, AFF4_IDENTITY,             (AFFObject)&__Identity},

  // All handled by libcurl
#ifdef HAVE_LIBCURL
  { 1, "http://",                 (AFFObject)&__HTTPObject },
  { 1, "https://",                (AFFObject)&__HTTPObject },
  { 1, "ftp://",                  (AFFObject)&__HTTPObject },
#endif
  { 0, NULL, NULL}
};

/** This is the global oracle - it knows everything about everyone. */
Resolver oracle = NULL;

/** We need to call these initialisers explicitely or the class
    references wont work.
*/
/** Our own special add routine */
static void oracle_add(Resolver self, char *uri, char *attribute, char *value) {
  Resolver i;

  // Call the real add method for the oracle itself
  __Resolver.add(self, uri, attribute, value);

  // Add these to all the other identities
  list_for_each_entry(i, &self->identities, identities) {
    CALL(i, add, uri, attribute, value);
  };
};

static void oracle_del(Resolver self, char *uri, char *attribute) {
  Resolver i;

  // Call the real add method for the oracle itself
  __Resolver.del(self, uri, attribute);

  // Add these to all the other identities
  list_for_each_entry(i, &self->identities, identities) {
    CALL(i, del, uri, attribute);
  };
};

void AFF2_Init(void) {
  FileLikeObject_init();
  FileBackedObject_init();
  ZipFile_init();
  ZipFileStream_init();
  Image_init();
  MapDriver_init();
  Resolver_init();
  Link_init();

#ifdef HAVE_LIBCURL
  HTTPObject_init();
#endif

  Encrypted_init();
  DirVolume_init();
  Identity_init();

  init_luts();

  // Make a global oracle
  if(!oracle) {
    // Create the oracle - it has a special add method which distributes
    // all the adds to the other identities
    oracle =CONSTRUCT(Resolver, Resolver, Con, NULL);
    oracle->del = oracle_del;
    oracle->add = oracle_add;
  };
};


/** Implementation of Caches */

/** A destructor on the cache object to automatically unlink us from
    the lists.
*/
static int Cache_destructor(void *this) {
  Cache self = (Cache) this;
  list_del(&self->cache_list);
  list_del(&self->hash_list);

  // We can automatically maintain a tally of elements in the cache
  // because we have a reference to the main cache object here.
  if(self->cache_head)
    self->cache_head->cache_size--;
  return 0;
};

/** A max_cache_size of 0 means we never expire anything */
static Cache Cache_Con(Cache self, int hash_table_width, int max_cache_size) {
  self->hash_table_width = hash_table_width;
  self->max_cache_size = max_cache_size;

  INIT_LIST_HEAD(&self->cache_list);
  INIT_LIST_HEAD(&self->hash_list);

  // Install our destructor
  talloc_set_destructor((void *)self, Cache_destructor);
  return self;
};

/** Quick and simple */
static unsigned int Cache_hash(Cache self, void *key) {
  char *name = (char *)key;
  int len = strlen(name);
  char result = 0;
  int i;
  for(i=0; i<len; i++) 
    result ^= name[i];

  return result % self->hash_table_width;
};

static int Cache_cmp(Cache self, void *other) {
  return strcmp((char *)self->key, (char *)other);
};

static Cache Cache_put(Cache self, void *key, void *data, int data_len) {
  unsigned int hash;
  Cache hash_list_head;
  Cache new_cache;
  Cache i;

  // Check to see if we need to expire something else. We do this
  // first to avoid the possibility that we might expire the same key
  // we are about to add.
  while(self->max_cache_size > 0 && self->cache_size >= self->max_cache_size) {
    list_for_each_entry(i, &self->cache_list, cache_list) {
      talloc_free(i);
      break;
    };
  };

  if(!self->hash_table)
    self->hash_table = talloc_array(self, Cache, self->hash_table_width);

  hash = CALL(self, hash, key);
  hash_list_head = self->hash_table[hash];
  new_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  // Make sure the new cache member knows where the list head is. We
  // only keep stats about the cache here.
  new_cache->cache_head = self;

  // Take over the data
  new_cache->key = key;
  new_cache->data = data;
  new_cache->data_len = data_len;
  talloc_steal(new_cache, key);
  talloc_steal(new_cache, data);

  if(!hash_list_head) {
    hash_list_head = self->hash_table[hash] = CONSTRUCT(Cache, Cache, Con, self, 
							HASH_TABLE_SIZE, -10);
    talloc_set_name_const(hash_list_head, "Hash Head");
  };

  //  printf("Adding %p\n", new_cache);
  list_add_tail(&new_cache->hash_list, &self->hash_table[hash]->hash_list);
  list_add_tail(&new_cache->cache_list, &self->cache_list);
  self->cache_size ++;
 
  return new_cache;
};

static Cache Cache_get(Cache self, void *key) {
  int hash;
  Cache hash_list_head;
  Cache i;

  hash = CALL(self, hash, key);
  if(!self->hash_table) return NULL;

  hash_list_head = self->hash_table[hash];

  // There are 2 lists each Cache object is on - the hash list is a
  // shorter list at the end of each hash table slot, while the cache
  // list is a big list of all objects in the cache. We find the
  // object using the hash list, but expire the object based on the
  // cache list which is also kept in sorted order.
  if(!hash_list_head) return NULL;
  list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
    if(!CALL(i, cmp, key)) {
      // Thats it - we remove it from where its in and put it on the
      // tail:
      if(self->policy == CACHE_EXPIRE_LEAST_USED) {
	list_move(&i->cache_list, &self->cache_list);
	list_move(&i->hash_list, &hash_list_head->hash_list);
      };
      //printf("Getting %p\n", i);
      return i;
    };
  };
  
  return NULL;
};

static void *Cache_get_item(Cache self, char *key) {
  Cache tmp = CALL(self, get, key);

  if(tmp && tmp->data)
    return tmp->data;

  return NULL;
};

VIRTUAL(Cache, Object)
     VMETHOD(Con) = Cache_Con;
     VMETHOD(put) = Cache_put;
     VMETHOD(cmp) = Cache_cmp;
     VMETHOD(hash) = Cache_hash;
     VMETHOD(get) = Cache_get;
     VMETHOD(get_item) = Cache_get_item;
END_VIRTUAL

/***********************************************************
   This is the implementation of the resolver.
***********************************************************/

static Resolver Resolver_Con(Resolver self) {
  // This is a persistent cache for all object relations we know about
  self->urn = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  talloc_set_name_const(self->urn, "Main Resolver");

  // This is a cache for frequently used objects
  self->cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 50);
  talloc_set_name_const(self->cache, "Temporary Cache");

  // This is a cache for writers:
  self->writers = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 50);
  talloc_set_name_const(self->writers, "Writer Cache");

  INIT_LIST_HEAD(&self->identities);

  return self;
};

static char *Resolver_resolve(Resolver self, char *urn, char *attribute) {
  Cache i=CALL(self->urn, get_item, urn);

  if(!i) {
    /*
    RaiseError(ERuntimeError, "Unable to locate attribute %s for urn %s",
	       attribute, urn);
    */
    return NULL;
  };

  return CALL(i, get_item, attribute);
};

static char **Resolver_resolve_list(Resolver self, void *ctx, char *urn, char *attribute) {
  Cache i=CALL(self->urn, get_item, urn);
  StringIO result = CONSTRUCT(StringIO, StringIO, Con, ctx);
  Cache j;
  int hash;

  if(!i) {
    RaiseError(ERuntimeError, "Unable to locate attribute %s for urn %s",
	       attribute, urn);
    goto exit;
  };

  hash = CALL(i, hash, attribute);
  if(i->hash_table && i->hash_table[hash]) {
    list_for_each_entry(j, &i->hash_table[hash]->hash_list, hash_list) {
      char *key = (char *)j->key;
      char **existing = (char **)&j->data;
      if(!strcmp(key, attribute)) {
	// Push the string onto the result - note that we rely on the
	// fact that the existing string will never be freed.
	CALL(result, write, (char *)existing, sizeof(existing));
      };
    };
  };

 exit:
  // NULL terminate it
  i=0;
  CALL(result, write, (char *)&i, sizeof(i));
  talloc_steal(ctx, result->data);

  return (char **)result->data;
};

static AFFObject Resolver_open(Resolver self, void *ctx, char *urn, char mode) {
  int i;
  char *stream_type;
  struct dispatch_t *dispatch_ptr=NULL;
  AFFObject result;
  Cache tmp;
  char buff[BUFF_SIZE];

  if(!urn) return NULL;

  // Is this object cached?
  if(mode =='r') {
    tmp = CALL(self->cache, get, urn);
  } else {
    tmp = CALL(self->writers, get, urn);
  };

  if(tmp) {
    result = (AFFObject)tmp->data;
    talloc_steal(ctx, result);
    talloc_free(tmp);
    return result;
  };

  // OK Maybe the type is encoded into the URN:
  for(i=0; dispatch[i].type !=NULL; i++) {
    if(dispatch[i].scheme && startswith(urn, dispatch[i].type)) {
      dispatch_ptr = &dispatch[i];
      break;
    };
  };

  // Nope - maybe its stated explicitely
  if(!dispatch_ptr) {
    stream_type = CALL(self, resolve, urn, AFF4_TYPE);
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
      return NULL;
    } else {
      RaiseError(ERuntimeError, "Unable to open %s", urn);
      return NULL;
    };
  };
  
  // A special constructor from a class reference
  result = CONSTRUCT_FROM_REFERENCE(dispatch[i].class_ptr, 
				    Con, ctx, urn, mode);
  
  // Make sure the object mode is set
  if(result)
    ((AFFObject)result)->mode = mode;
  return result; 
};

/** Return the object to the cache. Callers may not make a reference
    to it after that.
*/
static void Resolver_return(Resolver self, AFFObject obj) {
  // Cache it
  if(!obj) return;

  if(obj->mode == 'r') {
    CALL(self->cache, put, talloc_strdup(self, obj->urn),
	 obj, sizeof(*obj));
  } else if(obj->mode == 'w') {
    CALL(self->writers, put, talloc_strdup(self, obj->urn),
	 obj, sizeof(*obj));
  } else {
    RaiseError(ERuntimeError, "Programming error. %s has no valid mode", NAMEOF(obj));
  };
};

static void Resolver_add(Resolver self, char *uri, char *attribute, char *value) {
  Cache tmp;

  if(CALL(self, is_set, uri, attribute, value)) return;

  //LogWarnings("%s %s=%s", uri, attribute, (char *)value);

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

static int Resolver_is_set(Resolver self, char *uri, char *attribute, char *value) {
  // See if the same value is already in there, if so we dont want it
  // again (use the hash table for some speed)
  Cache tmp =  CALL(self->urn, get_item, uri);
  int hash;
  Cache i;

  if(!tmp) return 0;

  hash = CALL(tmp, hash, attribute);
  if(tmp->hash_table && tmp->hash_table[hash]) {
    list_for_each_entry(i, &tmp->hash_table[hash]->hash_list, hash_list) {
      char *key = (char *)i->key;
      char *existing = (char *)i->data;
      if(!strcmp(key, attribute) && !strcmp(existing, value)){
	return 1;
      };
    };
  };

  return 0;
};

/** Format all the attributes of the object specified by urn */
static char *Resolver_export_urn(Resolver self, char *urn, char *context) {
  char *result=talloc_strdup(NULL, "");
  Cache i = CALL(self->urn, get_item, urn);
  Cache j;

  if(!i) return result;

  list_for_each_entry(j, &i->cache_list, cache_list) {
    char *attribute = (char *)j->key;
    char *value = (char *)j->data;

    // Do not write volatile data
    if(memcmp(attribute, ZSTRING_NO_NULL(VOLATILE_NS))) {
      if(!strcmp(urn, context)) {
	// Relative form
	result = talloc_asprintf_append(result, "%s=%s\n",
					attribute, value);
      } else {
	// Absolute form
        result = talloc_asprintf_append(result, "%s %s=%s\n", urn, 
					attribute, value);
      };
    };
  };

  return result;
};

static char *Resolver_export_all(Resolver self, char *context) {
  Cache i;
  char *result=talloc_strdup(NULL, "");  

  list_for_each_entry(i, &self->urn->cache_list, cache_list) {
    char *urn = (char *)i->key;
    char *tmp =Resolver_export_urn(self, urn, context);

    result = talloc_asprintf_append(result, "%s", tmp);
    talloc_free(tmp);
  };
  return result;
};

static AFFObject Resolver_create(Resolver self, AFFObject *class_reference) {
  AFFObject result;
  if(!class_reference || !*class_reference) return NULL;

  result = CONSTRUCT_FROM_REFERENCE((*class_reference), Con, self, NULL, 'w');

  return result;
};

static void Resolver_del(Resolver self, char *uri, char *attribute) {
  Cache tmp,j;

  tmp = CALL(self->urn, get_item, uri);
  if(!tmp) return;
  
  while(1) {
    j = CALL(tmp, get, attribute);
    if(!j) break;
    //    LogWarnings("Removing %s %s",uri, attribute);
    talloc_free(j);
  };
};
  
static void Resolver_set(Resolver self, char *uri, char *attribute, char *value) {
  CALL(self, del, uri, attribute);
  CALL(self, add, uri, attribute, value);
};

// Parse a properties file (implicit context is context - if the file
// does not specify a subject URN we use context instead).
static void Resolver_parse(Resolver self, char *context_urn, char *text, int len) {
  int i,j,k;
  // Make our own local copy so we can modify it (the original is
  // cached and doesnt belong to us).
  char *tmp;
  char *tmp_text;
  char *source;
  char *attribute;
  char *value;
  
  tmp = talloc_memdup(self, text, len+1);
  tmp[len]=0;
  tmp_text = tmp;
  
  // Find the next line:
  while((i=strcspn(tmp_text, "\r\n"))) {
    tmp_text[i]=0;
    
    // Locate the =
    j=strcspn(tmp_text,"=");
    if(j==i) goto exit;
    
    tmp_text[j]=0;
    value = tmp_text + j + 1;
    
    // Locate the space
    k=strcspn(tmp_text," ");
    if(k==j) {
      // No absolute URN specified, we use the current filename:
      source = talloc_strdup(tmp, context_urn);
      attribute = tmp_text;
    } else {
      source = tmp_text;
      attribute = tmp_text + k+1;
    };
    tmp_text[k]=0;
    
    /** If source is not a FQN, we use the context_urn instead */
    if(strstr(source, FQN) != source) {
      source = context_urn;
    };
        
    // Now add to the global resolver (These will all be possibly
    // stolen).
    CALL(self, add, 
	 talloc_strdup(tmp, source),
	 talloc_strdup(tmp, attribute),
	 talloc_strdup(tmp, value));
    
    // Move to the next line
    tmp_text = tmp_text + i+1;
  };
  
 exit:
  talloc_free(tmp);
};

/** Here we implement the resolver */
VIRTUAL(Resolver, AFFObject)
     VMETHOD(Con) = Resolver_Con;
     VMETHOD(create) = Resolver_create;

     VMETHOD(resolve) = Resolver_resolve;
     VMETHOD(resolve_list) = Resolver_resolve_list;
     VMETHOD(add)  = Resolver_add;
     VMETHOD(export_urn) = Resolver_export_urn;
     VMETHOD(export_all) = Resolver_export_all;
     VMETHOD(open) = Resolver_open;
     VMETHOD(cache_return) = Resolver_return;
     VMETHOD(set) = Resolver_set;
     VMETHOD(is_set) = Resolver_is_set;
     VMETHOD(del) = Resolver_del;
     VMETHOD(parse) = Resolver_parse;
END_VIRTUAL

/************************************************************
  AFFObject - This is the base class for all other objects
************************************************************/
static AFFObject AFFObject_Con(AFFObject self, char *uri, char mode) {
  uuid_t uuid;
  char *uuid_str;
  
  if(!uri) {
    // This function creates a new stream from scratch so the stream
    // name will be a new UUID
    uuid_generate(uuid);
    uuid_str = talloc_size(self, 40);
    uuid_unparse(uuid, uuid_str);

    uri = talloc_asprintf(self, FQN "%s", uuid_str);
  };

  if(!self->urn)
    self->urn = uri;

  return self;
};


static void AFFObject_set_property(AFFObject self, char *attribute, char *value) {
  CALL(oracle, set,
       self->urn,
       attribute, value);
};

// Prepares an object to be used
static AFFObject AFFObject_finish(AFFObject self) {
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

