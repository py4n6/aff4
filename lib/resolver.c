/** This file implemnts the resolver */
#include "aff4.h"
#include <uuid/uuid.h>
#include <pthread.h>
#include <tdb.h>
#include <raptor.h>
#include <errno.h>

//int AFF4_TDB_FLAGS = TDB_NOLOCK | TDB_NOSYNC | TDB_VOLATILE | TDB_INTERNAL;
//int AFF4_TDB_FLAGS = TDB_INTERNAL;
int AFF4_TDB_FLAGS = TDB_DEFAULT;

#define RESOLVER_CACHE_SIZE 10

#define TDB_HASH_SIZE 1024*128

// Some prototypes
static int Resolver_lock(Resolver self, RDFURN urn, char mode);
static int Resolver_unlock(Resolver self, RDFURN urn, char mode);

// This is a big dispatcher of all AFFObjects we know about. We call
// their AFFObjects::Con(urn, mode) constructor.
static Cache type_dispatcher = NULL;

/** This is the global oracle - it knows everything about everyone. */
Resolver oracle = NULL;

void register_type_dispatcher(char *type, AFFObject *classref) {
  if(!(*classref)->dataType) {
    printf("%s has no dataType\n", NAMEOF(*classref));
  };

  // Make the various dispatchers
  if(!type_dispatcher) {
    type_dispatcher = CONSTRUCT(Cache, Cache, Con, NULL, 100, 0);
    talloc_set_name_const(type_dispatcher, "Type Dispatcher");
  };

  if(!CALL(type_dispatcher, present, ZSTRING(type))) {
    // Make a copy of the class for storage in the registry
    Object tmp =  talloc_memdup(NULL, classref, SIZEOF(classref));
    talloc_set_name(tmp, "handler %s for type '%s'", NAMEOF(classref), type);

    CALL(type_dispatcher, put, ZSTRING(type), tmp);
    talloc_unlink(NULL, tmp);
  };
};

void AFF4_Init(void) {
  INIT_CLASS(Resolver);
  INIT_CLASS(AFFObject);
  INIT_CLASS(Logger);

  image_init();
  mapdriver_init();
  zip_init();
  rdf_init();

#ifdef HAVE_OPENSSL
  encrypt_init();
#endif

#ifdef HAVE_LIBCURL
  //  HTTPObject_init();
#endif

#ifdef HAVE_LIBAFFLIB
  //  AFF1Volume_init();
  // AFF1Stream_init();
#endif

#ifdef HAVE_EWF
  EWF_init();
  // EWFStream_init();
#endif

  //  Encrypted_init();
  //  DirVolume_init();
  // Identity_init();

  init_luts();

  // Make a global oracle
  if(!oracle) {
    // Create the oracle - it has a special add method which distributes
    // all the adds to the other identities
    oracle =CONSTRUCT(Resolver, Resolver, Con, NULL, 0);
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
static unsigned int Cache_hash(Cache self, char *key, int len) {
  // Use int size to obtain the hashes this should be faster... NOTE
  // that we dont process up to the last 3 bytes.
  unsigned char *name = (unsigned char *)key;
  unsigned int result = 0;
  int i;
  int count = len / sizeof(*name);

  for(i=0; i<count; i++)
    result ^= name[i];

  return result % self->hash_table_width;
};

static int Cache_cmp(Cache self, char *other, int len) {
  return memcmp((char *)self->key, (char *)other, len);
};

static int print_cache(Cache self) {
  Cache i;

  list_for_each_entry(i, &self->cache_list, cache_list) {
    printf("%s: %p\n", i->key, i);
  };

  return 0;
};

static void Cache_put(Cache self, char *key, int len, Object data) {
  unsigned int hash;
  Cache hash_list_head;
  Cache new_cache;
  Cache i;

  // Check to see if we need to expire something else. We do this
  // first to avoid the possibility that we might expire the same key
  // we are about to add.
  while(self->max_cache_size > 0 && self->cache_size >= self->max_cache_size) {
    list_for_each_entry(i, &self->cache_list, cache_list) {
      // Try to free the data object and if that succeeds we free the
      // Cache itself. This allows the data to set a destructor which
      // blocks this free. Sometimes its crucial to ensure that a
      // cached object does not get expired suddenly when its not
      // ready.
      if(talloc_free(i) != -1) {
        break;
      };
    };
  };

  if(!self->hash_table)
    self->hash_table = talloc_array(self, Cache, self->hash_table_width);

  hash = CALL(self, hash, key, len);
  assert(hash <= self->hash_table_width);

  new_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  // Make sure the new cache member knows where the list head is. We
  // only keep stats about the cache here.
  new_cache->cache_head = self;

  talloc_set_name(new_cache, "Cache key %s", key);

  // Take over the data
  new_cache->key = talloc_memdup(new_cache, key, len);
  new_cache->key_len = len;

  new_cache->data = data;
  if(data)
    talloc_reference(self, data);

  hash_list_head = self->hash_table[hash];
  if(!hash_list_head) {
    hash_list_head = self->hash_table[hash] = CONSTRUCT(Cache, Cache, Con, self,
							HASH_TABLE_SIZE, -10);
    talloc_set_name_const(hash_list_head, "Hash Head");
  };

  list_add_tail(&new_cache->hash_list, &self->hash_table[hash]->hash_list);
  list_add_tail(&new_cache->cache_list, &self->cache_list);
  self->cache_size ++;

  return;
};

static Object Cache_get(Cache self, char *key, int len) {
  int hash;
  Cache hash_list_head;
  Cache i;

  if(!self->hash_table) return NULL;

  hash = CALL(self, hash, key, len);
  hash_list_head = self->hash_table[hash];
  if(!hash_list_head) return NULL;

  // There are 2 lists each Cache object is on - the hash list is a
  // shorter list at the end of each hash table slot, while the cache
  // list is a big list of all objects in the cache. We find the
  // object using the hash list, but expire the object based on the
  // cache list which is also kept in sorted order.
  list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
    if(i->key_len == len && !CALL(i, cmp, key, len)) {
      Object result = i->data;

      // When we return the object we increase its reference to null,
      // and remove the reference from us (we dont own it and NULL
      // owns it). This makes sure it is not freed.
      if(result) {
        talloc_reference(NULL, result);
        talloc_unlink(self, result);
      };

      // Now free the container
      talloc_free(i);

      return result;
    };
  };

  RaiseError(EKeyError, "Key '%s' not found in Cache", key);
  return NULL;
};


static Object Cache_borrow(Cache self, char *key, int len) {
  int hash;
  Cache hash_list_head;
  Cache i;

  if(!self->hash_table) return NULL;

  hash = CALL(self, hash, key, len);
  hash_list_head = self->hash_table[hash];
  if(!hash_list_head) return NULL;

  // There are 2 lists each Cache object is on - the hash list is a
  // shorter list at the end of each hash table slot, while the cache
  // list is a big list of all objects in the cache. We find the
  // object using the hash list, but expire the object based on the
  // cache list which is also kept in sorted order.
  list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
    if(i->key_len == len && !CALL(i, cmp, key, len)) {
      return i->data;
    };
  };

  return NULL;
};


int Cache_present(Cache self, char *key, int len) {
  int hash;
  Cache hash_list_head;
  Cache i;

  if(self->hash_table) {
    hash = CALL(self, hash, key, len);
    hash_list_head = self->hash_table[hash];
    if(!hash_list_head) return 0;

    list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
      if(i->key_len == len && !CALL(i, cmp, key, len)) {
        return 1;
      };
    };
  };

  return 0;
};

VIRTUAL(Cache, Object) {
     VMETHOD(Con) = Cache_Con;
     VMETHOD(put) = Cache_put;
     VMETHOD(cmp) = Cache_cmp;
     VMETHOD(hash) = Cache_hash;
     VMETHOD(get) = Cache_get;
     VMETHOD(present) = Cache_present;
     VMETHOD(borrow) = Cache_borrow;
} END_VIRTUAL

/**********************************************************
   The following are utilities that will be needed later
***********************************************************/
#define MAX_KEY "__MAX"
#define VOLATILE_NS "aff4volatile:"

/** Some constants */
static TDB_DATA INHERIT = {
  .dptr = (unsigned char *)"aff4:inherit",
  .dsize = 12
};

static TDB_DATA WLOCK = {
  .dptr = (unsigned char *)VOLATILE_NS "WLOCK",
  .dsize = 18
};

static TDB_DATA RLOCK = {
  .dptr = (unsigned char *)VOLATILE_NS "RLOCK",
  .dsize = 18
};

static TDB_DATA LOCK = {
  .dptr = (unsigned char *)"LOCK",
  .dsize = 4
};

/* Given an int serialise into the buffer */
static int tdb_serialise_int(uint64_t i, char *buff, int buff_len) {
  int result = snprintf(buff, buff_len, "__%lld", i);

  return min(result, buff_len);
};

/** Given a buffer unserialise an int from it */
static uint64_t tdb_to_int(TDB_DATA string) {
  unsigned char buff[BUFF_SIZE];
  int buff_len = min(string.dsize, BUFF_SIZE-1);

  if(buff_len < 2) return 0;

  memcpy(buff, string.dptr, buff_len);

  //Make sure its null terminated
  buff[buff_len]=0;

  return strtoll((char *)buff+2, NULL, 0);
};

/** Fetches the id for the given key from the database tdb - if
    create_new is set and there is no id present, we create a new id
    and return id.
*/
static uint32_t get_id(struct tdb_context *tdb, TDB_DATA key, int create_new) {
  char buff[BUFF_SIZE];
  TDB_DATA urn_id;
  uint32_t max_id=0;
  uint32_t result=0;

  /* We get given an ID and we retrieve the URN it belongs to */
  tdb_lockall(tdb);
  urn_id = tdb_fetch(tdb, key);

  if(urn_id.dptr) {
    result = tdb_to_int(urn_id);
    free(urn_id.dptr);

    tdb_unlockall(tdb);
    return result;
  } else if(create_new) {
    TDB_DATA max_key;

    max_key = tdb_data_from_string(MAX_KEY);

    urn_id = tdb_fetch(tdb, max_key);
    if(urn_id.dptr) {
      max_id = tdb_to_int(urn_id);
      free(urn_id.dptr);
    };

    max_id++;

    // Update the new MAX_KEY
    urn_id.dptr = (unsigned char *)buff;
    urn_id.dsize = tdb_serialise_int(max_id, buff, BUFF_SIZE);
    tdb_store(tdb, key, urn_id, TDB_REPLACE);
    tdb_store(tdb, max_key, urn_id, TDB_REPLACE);
    tdb_store(tdb, urn_id, key, TDB_REPLACE);

    tdb_unlockall(tdb);
    return max_id;
  };

  tdb_unlockall(tdb);
  // This will only happen if create_new is false and the key is not
  // found:
  return 0;
};

/***********************************************************
   This is the implementation of the resolver.
***********************************************************/
static int Resolver_destructor(void *this) {
  Resolver self = (Resolver) this;
  // Close all the tdb handles
  tdb_close(self->urn_db);
  self->urn_db = NULL;

  tdb_close(self->attribute_db);
  self->attribute_db = NULL;

  tdb_close(self->data_db);
  self->data_db = NULL;

  close(self->data_store_fd);
  self->data_store_fd = 0;

  return 0;
};

static void store_rdf_registry(Resolver self) {
  Cache i;

  list_for_each_entry(i, &RDF_Registry->cache_list, cache_list) {
    TDB_DATA tmp;
    int attribute_id;
    RDFValue value_class = (RDFValue)i->data;

    // Make sure the objects we are dealing with are actually of the
    // right type:
    if(ISSUBCLASS(value_class, RDFValue)) {
      tmp = tdb_data_from_string(value_class->dataType);

      attribute_id = get_id(self->attribute_db, tmp, 1);
      ((RDFValue)CLASSOF(value_class))->id = attribute_id;
    };
  };
};

static Resolver Resolver_Con(Resolver self, int mode) {
  char buff[BUFF_SIZE];
  // The location of the TDB databases
  char *path = getenv("AFF4_TDB_PATH");

  if(mode & RESOLVER_MODE_NONPERSISTANT)
    AFF4_TDB_FLAGS |= TDB_CLEAR_IF_FIRST;

  if(mode & RESOLVER_MODE_DEBUG_MEMORY)
    talloc_enable_leak_report_full();

  // Make this object a singleton - the Resolver may be constructed as
  // many times as needed, but we always return a reference to the
  // singleton oracle if it exists
  if(oracle) {
    talloc_free(self);
    talloc_increase_ref_count(oracle);

    return oracle;
  };

  if(!path)  path = ".";

  self->logger = CONSTRUCT(Logger, Logger, Con, self);

  // Open the TDB databases
  if(snprintf(buff, BUFF_SIZE, "%s/urn.tdb", path) >= BUFF_SIZE)
    goto error;

  self->urn_db = tdb_open(buff, TDB_HASH_SIZE,
			  AFF4_TDB_FLAGS,
			  O_RDWR | O_CREAT, 0644);
  if(!self->urn_db)
    goto error;

  if(snprintf(buff, BUFF_SIZE, "%s/attribute.tdb", path) >= BUFF_SIZE)
    goto error1;

  self->attribute_db = tdb_open(buff, TDB_HASH_SIZE,
				AFF4_TDB_FLAGS,
				O_RDWR | O_CREAT, 0644);
  
  if(!self->attribute_db) 
    goto error1;

  if(snprintf(buff, BUFF_SIZE, "%s/data.tdb", path) >= BUFF_SIZE)
    goto error2;

  self->data_db = tdb_open(buff, TDB_HASH_SIZE,
			   AFF4_TDB_FLAGS,
			   O_RDWR | O_CREAT, 0644);

  if(!self->data_db)
    goto error2;
  
  if(snprintf(buff, BUFF_SIZE, "%s/data_store.tdb", path) >= BUFF_SIZE)
    goto error3;
  
  self->data_store_fd = open(buff, O_RDWR | O_CREAT, 0644);
  if(self->data_store_fd < 0)
    goto error3;

  if(AFF4_TDB_FLAGS & TDB_CLEAR_IF_FIRST) {
    ftruncate(self->data_store_fd, 0);
  };

  // This ensures that the data store never has an offset of 0 (This
  // indicates an error)
  // Access to the tdb_store is managed via locks on the data.tdb
  tdb_lockall(self->data_db);
  if(lseek(self->data_store_fd, 0, SEEK_END)==0) {
    (void)write(self->data_store_fd, "data",4);
  };
  tdb_unlockall(self->data_db);

  // Populate the attribute_db with the RDF_Registry
  store_rdf_registry(self);

  // Create local read and write caches
  CALL(self, flush);

  INIT_LIST_HEAD(&self->identities);

  // Make sure we close our files when we get freed
  talloc_set_destructor((void *)self, Resolver_destructor);

  // Cache this so we dont need to rebuilt it all the time.
  self->type = new_XSDString(self);

  return self;

 error3:
  tdb_close(self->data_db);
  self->data_db = NULL;
 error2:
  tdb_close(self->attribute_db);
  self->attribute_db = NULL;
 error1:
  tdb_close(self->urn_db);
  self->urn_db = NULL;
 error:
  RaiseError(ERuntimeError, "Unable to open tdb files in '%s'", path);

  return NULL;
};

/** Writes the data key onto the buffer - this is a combination of the
    uri_id and the attribute_id
*/
static int calculate_key(Resolver self, TDB_DATA uri,
			 TDB_DATA attribute, char *buff,
			 int buff_len, int create_new) {
  uint32_t urn_id = get_id(self->urn_db, uri, create_new);
  uint32_t attribute_id = get_id(self->attribute_db, attribute, create_new);

  // urn or attribute not found
  if(urn_id == 0 || attribute_id == 0) return 0;

  return snprintf(buff, buff_len, "%d:%d", urn_id, attribute_id);
};

/** returns the list head in the data file for the uri and attribute
    specified. Returns the offset in the data_store for the TDB_DATA_LIST 
*/
static uint64_t get_data_head(Resolver self, TDB_DATA uri, TDB_DATA attribute, 
			      TDB_DATA_LIST *result) {
  char buff[BUFF_SIZE];
  TDB_DATA data_key;

  data_key.dptr = (unsigned char *)buff;
  data_key.dsize = calculate_key(self, uri, attribute, buff, BUFF_SIZE, 0);

  if(data_key.dsize > 0) {
    // We found these attribute/urn
    TDB_DATA offset_serialised = tdb_fetch(self->data_db, data_key);

    if(offset_serialised.dptr) {
      // We found the head - read the struct
      uint32_t offset = tdb_to_int(offset_serialised);

      lseek(self->data_store_fd, offset, SEEK_SET);
      if(read(self->data_store_fd, result, sizeof(*result)) == sizeof(*result)) {
        free(offset_serialised.dptr);
	return offset;
      };

      free(offset_serialised.dptr);
    };
  };

  return 0;
};

// Returns the offset in the data_store for the TDB_DATA_LIST 
static inline uint64_t get_data_next(Resolver self, TDB_DATA_LIST *i){
  uint64_t next_offset = i->next_offset;

  if(next_offset > 0) {
    lseek(self->data_store_fd, i->next_offset, SEEK_SET);
    if(read(self->data_store_fd, i, sizeof(*i)) == sizeof(*i)) {
      return next_offset;
    };
  };

  return 0;
};

/** Resolves a single attribute and returns the value. Value will be
    talloced with the context of ctx.
*/
static int Resolver_resolve_value(Resolver self, RDFURN urn_str, char *attribute_str,
			     RDFValue result) {
  TDB_DATA_LIST i;
  TDB_DATA urn, attribute;

  urn = tdb_data_from_string(urn_str->value);
  attribute = tdb_data_from_string(attribute_str);

  DEBUG("Getting %s, %s\n", urn_str->value, attribute_str);
  if(get_data_head(self, urn, attribute, &i)) {
    char buff[i.length];

    // Make sure its the type our caller expects
    if(i.encoding_type != result->id) {
      RaiseError(ERuntimeError, "Request for attribute %s with type %d - but its stored with type %d\n", attribute_str, result->id, i.encoding_type);
      goto not_found;
    };

    // Now get the RDFValue class to read itself
    if(read(self->data_store_fd, buff, i.length) != i.length)
      goto not_found;

    if(!CALL(result, decode, buff, i.length)) {
      RaiseError(ERuntimeError, "%s is unable to decode %s:%s from TDB store", NAMEOF(result),
		 urn_str, attribute_str);
      goto not_found;
    };

    return i.length;
  };

 not_found:
  return 0;
};

// Allocate and return the first value
static RDFValue Resolver_resolve_alloc(Resolver self, void *ctx, RDFURN urn, char *attribute) {
  RESOLVER_ITER *iter = CALL(self, get_iter, ctx, urn, attribute);

  if(iter) {
    RDFValue result = CALL(self, iter_next_alloc, iter);

    if(result) {
      return result;
    };
    talloc_free(iter);
  };

  return NULL;
};

static int set_new_value(Resolver self, TDB_DATA urn, TDB_DATA attribute,
			 TDB_DATA value, int type_id, uint64_t previous_offset) {
  TDB_DATA key,offset;
  char buff[BUFF_SIZE];
  char buff2[BUFF_SIZE];
  uint32_t new_offset;
  TDB_DATA_LIST i;

  if(type_id == 0) abort();

  // Update the value in the db and replace with new value
  key.dptr = (unsigned char *)buff;
  key.dsize = calculate_key(self, urn, attribute, buff, BUFF_SIZE, 1);

  // Lock the database
  tdb_lockall(self->data_db);

  // Go to the end and write the new record
  new_offset = lseek(self->data_store_fd, 0, SEEK_END);
  // The offset to the next item in the list
  i.next_offset = previous_offset;
  i.length = value.dsize;
  i.encoding_type = type_id;

  write(self->data_store_fd, &i, sizeof(i));
  write(self->data_store_fd, value.dptr, value.dsize);

  offset.dptr = (unsigned char *)buff2;
  offset.dsize = tdb_serialise_int(new_offset, buff2, BUFF_SIZE);

  tdb_store(self->data_db, key, offset, TDB_REPLACE);

  //Done
  tdb_unlockall(self->data_db);

  return 1;
};

/** This sets a single triple into the resolver replacing previous
    values set for this attribute
*/
static void Resolver_set_value(Resolver self, RDFURN urn, char *attribute_str,
			       RDFValue value) {
  TDB_DATA *encoded_value = CALL(value, encode);

  if(encoded_value) {
    uint64_t data_offset;
    TDB_DATA_LIST iter;
    TDB_DATA attribute = tdb_data_from_string(attribute_str);

    DEBUG("Setting %s, %s\n", urn->value, attribute_str);

    // Grab the lock
    tdb_lockall(self->data_db);

    // Try to overwrite a previously set property
    data_offset = get_data_head(self,
                                tdb_data_from_string(urn->value),
                                attribute,
                                &iter);

    if(data_offset > 0 && iter.length == encoded_value->dsize) {
      // Ensure that previous values are removed too
      lseek(self->data_store_fd, data_offset, SEEK_SET);
      iter.next_offset = 0;
      iter.encoding_type = value->id;
      write(self->data_store_fd, (char *)&iter, sizeof(iter));
      write(self->data_store_fd, encoded_value->dptr, encoded_value->dsize);
    } else {
      set_new_value(self, tdb_data_from_string(urn->value),
		    attribute, *encoded_value, value->id, 0);
    };

    tdb_unlockall(self->data_db);

    // Done with the encoded value
    talloc_free(encoded_value);
  } else {
    printf("Failed to set %s:%s\n", urn->value, attribute_str);
  };
};

static void Resolver_add_value(Resolver self, RDFURN urn, char *attribute_str,
			       RDFValue value) {
  TDB_DATA *encoded_value = CALL(value, encode);

  if(encoded_value) {
    TDB_DATA attribute;
    uint64_t previous_offset;
    TDB_DATA_LIST tmp;

    DEBUG("Adding %s, %s\n", urn->value, attribute_str);
    attribute = tdb_data_from_string(attribute_str);

    // Grab the lock
    tdb_lockall(self->data_db);

    /** If the value is already in the list, we just ignore this
	request.
    */
    previous_offset = get_data_head(self,
	   tdb_data_from_string(urn->value), attribute, &tmp);

    set_new_value(self, tdb_data_from_string(urn->value),
		  attribute, *encoded_value, value->id, previous_offset);

    tdb_unlockall(self->data_db);

    // Done with the encoded value
    talloc_free(encoded_value);
  };
};

RESOLVER_ITER *_Resolver_get_iter(Resolver self,
                                  void *ctx,
                                  TDB_DATA tdb_urn,
                                  TDB_DATA attribute) {
  RESOLVER_ITER *iter = talloc(ctx, RESOLVER_ITER);

    memset(&iter->head, 0, sizeof(iter->head));
  iter->offset = get_data_head(self, tdb_urn, attribute, &iter->head);

  iter->cache = CONSTRUCT(Cache, Cache, Con, iter, HASH_TABLE_SIZE, 0);

  return iter;
};

static RESOLVER_ITER *Resolver_get_iter(Resolver self,
                                        void *ctx,
                                        RDFURN urn,
                                        char *attribute_str) {
  TDB_DATA attribute, tdb_urn;

  attribute = tdb_data_from_string(attribute_str);
  tdb_urn = tdb_data_from_string(urn->value);

  return _Resolver_get_iter(self, ctx, tdb_urn, attribute);
};

static int Resolver_iter_next(Resolver self,
			      RESOLVER_ITER *iter,
			      RDFValue result) {
  int res_code;

  do {
    res_code = 0;

    // We assume the iter is valid and we write the contents of the iter
    // on here.
    // This is our iteration exit condition
    if(iter->offset == 0)
      return 0;

    /* If the value is of the correct type, ask it to read from
       the file. */
    if(result->id == iter->head.encoding_type) {
      /** We need to cache the triple "data", RDFValue_id because two
      different RDFValue implementations may use the same encoding,
      but they should be treated as different.
      */
      uint32_t id = iter->head.encoding_type;
      int key_len = iter->head.length + sizeof(id);
      char *key = talloc_size(iter, key_len);

      lseek(self->data_store_fd, iter->offset + sizeof(TDB_DATA_LIST), SEEK_SET);
      read(self->data_store_fd, key, iter->head.length);

      memcpy(key + iter->head.length, &id, sizeof(id));

      // Check if the value was already seen:
      if(!CALL(iter->cache, present, key, key_len)) {
        // No did not see it before - store it in the Cache. NOTE this
        // doesnt actually store anything, but simply ensure the key
        // is stored.
        CALL(iter->cache, put, key, key_len, NULL);

        res_code = CALL(result, decode, key, iter->head.length);
      } else {
        // We have seen this before:
        talloc_free(key);
      };
    };

    // Get the next iterator
    iter->offset = get_data_next(self, &iter->head);
  } while(res_code == 0);

  return res_code;
};

static RDFValue Resolver_iter_next_alloc(Resolver self,
					 RESOLVER_ITER *iter) {
  RDFValue rdf_value_class;
  RDFValue result;
  TDB_DATA attribute, dataType;
  char buff[BUFF_SIZE];

  // This is our iteration exit condition
  while(iter->offset != 0) {
    // We try to find the type of the next item, instantiate it and then
    // parse it normally.
    attribute.dptr = (unsigned char *)buff;
    attribute.dsize = tdb_serialise_int(iter->head.encoding_type, buff, BUFF_SIZE);

    // strings are always stored in the tdb with null termination
    dataType = tdb_fetch(self->attribute_db, attribute);
    if(dataType.dptr) {
      rdf_value_class = (RDFValue)CALL(RDF_Registry, borrow, (char *)dataType.dptr,
                                       dataType.dsize);
      if(rdf_value_class) {
        result = (RDFValue)CONSTRUCT_FROM_REFERENCE(rdf_value_class,
                                                    Con, iter);

        if(result) {
          if(Resolver_iter_next(self, iter, result))
            return result;
          else {
            talloc_free(result);
          };
        };
      };

      free(dataType.dptr);
      iter->offset = get_data_next(self, &iter->head);
    };
  };

  return NULL;
};

/** Instantiates a single instance of the class using this
    resolver. The returned object MUST be returned to the cache using
    cache_return(). We initially create the object using the NULL
    context, and when the object is returned to the cache, it will be
    stolen again.
*/
static AFFObject Resolver_open(Resolver self, RDFURN urn, char mode) {
  AFFObject result;
  AFFObject classref;
  char *scheme;
  TDB_DATA tdb_urn = tdb_data_from_string(urn->value);

  if(!urn) {
    RaiseError(ERuntimeError, "No URN specified");
    goto error;
  };

  DEBUG("Opening %s for mode %c\n", urn->value, mode);

  // Is this object cached?
  if(mode =='r') {
    /*
    if(CALL(self->rlocks, present, TDB_DATA_STRING(tdb_urn))) {
      RaiseError(ERuntimeError, "URN %s is locked (r)", urn->value);
      goto error;
    };
    */

    result = (AFFObject)CALL(self->read_cache, get, TDB_DATA_STRING(tdb_urn));
    //    if(result)
    //  printf("%s fetched from cache\n", urn->value);

  } else {
    // If the object is write locked, we raise an error here.

    if(CALL(self->wlocks, present, TDB_DATA_STRING(tdb_urn))) {
      RaiseError(ERuntimeError, "URN %s is locked (w)", urn->value);
      goto error;
    };

    result = (AFFObject)CALL(self->write_cache, get, TDB_DATA_STRING(tdb_urn));
  };

  if(result) {
    // If its a FileLikeObject seek it to 0
    if(ISSUBCLASS(result, FileLikeObject)) {
      CALL((FileLikeObject)result, seek, 0, SEEK_SET);
    };

    // Lock it
    Resolver_lock(self, URNOF(result), mode);
    goto exit;
  };

  // Here we need to build a new instance of this class using the
  // dispatcher. We first try the scheme:
  scheme = urn->parser->scheme;

  // Empty schemes default to file:// URLs
  if(!scheme || strlen(scheme)==0)
    scheme = "file";

  classref = (AFFObject)CALL(type_dispatcher, borrow, ZSTRING(scheme));
  if(!classref) {
    if(!CALL(self, resolve_value, urn, AFF4_TYPE, (RDFValue)self->type)) {
      RaiseError(ERuntimeError, "No type found for %s", urn->value);
      goto error;
    };

    classref = (AFFObject)CALL(type_dispatcher, borrow, ZSTRING(self->type->value));
    if(!classref) {
      RaiseError(ERuntimeError, "Unable to open urn - this implementation can not open objects of type %s", self->type->value);
      goto error;
    };
  };

  // A special constructor from a class reference
  result = CONSTRUCT_FROM_REFERENCE(classref, Con, NULL, urn, mode);
  if(!result) goto error;

  // Make sure the object mode is set, name the object for talloc
  // memory reports and lock it.
  talloc_set_name(result, "%s %s", NAMEOF(result), STRING_URNOF(result));
  ((AFFObject)result)->mode = mode;

  // Lock it
  Resolver_lock(self, URNOF(result), mode);

 exit:
  // Lock the urn
  if(mode == 'r') {
    //CALL(self->rlocks, put, TDB_DATA_STRING(tdb_urn), (Object)result);
  } else {
    CALL(self->wlocks, put, TDB_DATA_STRING(tdb_urn), (Object)result);
  };

  // Non error path
  ClearError();
  return result;

 error:
  return NULL;
};

/** Return the object to the cache. Callers may not make a reference
    to it after that.
*/
static void Resolver_cache_return(Resolver self, AFFObject obj) {
  char *urn = URNOF(obj)->value;
  Object result;

  if(obj->mode == 'r') {
    CALL(self->read_cache, put, ZSTRING(urn), (Object)obj);
  } else if(obj->mode == 'w') {
    CALL(self->write_cache, put, ZSTRING(urn), (Object)obj);
  } else {
    RaiseError(ERuntimeError, "Programming error. %s has no valid mode", NAMEOF(obj));
    return;
  };

  if(obj->mode == 'r') {
    //result = CALL(self->rlocks, get, ZSTRING(urn));
    //if(result) talloc_unlink(NULL, result);
  } else if(obj->mode == 'w') {
    result = CALL(self->wlocks, get, ZSTRING(urn));
    if(result) talloc_unlink(NULL, result);
  };

  // Unlock the URN
  Resolver_unlock(self, URNOF(obj), obj->mode);

  // We free it here, but there should still be a cache reference.
  talloc_unlink(NULL, obj);
  ClearError();
};

// A helper method to construct the class
static AFFObject Resolver_create(Resolver self, char *name, char mode) {
  AFFObject class_reference;
  AFFObject result;

  class_reference = (AFFObject)CALL(type_dispatcher, borrow, ZSTRING(name));
  if(!class_reference) {
    RaiseError(ERuntimeError, "No handler for object type '%s'", name);
    goto error;
  };

  // Newly created objects belong in the write cache
  result = (AFFObject)CONSTRUCT_FROM_REFERENCE((class_reference),
                                               Con, self->write_cache, NULL, mode);
  if(!result) goto error;

  talloc_set_name(result, "%s (%c) (created by resolver)", NAMEOF(class_reference), mode);

  DEBUG("Created %s with URN %s\n", NAMEOF(result), URNOF(result)->value);

  return result;

 error:
  return NULL;
};

static void Resolver_del(Resolver self, RDFURN urn, char *attribute_str) {
  TDB_DATA attribute;
  TDB_DATA key;
  char buff[BUFF_SIZE];

  if(attribute_str) {
    attribute = tdb_data_from_string(attribute_str);

    key.dptr = (unsigned char *)buff;
    key.dsize = calculate_key(self, tdb_data_from_string(urn->value),
			      attribute, buff, BUFF_SIZE, 0);

    // Remove the key from the database
    tdb_delete(self->data_db, key);
    DEBUG("Removing attribute %s from %s\n", attribute_str, urn->value);
  } else {
    TDB_DATA max_key;
    int max_id,i;

    DEBUG("Removing all attributes from %s\n", urn->value);
    max_key = tdb_data_from_string(MAX_KEY);

    max_key = tdb_fetch(self->attribute_db, max_key);
    if(max_key.dptr) {
      max_id = tdb_to_int(max_key);
      free(max_key.dptr);
    };

    for(i=0;i<max_id;i++) {
      char buff[BUFF_SIZE];
      int urn_id =get_id(self->urn_db,
			 tdb_data_from_string(urn->value), 0);

      key.dptr = (unsigned char *)buff;
      key.dsize = snprintf(buff, BUFF_SIZE, "%d:%d", urn_id, i);

      // Remove the key from the database
      tdb_delete(self->data_db, key);
    };
  };
};

/** Synchronization methods. */
int Resolver_lock_gen(Resolver self, RDFURN urn, char mode, int sense) {
  TDB_DATA attribute;
  uint64_t offset;
  TDB_DATA_LIST data_list;

  if(mode == 'r') attribute=RLOCK;
  else if(mode =='w') attribute=WLOCK;
  else {
    RaiseError(ERuntimeError, "Invalid mode %c", mode);
    return 0;
  };

  offset = get_data_head(self, tdb_data_from_string(urn->value),
			 attribute, &data_list);
  if(!offset){
    // The attribute is not set - make it now:
    set_new_value(self, tdb_data_from_string(urn->value),
		  attribute, LOCK, -1, 0);

    offset = get_data_head(self, tdb_data_from_string(urn->value),
			   attribute, &data_list);
    if(!offset) {
      RaiseError(ERuntimeError, "Unable to set lock attribute");
      return 0;
    };
  };

  // If we get here data_list should be valid:
  lseek(self->data_store_fd, offset, SEEK_SET);
  if(lockf(self->data_store_fd, sense, data_list.length)==-1){
    RaiseError(ERuntimeError, "Unable to lock: %s", strerror(errno));
    return 0;
  };

  return 1;
};

static int Resolver_lock(Resolver self, RDFURN urn, char mode) {
  DEBUG("locking %s mode %c\n", urn->value, mode);
  return Resolver_lock_gen(self, urn, mode, F_LOCK);
};

static int Resolver_unlock(Resolver self, RDFURN urn, char mode) {
  DEBUG("releasing %s mode %c\n", urn->value, mode);

  return Resolver_lock_gen(self, urn, mode, F_ULOCK);
};

static int Resolver_get_id_by_urn(Resolver self, RDFURN uri) {
  return get_id(self->urn_db, tdb_data_from_string(uri->value), 1);
};

static int Resolver_get_urn_by_id(Resolver self, int id, RDFURN uri) {
  TDB_DATA urn_id, result;
  char buff[BUFF_SIZE];

  urn_id.dptr = (unsigned char *)buff;
  urn_id.dsize = tdb_serialise_int(id, buff, BUFF_SIZE);

  result = tdb_fetch(self->urn_db, urn_id);
  if(result.dptr) {
    CALL(uri, set, (char *)result.dptr);
    free(result.dptr);
    return 1;
  };

  return 0;
};

void Resolver_register_rdf_value_class(Resolver self, RDFValue class_ref) {
  register_rdf_value_class(class_ref);
  talloc_increase_ref_count(class_ref);

  // Sync up the rdf registry with the attribute tdb
  store_rdf_registry(self);
};

RDFValue Resolver_new_rdfvalue(Resolver self, void *ctx, char *type) {

  return new_rdfvalue(ctx, type);
};

int Resolver_load(Resolver self, RDFURN uri) {
  /** We iterate over all the type handlers which are derived from the
      AFF4Volume abstract class, and try to instantiate them in their
      preferred order.
  */
  Cache i;

  list_for_each_entry(i, &type_dispatcher->cache_list, cache_list) {
    if(ISSUBCLASS(i->data, AFF4Volume)) {
      AFF4Volume class_ref = *(AFF4Volume *)i->data;
      AFF4Volume volume = (AFF4Volume)CALL(oracle, create,
                                           ((AFFObject)class_ref)->dataType, 'r');

      if(CALL(volume, load_from, uri, 'r')) {
        CALL(uri, set, STRING_URNOF(volume));
        CALL(oracle, cache_return, (AFFObject)volume);

        ClearError();
        return 1;
      };

      talloc_free(volume);
    };
  };

  ClearError();
  return 0;
};

static void Resolver_set_logger(Resolver self, Logger logger) {
  if(self->logger) talloc_free(self->logger);

  self->logger = logger;
  talloc_reference(self, logger);
};

static void Resolver_flush(Resolver self) {
  if(self->rlocks) {
    printf("Read locks\n");
    print_cache(self->rlocks);
  };

  if(self->wlocks) {
    printf("Write locks\n");
    print_cache(self->wlocks);
  };


  if(self->read_cache)
    talloc_free(self->read_cache);

  if(self->write_cache)
    talloc_free(self->write_cache);

  if(self->wlocks)
    talloc_free(self->wlocks);

  if(self->rlocks)
    talloc_free(self->rlocks);

  self->read_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, RESOLVER_CACHE_SIZE);
  talloc_set_name_const(self->read_cache, "Resolver Read Cache");

  self->write_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, RESOLVER_CACHE_SIZE);
  talloc_set_name_const(self->write_cache, "Resolver Write Cache");

  self->rlocks = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  talloc_set_name_const(self->rlocks, "Resolver RLock Cache");

  self->wlocks = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  talloc_set_name_const(self->wlocks, "Resolver WLock Cache");
};

static void Resolver_expire(Resolver self, RDFURN uri) {
  /** Remove the object from the cache */
  Object obj;

  obj = CALL(self->read_cache, get, ZSTRING(uri->value));
  if(obj) talloc_unlink(self->read_cache, obj);

  obj = CALL(self->write_cache, get, ZSTRING(uri->value));
  if(obj) talloc_unlink(self->write_cache, obj);


  // If the objects are in the lock cache they are outstanding... This
  // is a problem because someone else has a hold of them right now -
  // what to do? for now do nothing.

  // Now as the object to delete itself
  if(CALL(self, resolve_value, uri, AFF4_TYPE, (RDFValue)self->type)) {
    AFFObject class_reference = CALL(type_dispatcher, borrow, ZSTRING(self->type->value));

    if(class_reference) {
      class_reference->delete(uri);
    };
  };
};


/** Here we implement the resolver */
VIRTUAL(Resolver, Object) {
     VMETHOD(Con) = Resolver_Con;
     VMETHOD(create) = Resolver_create;

     VMETHOD(resolve_value) = Resolver_resolve_value;
     VMETHOD(resolve_alloc) = Resolver_resolve_alloc;
     VMETHOD(get_iter) = Resolver_get_iter;
     VMETHOD(iter_next) = Resolver_iter_next;
     VMETHOD(iter_next_alloc) = Resolver_iter_next_alloc;
     VMETHOD(open) = Resolver_open;
     VMETHOD(cache_return) = Resolver_cache_return;
     VMETHOD(set_value) = Resolver_set_value;
     VMETHOD(add_value) = Resolver_add_value;
     VMETHOD(del) = Resolver_del;
     VMETHOD(expire) = Resolver_expire;

     VMETHOD(get_id_by_urn) = Resolver_get_id_by_urn;
     VMETHOD(get_urn_by_id) = Resolver_get_urn_by_id;
     VMETHOD(register_rdf_value_class) = Resolver_register_rdf_value_class;
     VMETHOD(new_rdfvalue) = Resolver_new_rdfvalue;

     VMETHOD(load) = Resolver_load;
     VMETHOD(set_logger) = Resolver_set_logger;
     VMETHOD(flush) = Resolver_flush;
} END_VIRTUAL

/************************************************************
  AFFObject - This is the base class for all other objects
************************************************************/
static AFFObject AFFObject_Con(AFFObject self, RDFURN uri, char mode) {
  uuid_t uuid;
  char uuid_str[BUFF_SIZE];

  if(!uri) {
    // This function creates a new stream from scratch so the stream
    // name will be a new UUID
    strcpy(uuid_str, FQN);
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str + strlen(FQN));

    self->urn = new_RDFURN(self);
    self->urn->set(self->urn, uuid_str);
  } else {
    self->urn = CALL(uri, copy, self);
  };

  self->mode = mode;

  return self;
};


static void AFFObject_set_property(AFFObject self, char *attribute, RDFValue value) {
  CALL(oracle, set_value,
       self->urn,
       attribute, value);
};

// Prepares an object to be used
static AFFObject AFFObject_finish(AFFObject self) {
  return CALL(self, Con, URNOF(self), self->mode);
};

static void AFFObject_delete(RDFURN urn) {
  CALL(oracle, del, urn, NULL);
};

static void AFFObject_cache_return(AFFObject self) {
  CALL(oracle, cache_return, self);
};

VIRTUAL(AFFObject, Object) {
     VMETHOD(finish) = AFFObject_finish;
     VMETHOD(set_attribute) = AFFObject_set_property;
     VMETHOD(delete) = AFFObject_delete;
     VMETHOD(cache_return) = AFFObject_cache_return;

     VMETHOD(Con) = AFFObject_Con;
} END_VIRTUAL

/* Prints out all available volume drivers */
void print_volume_drivers() {
  Cache i;

  printf("Valid volume drivers: \n");

  list_for_each_entry(i, &type_dispatcher->cache_list, cache_list) {
    AFFObject class_ref = (AFFObject)i->data;

    if(ISSUBCLASS(class_ref, ZipFile)) {
      printf(" - %s\n", (char *)i->key);
    };
  };
};

static Logger Logger_Con(Logger self) {
  return self;
};

static void Logger_message(Logger self, int level, char *message) {
  printf("%s\n", message);
};

VIRTUAL(Logger, Object) {
  VMETHOD(Con) = Logger_Con;
  VMETHOD(message) = Logger_message;
} END_VIRTUAL
