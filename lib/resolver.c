/** This file implemnts the resolver */
#include "aff4.h"
#include <uuid/uuid.h>
#include <pthread.h>
#include <tdb.h>
#include <raptor.h>
#include <errno.h>
#include <pthread.h>
#include "config.h"

//int AFF4_TDB_FLAGS = TDB_NOLOCK | TDB_NOSYNC | TDB_VOLATILE | TDB_INTERNAL;
//int AFF4_TDB_FLAGS = TDB_INTERNAL;
int AFF4_TDB_FLAGS = TDB_DEFAULT;

#define RESOLVER_CACHE_SIZE 20

#define TDB_HASH_SIZE 1024*128

// Some extra debugging
#define LOCK_RESOLVER pthread_mutex_lock(&self->mutex); \
  DEBUG_LOCK("Locking resolver %u\n", self->mutex.__data.__count)

#define UNLOCK_RESOLVER  pthread_mutex_unlock(&self->mutex);    \
  DEBUG_LOCK("Unlocking resolver %u\n", self->mutex.__data.__count)

// Prototypes
int Resolver_lock_gen(Resolver self, RDFURN urn, char mode, int sense);

// This is a big dispatcher of all AFFObjects we know about. We call
// their AFFObjects::Con(urn, mode) constructor.
static Cache type_dispatcher = NULL;

/** This is the global oracle - it knows everything about
    everyone. Note: The Resolver() class is a singleton which returns
    the oracle if it already exists.
*/
Resolver oracle = NULL;

void register_type_dispatcher(Resolver self, char *type,        \
                              AFFObject *classref) {
  if(!(*classref)->dataType) {
    printf("%s has no dataType\n", NAMEOF(*classref));
  };

  // Make the various dispatchers
  if(!type_dispatcher) {
    type_dispatcher = CONSTRUCT(Cache, Cache, Con, NULL, 100, 0);
    talloc_set_name_const(type_dispatcher, "Type Dispatcher");
  };

  if(1 || !CALL(type_dispatcher, present, ZSTRING(type))) {
    // Make a copy of the class for storage in the registry
    Object tmp =  talloc_memdup(NULL, classref, SIZEOF(classref));
    talloc_set_name(tmp, "handler %s for type '%s'", NAMEOF(classref), type);

    CALL(type_dispatcher, put, ZSTRING(type), tmp);
    talloc_unlink(NULL, tmp);
  };
};

static void Resolver_register_type_dispatcher(Resolver self, char *type, \
                                              AFFObject classref) {
  if(!(classref)->dataType) {
    RaiseError(ERuntimeError,"%s has no dataType\n", NAMEOF(classref));
    goto error;
  };

  if(1 || !CALL(type_dispatcher, present, ZSTRING(type))) {
    Object old_obj = CALL(type_dispatcher, get, ZSTRING(type));

    if(old_obj) talloc_free(old_obj);
    // Make a copy of the class for storage in the registry
    talloc_set_name(classref, "handler %s for type '%s'", NAMEOF(classref), type);

    CALL(type_dispatcher, put, ZSTRING(type), (Object)classref);
  };

 error:
  return;
};

/** Implementation of Caches */

/** FIXME - Need to implement hash table rebalancing */

/** A destructor on the cache object to automatically unlink us from
    the lists.
*/
static int Cache_destructor(void *this) {
  Cache self = (Cache) this;
  list_del(&self->cache_list);
  list_del(&self->hash_list);

  // We can automatically maintain a tally of elements in the cache
  // because we have a reference to the main cache object here.
  /* It is not a good idea to keep count of objects in the cache by
     reference counting because they might be alive due to external
     references (e.g. in python).
  */
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
  // Alternate function - maybe faster?
  //  unsigned uint32_t *name = (unsigned uint32_t *)key;
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
    printf("%s %p %p\n",(char *) i->key,i , i->data);
  };

  return 0;
};

static Cache Cache_put(Cache self, char *key, int len, Object data) {
  unsigned int hash;
  Cache hash_list_head;
  Cache new_cache;
  Cache i;

  // Check to see if we need to expire something else. We do this
  // first to avoid the possibility that we might expire the same key
  // we are about to add.
  while(self->max_cache_size > 0 && self->cache_size >= self->max_cache_size) {
    list_for_each_entry(i, &self->cache_list, cache_list) {
      talloc_unlink(self, i);
      break;
    };
  };

  if(!self->hash_table)
    self->hash_table = talloc_array(self, Cache, self->hash_table_width);

  hash = CALL(self, hash, key, len);
  assert(hash <= self->hash_table_width);

  /** Note - this form allows us to extend Cache without worrying
  about updating this method. We replicate the size of the object we
  are automatically.
  */
  new_cache = CONSTRUCT_FROM_REFERENCE((Cache)CLASSOF(self), Con, self, HASH_TABLE_SIZE, 0);
  // Make sure the new cache member knows where the list head is. We
  // only keep stats about the cache here.
  new_cache->cache_head = self;

  talloc_set_name(new_cache, "Cache key %s", key);

  // Take over the data
  new_cache->key = talloc_memdup(new_cache, key, len);
  new_cache->key_len = len;

  new_cache->data = data;
  if(data)
    talloc_reference(new_cache, data);

  hash_list_head = self->hash_table[hash];
  if(!hash_list_head) {
    hash_list_head = self->hash_table[hash] = CONSTRUCT(Cache, Cache, Con, self,
							HASH_TABLE_SIZE, -10);
    talloc_set_name_const(hash_list_head, "Hash Head");
  };

  list_add_tail(&new_cache->hash_list, &self->hash_table[hash]->hash_list);
  list_add_tail(&new_cache->cache_list, &self->cache_list);
  self->cache_size ++;

  return new_cache;
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
      };

      // Now free the container this will unlink it from the lists
      // through its destructor.
      talloc_unlink(self, i);

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

static Object Cache_iter(Cache self, char *key, int len) {
  Cache i, hash_list_head;
  int hash;

  if(self->hash_table) {
    hash = CALL(self, hash, key, len);
    hash_list_head = self->hash_table[hash];
    if(!hash_list_head)
      goto error;

    list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
      // If we find ourselves pointing at the list head again we did
      // not find it:
      if(!i->cache_head)
        goto error;

      if(i->key_len == len && !CALL(i, cmp, key, len)) {
        goto exit;
      };
    };
  };

 error:
  return NULL;

 exit:
  return (Object)i;
};

static Object Cache_next(Cache self, Object *opaque_iter) {
  Cache iter = (Cache)*opaque_iter;
  Cache i;
  char *key;
  int len;
  Object result;

  if(!iter) return NULL;

  key = iter->key;
  len = iter->key_len;

  // Search for the next occurance. We must already be on the right
  // hash list
  list_for_each_entry(i, &iter->hash_list, hash_list) {
    // If we find ourselves pointing at the list head again we did
    // not find it:
    if(!i->cache_head) {
      // Indicate no more elements
      *opaque_iter = NULL;
      break;
    };

    if(i->key_len == len && !CALL(i, cmp, key, len)) {
      // Set the next element
      *opaque_iter = (Object)i;
      break;
    };
  };

  // Now we return a reference to the original object
  result = iter->data;

  // We refresh the current object in the cache:
  list_move_tail(&iter->cache_list, &self->cache_list);

  return result;
};


VIRTUAL(Cache, Object) {
     VMETHOD(Con) = Cache_Con;
     VMETHOD(put) = Cache_put;
     VMETHOD(cmp) = Cache_cmp;
     VMETHOD(hash) = Cache_hash;
     VMETHOD(get) = Cache_get;
     VMETHOD(present) = Cache_present;
     VMETHOD(borrow) = Cache_borrow;

     VMETHOD(iter) = Cache_iter;
     VMETHOD(next) = Cache_next;

     VMETHOD(print_cache) = print_cache;

} END_VIRTUAL

/**********************************************************
   The following are utilities that will be needed later
***********************************************************/
#define VOLATILE_NS "aff4volatile:"

/** Some constants */
static TDB_DATA MAX_KEY = {
  .dptr = (unsigned char *)"__MAX",
  .dsize = 6
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
static inline int tdb_serialise_int(uint64_t i, char *buff, int buff_len) {
  if(buff_len < 8) return 0;

  *(uint64_t *)buff = i;

  return sizeof(i);
};

/** Given a buffer unserialise an int from it */
static inline uint64_t tdb_to_int(TDB_DATA string) {
  if(string.dsize != 8) return 0;

  return *(uint64_t *)(string.dptr);
};

/** Fetches the id for the given key from the database tdb - if
    create_new is set and there is no id present, we create a new id
    and return id.
*/
static inline uint32_t get_id(struct tdb_context *tdb, TDB_DATA key, int create_new) {
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
    urn_id = tdb_fetch(tdb, MAX_KEY);
    if(urn_id.dptr) {
      max_id = tdb_to_int(urn_id);
      free(urn_id.dptr);
    };

    max_id++;

    // Update the new MAX_KEY
    urn_id.dptr = (unsigned char *)buff;
    urn_id.dsize = tdb_serialise_int(max_id, buff, BUFF_SIZE);
    tdb_store(tdb, key, urn_id, TDB_REPLACE);
    tdb_store(tdb, MAX_KEY, urn_id, TDB_REPLACE);
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

  pthread_mutex_destroy(&self->mutex);

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
      ((RDFValue)value_class)->id = attribute_id;
      ((RDFValue)CLASSOF(value_class))->id = attribute_id;

      DEBUG_OBJECT("RDF type %p %s - %u\n", value_class,\
                   NAMEOF(value_class), ((RDFValue)value_class)->id);
    };
  };
};

static Resolver Resolver_Con(Resolver self, int mode) {
  char buff[BUFF_SIZE];
  // The location of the TDB databases
  char *path = getenv("AFF4_TDB_PATH");
  pthread_mutexattr_t mutex_attr;

  self->mode = mode;

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

  if(!path)  path = TDB_LOCATION;

  self->logger = CONSTRUCT(Logger, Logger, Con, self);

  // This is recursive to ensure that only one thread can hold the
  // resolver at once. Once the thread holds the resolver its free to
  // continue calling functions from it.
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
  //pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
  pthread_mutex_init(&self->mutex, &mutex_attr);
  pthread_mutexattr_destroy(&mutex_attr);

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
  self->type = new_RDFURN(self);

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
  RaiseError(ERuntimeError, "Unable to open tdb files in '%s' (%s)", path,\
             strerror(errno));

  return NULL;
};

static inline int calculate_key_from_ids(int urn_id, int attribute_id, char *buff, int buff_len) {
  // urn or attribute not found
  if(urn_id == 0 || attribute_id == 0 || buff_len < sizeof(urn_id)*2) return 0;

  *(uint32_t *)buff = urn_id;
  *(uint32_t *)(buff + sizeof(urn_id)) = attribute_id;

  return sizeof(urn_id) + sizeof(attribute_id);
};

/** Writes the data key onto the buffer - this is a combination of the
    uri_id and the attribute_id
*/
static int calculate_key(Resolver self, TDB_DATA uri,
			 TDB_DATA attribute, char *buff,
			 int buff_len, int create_new) {
  uint32_t urn_id = get_id(self->urn_db, uri, create_new);
  uint32_t attribute_id = get_id(self->attribute_db, attribute, create_new);

  return calculate_key_from_ids(urn_id, attribute_id, buff, buff_len);
};

static inline uint64_t get_data_head_by_key(Resolver self, TDB_DATA data_key,
                                            TDB_DATA_LIST *result) {
  // We found these attribute/urn
  TDB_DATA offset_serialised = tdb_fetch(self->data_db, data_key);

  if(offset_serialised.dptr) {
    // We found the head - read the struct
    uint64_t offset = tdb_to_int(offset_serialised);

    lseek(self->data_store_fd, offset, SEEK_SET);
    if(read(self->data_store_fd, result, sizeof(*result)) == sizeof(*result)) {
      free(offset_serialised.dptr);
      return offset;
    };

    free(offset_serialised.dptr);
  };
  return 0;
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
    return get_data_head_by_key(self, data_key, result);
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

  LOCK_RESOLVER

  urn = tdb_data_from_string(urn_str->value);
  attribute = tdb_data_from_string(attribute_str);

  DEBUG_RESOLVER("Getting %s, %s\n", urn_str->value, attribute_str);
  if(get_data_head(self, urn, attribute, &i)) {
    char buff[i.length];

    /* If the called is really not interested in the value we just
       return. The allows callers to just test for the existance of an
       attribute.
    */
    if(!result) return i.length;

    // Make sure its the type our caller expects
    if(i.encoding_type != result->id) {
      RaiseError(ERuntimeError, "Request for attribute %s with type %d - but its stored with type %d\n", attribute_str, result->id, i.encoding_type);
      goto not_found;
    };

    // Now get the RDFValue class to read itself
    if(read(self->data_store_fd, buff, i.length) != i.length)
      goto not_found;

    if(!CALL(result, decode, buff, i.length, urn_str)) {
      // Make sure we propagate the original error:
      RaiseError(ERuntimeError, "%s is unable to decode %s:%s from TDB store", NAMEOF(result),
                 urn_str, attribute_str);
      goto not_found;
    };

    UNLOCK_RESOLVER;
    return i.length;
  };

 not_found:

  UNLOCK_RESOLVER;
  return 0;
};

// Allocate and return the first value
static RDFValue Resolver_resolve_alloc(Resolver self, void *ctx, RDFURN urn, char *attribute) {
  RESOLVER_ITER *iter;

  LOCK_RESOLVER;

  iter = CALL(self, get_iter, ctx, urn, attribute);

  // Only try to allocate the first object from the iterator. FIXME
  // this should iterate through all the objects until one works.
  if(iter) {
    RDFValue result = CALL(self, alloc_from_iter, ctx, iter);
    talloc_free(iter);

    if(result) {
      UNLOCK_RESOLVER;

      return result;
    };
  };

  UNLOCK_RESOLVER;
  return NULL;
};

static void set_new_value(Resolver self, RESOLVER_ITER *iter,
                                    TDB_DATA urn, TDB_DATA attribute,
                                    TDB_DATA value, int type_id, uint8_t flags,
                                    uint32_t source_id,
                                    uint64_t previous_offset) {
  TDB_DATA key,offset;
  char buff[BUFF_SIZE];
  char buff2[BUFF_SIZE];

  if(type_id == 0) abort();

  // Update the value in the db and replace with new value
  key.dptr = (unsigned char *)buff;
  key.dsize = calculate_key(self, urn, attribute, buff, BUFF_SIZE, 1);

  // Lock the database
  tdb_lockall(self->data_db);

  // Go to the end and write the new record
  iter->offset = lseek(self->data_store_fd, 0, SEEK_END);
  // The offset to the next item in the list
  iter->head.next_offset = previous_offset;
  iter->head.length = value.dsize;
  iter->head.encoding_type = type_id;
  iter->head.asserter_id = source_id;
  iter->head.flags = flags;

  write(self->data_store_fd, &iter->head, sizeof(iter->head));
  write(self->data_store_fd, value.dptr, value.dsize);

  offset.dptr = (unsigned char *)buff2;
  offset.dsize = tdb_serialise_int(iter->offset, buff2, BUFF_SIZE);

  tdb_store(self->data_db, key, offset, TDB_REPLACE);

  //Done
  tdb_unlockall(self->data_db);

  return;
};

/** This sets a single triple into the resolver replacing previous
    values set for this attribute
*/
static int Resolver_set_value(Resolver self, RDFURN urn, char *attribute_str,
                              RDFValue value, RESOLVER_ITER *iter) {
  TDB_DATA *encoded_value;
  RESOLVER_ITER temp;
  RESOLVER_ITER *result = iter ? iter : &temp;

  LOCK_RESOLVER;

  encoded_value = CALL(value, encode, urn);

  if(encoded_value) {
    uint64_t data_offset;
    TDB_DATA attribute = tdb_data_from_string(attribute_str);

#ifdef AFF4_DEBUG_RESOLVER
    {
      char *serialised = CALL(value, serialise, urn);

      DEBUG_RESOLVER("Setting %s, %s = %s\n", urn->value, attribute_str, serialised);

      // Its annoying to have different free functions
      if(value->raptor_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE) {
        raptor_free_uri((raptor_uri*)serialised);
      } else
        talloc_unlink(NULL, serialised);
    };
#endif

    // Grab the lock
    tdb_lockall(self->data_db);

    // Try to overwrite a previously set property
    data_offset = get_data_head(self,
                                tdb_data_from_string(urn->value),
                                attribute,
                                &result->head);

    if(data_offset > 0 && result->head.length == encoded_value->dsize) {
      // Ensure that previous values are removed too
      lseek(self->data_store_fd, data_offset, SEEK_SET);
      result->head.next_offset = 0;
      result->head.encoding_type = value->id;
      result->head.flags = value->flags;
      result->offset = data_offset;

      write(self->data_store_fd, (char *)&result->head, sizeof(result->head));
      write(self->data_store_fd, encoded_value->dptr, encoded_value->dsize);
    } else {
      set_new_value(self, result, tdb_data_from_string(urn->value),
                    attribute, *encoded_value, value->id,
                    value->flags, -1, 0);
    };

    tdb_unlockall(self->data_db);

    // Done with the encoded value
    talloc_free(encoded_value);

    UNLOCK_RESOLVER;
    return 1;

  } else {
    DEBUG_RESOLVER("Failed to set %s:%s\n", urn->value, attribute_str);
    UNLOCK_RESOLVER;
    return 0;
  };
};

static int Resolver_add_value(Resolver self,
                              RDFURN urn, char *attribute_str,
                              RDFValue value, RESOLVER_ITER *iter) {
  TDB_DATA *encoded_value;
  RESOLVER_ITER temp;
  RESOLVER_ITER *result = iter ? iter : &temp;

  LOCK_RESOLVER;

  encoded_value = CALL(value, encode, urn);

  if(encoded_value) {
    TDB_DATA attribute;
    uint64_t previous_offset;

    DEBUG_RESOLVER("Adding %s, %s\n", urn->value, attribute_str);
    attribute = tdb_data_from_string(attribute_str);

    // Grab the lock
    tdb_lockall(self->data_db);

    /** For efficiency we do not check that the value is not already
	in the list. When we iterate over all the items in the list,
	we check then for unique values so it doesnt matter if we
	store duplicates.
    */
    previous_offset = get_data_head(self,
	   tdb_data_from_string(urn->value), attribute, &result->head);

    set_new_value(self, result, tdb_data_from_string(urn->value),
		  attribute, *encoded_value, value->id, 0, value->flags,
                  previous_offset);

    tdb_unlockall(self->data_db);

    // Done with the encoded value
    talloc_free(encoded_value);
  };

  UNLOCK_RESOLVER;
  return 1;
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
  RESOLVER_ITER *result;

  LOCK_RESOLVER;

  attribute = tdb_data_from_string(attribute_str);
  tdb_urn = tdb_data_from_string(urn->value);

  result = _Resolver_get_iter(self, ctx, tdb_urn, attribute);
  result->urn = urn;

  talloc_reference(result, urn);

  UNLOCK_RESOLVER;
  return result;
};

static int Resolver_iter_next(Resolver self,
			      RESOLVER_ITER *iter,
			      RDFValue result) {
  int res_code;

  LOCK_RESOLVER;

  do {
    res_code = 0;

    // We assume the iter is valid and we write the contents of the iter
    // on here.
    // This is our iteration exit condition
    if(iter->offset == 0)
      goto exit;

    /* If the value is of the correct type, ask it to read from
       the file. */
    if(result && result->id == iter->head.encoding_type) {
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
      if(iter->cache) {
        if(!CALL(iter->cache, present, key, key_len)) {
          // No did not see it before - store it in the Cache. NOTE this
          // doesnt actually store anything, but simply ensure the key
          // is stored.
          CALL(iter->cache, put, key, key_len, NULL);
        };
      };
      res_code = CALL(result, decode, key, iter->head.length, iter->urn);

      talloc_free(key);
    };

    // Get the next iterator
    iter->offset = get_data_next(self, &iter->head);
  } while(res_code == 0);

 exit:
  UNLOCK_RESOLVER;
  return res_code;
};

static char *Resolver_encoded_data_from_iter(Resolver self, RDFValue *rdf_value_class,
                                             RESOLVER_ITER *iter) {
  TDB_DATA attribute, dataType;
  char buff[BUFF_SIZE];
  // Now read the data from the file
  char *result = NULL;

  LOCK_RESOLVER;

  // This is our iteration exit condition
  if(iter->offset != 0) {
    // We try to find the type of the next item, instantiate it and then
    // parse it normally.
    attribute.dptr = (unsigned char *)buff;
    attribute.dsize = tdb_serialise_int(iter->head.encoding_type, buff, BUFF_SIZE);

    // strings are always stored in the tdb with null termination
    dataType = tdb_fetch(self->attribute_db, attribute);
    if(dataType.dptr) {
      *rdf_value_class = (RDFValue)CALL(RDF_Registry, borrow, (char *)dataType.dptr,
                                       dataType.dsize);

      result = talloc_size(NULL, iter->head.length + 1);
      lseek(self->data_store_fd, iter->offset + sizeof(TDB_DATA_LIST), SEEK_SET);
      read(self->data_store_fd, result, iter->head.length);

      result[iter->head.length] = 0;
      free(dataType.dptr);
    };
  };

  // Advance the iterator:
  Resolver_iter_next(self, iter, NULL);

  UNLOCK_RESOLVER;

  return result;
};


/* Given an iterator, allocate an RDFValue from it */
static RDFValue Resolver_alloc_from_iter(Resolver self, void *ctx,
					 RESOLVER_ITER *iter) {
  RDFValue rdf_value_class;
  RDFValue result = NULL;
  TDB_DATA attribute, dataType;
  char buff[BUFF_SIZE];

  LOCK_RESOLVER;

  // This is our iteration exit condition
  if(iter->offset != 0) {
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
                                                    Con, ctx);
      };

      if(result) {
        if(!Resolver_iter_next(self, iter, result)){
          talloc_free(result);
          result = NULL;
        };
      };

      free(dataType.dptr);
    };
  };

  UNLOCK_RESOLVER;

  return result;
};

/** Instantiates a single instance of the class using this
    resolver. The returned object MUST be returned to the cache using
    cache_return(). We initially create the object using the NULL
    context, and when the object is returned to the cache, it will be
    stolen again.

    IMPORTANT NOTE: Memory ownership:

    - create - new memory is referenced by NULL
    - open - new memory is referenced by NULL - in addition cached
           memory is also referenced by the cache.
    - cache_return - if object is not in cache it gets put there
           (taking a new reference from the cache). NULL reference is
           removed.
    - close - object is cache_returned (i.e. loses NULL reference and
           gains cache reference)
*/
static AFFObject Resolver_open(Resolver self, RDFURN urn, char mode) {
  AFFObject result = NULL;
  AFFObject classref;
  char *scheme;
  TDB_DATA tdb_urn = tdb_data_from_string(urn->value);

  LOCK_RESOLVER;

  if(!urn) {
    RaiseError(ERuntimeError, "No URN specified");
    goto error;
  };

  DEBUG_OBJECT("Opening %s for mode %c\n", urn->value, mode);

  // Is this object cached?
  if(mode =='r') {
    /* Try to find an available object we can use */
    Object iter = CALL(self->read_cache, iter, TDB_DATA_STRING(tdb_urn));

    while(iter) {
      result = (AFFObject)CALL(self->read_cache, next, &iter);

      // A thread_id of 0 means noone is using it right now
      if(result && result->thread_id == 0) {
        // The object is available and is not used by anyone.
        // we can now take it

        // If its a FileLikeObject seek it to 0
        if(ISSUBCLASS(result, FileLikeObject)) {
          CALL((FileLikeObject)result, seek, 0, SEEK_SET);
        };

        talloc_reference(NULL, result);
        goto exit;
      };
    };
  } else if(mode == 'w') {
    // If the object is write locked, there are two possibilities. If
    // its our own thread that locked it, we are in a deadlock because
    // noone is ever going to unlock it. If another thread locked it,
    // we need to wait on it here.

    // Note that we hold a big mutex over the entire resolver
    // including the write cache itself during this whole function.
    result = (AFFObject)CALL(self->write_cache, borrow, TDB_DATA_STRING(tdb_urn));

    if(result) {
      // The object is currently used by our own thread... this is a
      // deadlock. This is not a race since we are the only thread to
      // be setting the thread_id to our own.
      if(result->thread_id == pthread_self()) {
        RaiseError(ERuntimeError, "DEADLOCK!!! URN %s is already locked (w)", urn->value);
        goto error;

      };

      /* We need to lock the object here, but we are holding a
       recursive lock on the entire cache. If we just took the lock
       here, other threads will be unable to proceed and unlock it. So
       we must release the cache lock here to give them a chance to
       unlock it while we block on acquiring the lock. The matter is
       complicated by the fact that we could be holding a recursive
       lock on the resolver which means even if we release it once,
       other threads may still not get to run.

       The solution is to completely unlock the cache as many times as
       needed here, recording the recursion level, then block on
       acquiring the object lock. Finally we acquire the required
       number of locks on the resolver mutex.
      */
      {
        int count = 0;

        // We dont want it to be freed from right under us
        talloc_reference(self, result);
        while(pthread_mutex_unlock(&self->mutex) != EPERM) count ++;

        // Take the object
        pthread_mutex_lock(&result->mutex);
        result->thread_id = pthread_self();

        // Relock the resolver mutex the required number of times:
        for(;count > 0;count--) pthread_mutex_lock(&self->mutex);
      };

      // Reference it to NULL for giving it to the caller
      talloc_reference(NULL, result);
      talloc_unlink(self, result);

      UNLOCK_RESOLVER;
      return result;
    };
  };

  /** Ok if we get here we did not have it in cache, we must create it */
  ClearError();

  // Here we need to build a new instance of this class using the
  // dispatcher. We first try the scheme:
  scheme = urn->parser->scheme;

  // Empty schemes default to file:// URLs
  if(!scheme || strlen(scheme)==0)
    scheme = "file";

  classref = (AFFObject)CALL(type_dispatcher, borrow, ZSTRING(scheme));
  if(!classref) {
    if(!CALL(self, resolve_value, urn, AFF4_TYPE, (RDFValue)self->type) &&
       !CALL(self, resolve_value, urn, AFF4_VOLATILE_TYPE, (RDFValue)self->type)) {
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
#ifdef __DEBUG__
  talloc_asprintf(result, "Opened %p %s (%s %c) %s\n", result,
                  NAMEOF(result), self->type->value, mode,
                  STRING_URNOF(result));
#endif
  ((AFFObject)result)->mode = mode;

  // Place it in the cache now
  if(mode == 'r') {
    CALL(self->read_cache, put, TDB_DATA_STRING(tdb_urn), (Object)result);
  } else {
    CALL(self->write_cache, put, TDB_DATA_STRING(tdb_urn), (Object)result);
  };

 exit:
  pthread_mutex_lock(&result->mutex);
  result->thread_id = pthread_self();
  DEBUG_LOCK("Locking object %s\n", STRING_URNOF(result));

  UNLOCK_RESOLVER;
  return result;

 error:
  UNLOCK_RESOLVER;
  return NULL;
};

static inline void trim_cache(Cache cache) {
  Cache i,j;

  list_for_each_entry_safe(i,j,&cache->cache_list, cache_list) {
    AFFObject obj = (AFFObject)i->data;

    // If the cache size is not too full its ok
    if(cache->cache_size < RESOLVER_CACHE_SIZE) break;

    // Only remove objects which are not currently locked.
    if(obj->thread_id == 0) {
      talloc_unlink(cache, i);
    };
  };
};


/** Return the object to the cache. Callers may not make a reference
    to it after that. Note that the object is already in the cache
    anyway since it was created through create() or open(). In this
    case all we need to do is to just unlock it.

    We also trim back the cache if needed.
*/
static void Resolver_cache_return(Resolver self, AFFObject obj) {
  Cache cache = self->read_cache;

  LOCK_RESOLVER;

  // Find the object in the cache which is owned by our thread (Note -
  // only one thread may own the same object at the same time). Due to
  // class proxying it may not actually be the same as obj but will
  // have the same URN.
  if(obj->mode == 'r') {
    TDB_DATA tdb_urn = tdb_data_from_string(obj->urn->value);
    Object iter = CALL(cache, iter, TDB_DATA_STRING(tdb_urn));
    long unsigned int thread_id = pthread_self();

    while(iter) {
      AFFObject new_obj = (AFFObject)CALL(cache, next, &iter);

      // Found it - reset its thread_id so others can use it now.
      if(new_obj && new_obj->thread_id == thread_id) {
        obj = new_obj;
        talloc_reference(NULL, obj);

        break;
      };
    };
  } else {
    cache=self->write_cache;

    // Make sure the object is in the relevant cache
    if(!CALL(cache, present, ZSTRING(STRING_URNOF(obj)))) {
      CALL(cache, put, ZSTRING(STRING_URNOF(obj)), (Object)obj);
    };
  }

  // We are done with the object now
  obj->thread_id = 0;
  pthread_mutex_unlock(&obj->mutex);
  Resolver_lock_gen(self, URNOF(obj), obj->mode, F_UNLCK);
  DEBUG_LOCK("Unlocking object %s\n", STRING_URNOF(obj));

  ClearError();

  // Make sure read caches are not too big. Write caches are emptied
  // by the close() method. We never purge the write cache in order to
  // guarantee cached writable objects will always be available.
  trim_cache(self->read_cache);

  // We unlink it from NULL here - the object is owned by NULL after
  // the open call. This should not actually free it because the
  // object is in the cache now.
  talloc_unlink(NULL, obj);

  UNLOCK_RESOLVER;

  return;
};

// A helper method to construct the class
static AFFObject Resolver_create(Resolver self, char *name, char mode) {
  AFFObject class_reference;
  AFFObject result;

  LOCK_RESOLVER;

  class_reference = (AFFObject)CALL(type_dispatcher, borrow, ZSTRING(name));
  if(!class_reference) {
    RaiseError(ERuntimeError, "No handler for object type '%s'", name);
    goto error;
  };

  // Newly created objects are owned by NULL
  result = (AFFObject)CONSTRUCT_FROM_REFERENCE((class_reference),
                                               Con, NULL, NULL, mode);
  if(!result) goto error;

#ifdef __DEBUG__
  talloc_set_name(result, "%s (%c) (created by resolver)", NAMEOF(class_reference), mode);
#endif

  DEBUG_OBJECT("Created %s with URN %s\n", NAMEOF(result), URNOF(result)->value);

  // Lock the object
  result->thread_id = pthread_self();
  pthread_mutex_lock(&result->mutex);
  DEBUG_LOCK("Locking object %s %u\n", STRING_URNOF(result), result->mutex.__data.__count);

  UNLOCK_RESOLVER;
  return result;

 error:
  UNLOCK_RESOLVER;
  return NULL;
};

static void Resolver_del(Resolver self, RDFURN urn, char *attribute_str) {
  TDB_DATA attribute;
  TDB_DATA key;
  char buff[BUFF_SIZE];

  LOCK_RESOLVER;

  if(attribute_str) {
    attribute = tdb_data_from_string(attribute_str);

    key.dptr = (unsigned char *)buff;
    key.dsize = calculate_key(self, tdb_data_from_string(urn->value),
			      attribute, buff, BUFF_SIZE, 0);

    // Remove the key from the database
    tdb_delete(self->data_db, key);
    DEBUG_RESOLVER("Removing attribute %s from %s\n", attribute_str, urn->value);
  } else {
    TDB_DATA max_key;
    int max_id,i;

    DEBUG_RESOLVER("Removing all attributes from %s\n", urn->value);

    max_key = tdb_fetch(self->attribute_db, MAX_KEY);
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

  UNLOCK_RESOLVER;
};

/** Synchronization methods. */
int Resolver_lock_gen(Resolver self, RDFURN urn, char mode, int sense) {
  TDB_DATA attribute;
  uint64_t offset;
  TDB_DATA_LIST data_list;

  // Noop for now
  return 1;

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
    set_new_value(self, NULL, tdb_data_from_string(urn->value),
		  attribute, LOCK, -1, 0, 0, 0);

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

static int Resolver_attributes_iter(Resolver self, RDFURN urn, XSDString attribute,
                                    RESOLVER_ITER *iter) {
  int max_id, attribute_id =0;
  TDB_DATA tdb_urn, tdb_attribute;
  int urn_id;

  // Find the attribute_id
  tdb_attribute = tdb_data_from_string(attribute->value);
  attribute_id = get_id(self->attribute_db, tdb_attribute, 0);

  // Find tha maximum attribute_id
  max_id = get_id(self->attribute_db, MAX_KEY, 0);
  if(attribute_id > max_id) {
    RaiseError(EProgrammingError, "attribute_id > max_id");
    goto error;
  };

  /** Find the urn_id */
  tdb_urn = tdb_data_from_string(urn->value);
  urn_id = get_id(self->urn_db, tdb_urn, 0);
  if(!urn_id) {
    RaiseError(ERuntimeError, "URN %s not known", urn->value);
    goto error;
  };

  while(attribute_id < max_id) {
    char buff[BUFF_SIZE];
    TDB_DATA key, data;

    attribute_id++;

    key.dptr = (unsigned char *)buff;
    key.dsize = calculate_key_from_ids(urn_id, attribute_id, buff, BUFF_SIZE);

    iter->offset = get_data_head_by_key(self, key, &iter->head);
    if(iter->offset > 0) {
      // Update the attribute_name:
      tdb_serialise_int(attribute_id, (char *)key.dptr, BUFF_SIZE);

      data = tdb_fetch(self->attribute_db, key);
      if(!data.dptr){
        RaiseError(EProgrammingError, "attribute_id does not correspond to any attribute?");
        goto error;
      };

      // Set the attribute to the string
      CALL(attribute, set, (char *) data.dptr, data.dsize);
      free(data.dptr);

      // And update the URN
      if(iter->urn) {
        talloc_unlink(iter, urn);
      };
      iter->urn = urn;
      talloc_reference(iter, urn);

      // Reset the iterators key cache
      if(iter->cache) talloc_free(iter->cache);
      iter->cache = CONSTRUCT(Cache, Cache, Con, iter, HASH_TABLE_SIZE, 0);

      return 1;
    };
  };

  error:
    return 0;
};

static int Resolver_get_id_by_urn(Resolver self, RDFURN uri, int create) {
  int result;

  LOCK_RESOLVER;
  result = get_id(self->urn_db, tdb_data_from_string(uri->value), create);

  UNLOCK_RESOLVER;
  return result;
};

static int Resolver_get_urn_by_id(Resolver self, int id, RDFURN uri) {
  TDB_DATA urn_id, result;
  char buff[BUFF_SIZE];

  LOCK_RESOLVER;

  urn_id.dptr = (unsigned char *)buff;
  urn_id.dsize = tdb_serialise_int(id, buff, BUFF_SIZE);

  result = tdb_fetch(self->urn_db, urn_id);
  if(result.dptr) {
    CALL(uri, set, (char *)result.dptr);
    free(result.dptr);
    UNLOCK_RESOLVER;
    return 1;
  };

  UNLOCK_RESOLVER;
  return 0;
};

static void Resolver_register_rdf_value_class(Resolver self, RDFValue class_ref) {
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

  LOCK_RESOLVER;

  list_for_each_entry(i, &type_dispatcher->cache_list, cache_list) {
    if(ISSUBCLASS(i->data, AFF4Volume)) {
      AFF4Volume class_ref = *(AFF4Volume *)i->data;
      AFF4Volume volume = (AFF4Volume)CALL(oracle, create,
                                           ((AFFObject)class_ref)->dataType, 'r');

      if(CALL(volume, load_from, uri, 'r')) {
        CALL(uri, set, STRING_URNOF(volume));
        CALL(oracle, cache_return, (AFFObject)volume);

        ClearError();
        UNLOCK_RESOLVER;
        return 1;
      };

      talloc_free(volume);
    };
  };

  ClearError();
  UNLOCK_RESOLVER;
  return 0;
};

static void Resolver_set_logger(Resolver self, Logger logger) {
  if(self->logger) talloc_free(self->logger);

  self->logger = logger;
  talloc_reference(self, logger);
};

static void Resolver_flush(Resolver self) {
  if(self->read_cache)
    talloc_free(self->read_cache);

  if(self->write_cache)
    talloc_free(self->write_cache);

  self->read_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  talloc_set_name_const(self->read_cache, "Resolver Read Cache");

  self->write_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  talloc_set_name_const(self->write_cache, "Resolver Write Cache");
  //  NAMEOF(self->write_cache) = "Resolver Write Cache";
};

static void Resolver_expire(Resolver self, RDFURN uri) {
  /** Remove the object from the cache */
  Object obj;

  LOCK_RESOLVER;

  // Free the object
  obj = CALL(self->read_cache, get, ZSTRING(uri->value));
  if(obj) {
    talloc_unlink(NULL, obj);
  }

  obj = CALL(self->write_cache, get, ZSTRING(uri->value));
  if(obj) {
    talloc_unlink(NULL, obj);
  };

  // Now ask the object class to delete itself
  if(CALL(self, resolve_value, uri, AFF4_TYPE, (RDFValue)self->type) || \
     CALL(self, resolve_value, uri, AFF4_VOLATILE_TYPE, (RDFValue)self->type)) {
    AFFObject class_reference = (AFFObject)CALL(type_dispatcher,
                                                borrow, ZSTRING(self->type->value));

    if(class_reference) {
      class_reference->delete(uri);
    };
  };

  UNLOCK_RESOLVER;

  ClearError();
};

static void Resolver_close(Resolver self) {
  int mode = self->mode;

  talloc_unlink(NULL, self);

  oracle = CONSTRUCT(Resolver, Resolver, Con, NULL, mode) ;
};

#if HAVE_OPENSSL
extern struct SecurityProvider_t *AFF4_SECURITY_PROVIDER;

static void Resolver_register_security_provider(Resolver self, struct SecurityProvider_t *sec_provider) {
  AFF4_SECURITY_PROVIDER = sec_provider;
  talloc_reference(self, sec_provider);
};
#endif

void Resolver_log(Resolver self, int level, char *service, RDFURN subject, char *message) {
  CALL(self->logger, message, level, service, subject, message);
};

/** Here we implement the resolver */
VIRTUAL(Resolver, Object) {
     VMETHOD(Con) = Resolver_Con;
     VMETHOD(create) = Resolver_create;

     VMETHOD(resolve_value) = Resolver_resolve_value;
     VMETHOD(resolve_alloc) = Resolver_resolve_alloc;
     VMETHOD(get_iter) = Resolver_get_iter;
     VMETHOD(iter_next) = Resolver_iter_next;
     VMETHOD(encoded_data_from_iter) = Resolver_encoded_data_from_iter;
     VMETHOD(alloc_from_iter) = Resolver_alloc_from_iter;
     VMETHOD(attributes_iter) = Resolver_attributes_iter;

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
     VMETHOD(register_type_dispatcher) = Resolver_register_type_dispatcher;

#if HAVE_OPENSSL
     VMETHOD(register_security_provider) = Resolver_register_security_provider;
#endif

     VMETHOD(load) = Resolver_load;
     VMETHOD(set_logger) = Resolver_set_logger;
     VMETHOD(log) = Resolver_log;
     VMETHOD(flush) = Resolver_flush;
     VMETHOD(close) = Resolver_close;
} END_VIRTUAL

/************************************************************
  AFFObject - This is the base class for all other objects
************************************************************/
static AFFObject AFFObject_Con(AFFObject self, RDFURN uri, char mode) {
  uuid_t uuid;
  char uuid_str[BUFF_SIZE];

  // We have no valid URI - this is usually the first pass through
  // this function.
  if(!uri) {
    // This function creates a new stream from scratch so the stream
    // name will be a new UUID
    strcpy(uuid_str, FQN);
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str + strlen(FQN));

    self->urn = new_RDFURN(self);
    self->urn->set(self->urn, uuid_str);

    // The mutex controls access to the object
    {
      pthread_mutexattr_t mutex_attr;

      pthread_mutexattr_init(&mutex_attr);
      pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
      pthread_mutex_init(&self->mutex, &mutex_attr);
      pthread_mutexattr_destroy(&mutex_attr);
    };

    // We already have a valid URL - this is the second pass through
    // the function.
  } else {
    if(self->urn != uri)
      self->urn = CALL(uri, copy, self);

#if 0
    // Ok we successfully created the object - add it to the required
    // cached now:
    if(self->mode == 'w')
      cache = oracle->write_cache;

    // Put us in the right cache
    if(!CALL(cache, present, ZSTRING(STRING_URNOF(self)))) {
      CALL(cache, put, ZSTRING(STRING_URNOF(self)), (Object)self);
    };
#endif

    self->complete = 1;
  };

  self->mode = mode;

  return self;
};


static void AFFObject_set_property(AFFObject self, char *attribute, RDFValue value) {
  CALL(oracle, set_value,
       self->urn,
       attribute, value,0);
};

static void AFFObject_add(AFFObject self, char *attribute, RDFValue value) {
  CALL(oracle, add_value,
       self->urn,
       attribute, value,0);
};

// Prepares an object to be used at this point we add the object to
// the relevant cache
static AFFObject AFFObject_finish(AFFObject self) {
  Cache cache = oracle->read_cache;
  AFFObject result =CALL(self, Con, URNOF(self), self->mode);

  // Finishing failed
  if(!result) return NULL;

  // Ok we successfully created the object - add it to the required
  // cached now:
  if(self->mode == 'w')
    cache = oracle->write_cache;

  // Put us in the right cache
  CALL(cache, put, ZSTRING(STRING_URNOF(result)), (Object)result);

  self->complete = 1;

  return result;
};

static void AFFObject_delete(RDFURN urn) {
  CALL(oracle, del, urn, NULL);
};

static void AFFObject_cache_return(AFFObject self) {
  CALL(oracle, cache_return, self);
};

static int AFFObject_close(AFFObject self) {
  Cache cache = oracle->read_cache;
  Object o;

  if(self->mode == 'w')
    cache = oracle->write_cache;

  o = CALL(cache, get, ZSTRING(STRING_URNOF(self)));
  if(o) talloc_unlink(NULL, o);
  ClearError();

  //  talloc_unlink(NULL, self);
  return 1;
};

VIRTUAL(AFFObject, Object) {
     VMETHOD(finish) = AFFObject_finish;
     VMETHOD(set) = AFFObject_set_property;
     VMETHOD(add) = AFFObject_add;
     VMETHOD(delete) = AFFObject_delete;
     VMETHOD(cache_return) = AFFObject_cache_return;
     VMETHOD(close) = AFFObject_close;

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

static void Logger_message(Logger self, int level, char *service, RDFURN subject, char *message) {
  printf("(%d)%s: %s %s\n", level, service, subject->value, message);
};

VIRTUAL(Logger, Object) {
  VMETHOD(Con) = Logger_Con;
  VMETHOD(message) = Logger_message;
} END_VIRTUAL


  /** A wrapper around TDB */
static int TDB_destructor(void *ptr) {
  TDB self = (TDB)ptr;

  if(self->file) {
    tdb_close(self->file);
    self->file = 0;
  };

  return 1;
};

static TDB TDB_Con(TDB self, char *filename, int mode) {
  if(mode & RESOLVER_MODE_NONPERSISTANT)
    AFF4_TDB_FLAGS |= TDB_CLEAR_IF_FIRST;

  if(mode & RESOLVER_MODE_DEBUG_MEMORY)
    talloc_enable_leak_report_full();

  self->file = tdb_open(filename, TDB_HASH_SIZE,
                        AFF4_TDB_FLAGS,
                        O_RDWR | O_CREAT, 0644);

  if (self->file) {
    talloc_set_destructor((void*)self, TDB_destructor);
    return self;
  };

  RaiseError(ERuntimeError, "Unable to open tdb file on %s", filename);
  talloc_free(self);
  return NULL;
};

static void TDB_store(TDB self, char *key,int key_len, char *data, int len) {
  TDB_DATA tdb_key, value;

  tdb_key.dptr = (unsigned char *)key;
  tdb_key.dsize = key_len;
  value.dptr = (unsigned char *)data;
  value.dsize = len;

  tdb_store(self->file, tdb_key, value, TDB_REPLACE);
};

static TDB_DATA TDB_fetch(TDB self, char *key, int len) {
  TDB_DATA tdb_key;

  tdb_key.dptr = (unsigned char *)key;
  tdb_key.dsize = len;

  return tdb_fetch(self->file, tdb_key);
};

VIRTUAL(TDB, Object) {
  VMETHOD(Con) = TDB_Con;
  VMETHOD(store) = TDB_store;
  VMETHOD(fetch) = TDB_fetch;
}  END_VIRTUAL;

Resolver get_oracle(void) {
  return oracle;
};

// Note that we preceed the init function name with an altitude to
// control the order at which these will be called.
AFF4_MODULE_INIT(A100_resolver) {
  // Make a global oracle
  if(!oracle) {
    // Create the global oracle
    oracle =CONSTRUCT(Resolver, Resolver, Con, NULL, 0);
  };
};

DLL_PUBLIC void aff4_free(void *ptr) {
  talloc_unlink(NULL, ptr);
};

DLL_PUBLIC void aff4_incref(void *ptr) {
  talloc_reference(NULL, ptr);
};
