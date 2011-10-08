/** This file implemnts the resolver */
#include "aff4_internal.h"

extern Cache RDF_Registry;

/* Special variants which also take the graph name.

   These should not be called externally.
*/
PRIVATE int Graph_add_value(RDFURN graph, RDFURN urn, char *attribute_str,
                            RDFValue value);

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

/** This registers new AFFObject implementations. */
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
  };
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
static int Cache_hash(Cache self, char *key, int len) {
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

static int print_cache_urns(Cache self) {
  Cache i;

  list_for_each_entry(i, &self->cache_list, cache_list) {
    printf("%s %p %s\n",(char *) i->key,i , ((RDFURN)(i->data))->value);
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
      talloc_free(i);
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
    talloc_steal(new_cache, data);

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

static Object Cache_get(Cache self, void *ctx, char *key, int len) {
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

      // When we return the object we steal it to null.
      if(result) {
        talloc_steal(ctx, result);
      };

      // Now free the container - this will unlink it from the lists through its
      // destructor.
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



/***********************************************************
   This is the implementation of the resolver.
***********************************************************/
static int Resolver_destructor(void *this) {
  Resolver self = (Resolver) this;

  pthread_mutex_destroy(&self->mutex);
  return 0;
};


static Resolver Resolver_Con(Resolver self, DataStore store, int mode) {
  pthread_mutexattr_t mutex_attr;

  self->logger = CONSTRUCT(Logger, Logger, Con, self);

  if(!store) {
    store = new_MemoryDataStore(self);
  };

  talloc_steal(self, store);

  self->store = store;

  if(mode & RESOLVER_MODE_DEBUG_MEMORY)
    talloc_enable_leak_report_full();

  // This is recursive to ensure that only one thread can hold the
  // resolver at once. Once the thread holds the resolver its free to
  // continue calling functions from it.
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&self->mutex, &mutex_attr);
  pthread_mutexattr_destroy(&mutex_attr);

  // Create local read and write caches
  CALL(self, flush);

  // Make sure we close our files when we get freed
  talloc_set_destructor((void *)self, Resolver_destructor);

  // Cache this so we dont need to rebuilt it all the time.
  self->type = new_RDFURN(self);

  return self;
};


/** This sets a single triple into the resolver replacing previous
    values set for this attribute
*/
static int Resolver_set(Resolver self, RDFURN urn, char *attribute_str,
                        RDFValue value) {
  DataStoreObject obj;

  LOCK_RESOLVER;

  obj = CALL(value, encode, urn, self);

  // The DataStore will steal the object.
  CALL(self->store, set, urn->value, attribute_str, obj);

  UNLOCK_RESOLVER;
  return 1;
};

/** This removes all values for this attribute
 */
static void Resolver_del(Resolver self, RDFURN urn, char *attribute_str) {
  LOCK_RESOLVER;

  CALL(self->store, del, urn->value, attribute_str);

  UNLOCK_RESOLVER;
};

/** This sets a single triple into the resolver replacing previous
    values set for this attribute
*/
static int Resolver_add(Resolver self, RDFURN urn, char *attribute_str,
                        RDFValue value) {
  DataStoreObject obj;

  LOCK_RESOLVER;
  CALL(self->store, unlock);

  obj = CALL(value, encode, urn, self);

  // The DataStore will steal the object.
  CALL(self->store, add, urn->value, attribute_str, obj);

  UNLOCK_RESOLVER;
  return 1;
};


/* Allocate and return all the RDFValues which match the urn and attribute.
 */
static RDFValue Resolver_resolve(Resolver self, void *ctx, RDFURN urn, char *attribute) {
  Object iter;
  RDFValue result = NULL;

  LOCK_RESOLVER;
  CALL(self->store, lock);

  iter = CALL(self->store, iter, urn->value, attribute);

  while(iter) {
    DataStoreObject obj = CALL(self->store, next, &iter);
    RDFValue rdf_value_class = (RDFValue)CALL(RDF_Registry, borrow, ZSTRING(obj->rdf_type));
    RDFValue item = NULL;

    if(rdf_value_class) {
      item = (RDFValue)CONSTRUCT_FROM_REFERENCE(rdf_value_class, Con, ctx);
    };

    // Decode it.
    CALL(item, decode, obj, urn, self);

    // Add to the list
    if(result) {
      list_add_tail(&result->list, &item->list);
    } else {
      result = item;
    };
  };

  CALL(self->store, unlock);
  UNLOCK_RESOLVER;
  return result;
};


static AFFObject create_new_object(Resolver self, RDFURN urn, char *type, char mode) {
  AFFObject result = NULL;
  AFFObject classref = NULL;

  // Find the correct class to handle this type. First try the scheme and then
  // the type.
  if(urn) {
    // Here we need to build a new instance of this class using the
    // dispatcher. We first try the scheme:
    char *scheme = urn->parser->scheme;

    // Empty schemes default to file:// URLs
    if(!scheme || strlen(scheme)==0)
      scheme = "file";

    classref = (AFFObject)CALL(type_dispatcher, borrow, ZSTRING(scheme));
  };

  if(!classref) {
    classref = (AFFObject)CALL(type_dispatcher, borrow, ZSTRING(type));
  };

  if(!classref) {
    RaiseError(ERuntimeError, "Unable to open urn - this implementation "
               "can not open objects of type %s", type);
    goto error;
  };

  // Make a new instance of this class (which we own)
  result = CONSTRUCT_FROM_REFERENCE(classref, Con, self, urn, mode, self);
  if(!result) goto error;

  talloc_set_name_const(result, NAMEOF(result));
  ((AFFObject)result)->mode = mode;
  return result;

error:
  return NULL;
};


/* Instantiates a single instance of the class using this resolver. The returned
 * object MUST be returned to the cache using cache_return(). The object is
 * owned by the resolver for the entire time users use it - users must not free
 * it. While it is in use the object is locked to the calling thread.

 * In read mode, the object is created from scratch, and destroyed when
 * returned. This means there could be multiple copies of the same object in
 * simultaneous use. Its ok for clients to hold for an indefinite time.

 * In write mode only a single instance of the object may exist. The object is
 * locked between this call and the cache_return() call. Writer threads are
 * blocked waiting for it.
 */
static AFFObject Resolver_open(Resolver self, RDFURN urn, char mode) {
  AFFObject result = NULL;
  DataStoreObject type_obj;

  LOCK_RESOLVER;
  ClearError();

  if(!urn) {
    RaiseError(ERuntimeError, "No URN specified");
    goto exit;
  };

  DEBUG_OBJECT("Opening %s for mode %c\n", urn->value, mode);

  // Object must already exist.
  CALL(self->store, lock);
  type_obj = CALL(self->store, get, urn->value, AFF4_TYPE);
  CALL(self->store, unlock);

  if(!type_obj) {
    RaiseError(ERuntimeError, "Object does not exist.");
    goto exit;
  };

  if(mode == 'r') {
    result = create_new_object(self, urn, type_obj->data, mode);

    goto exit;
  } else if(mode =='w') {
    /* Check that the object is not already in the write cache */
    result = (AFFObject)CALL(self->write_cache, borrow, ZSTRING(urn->value));
    if(result) {
      // Is it already locked by us?
      if(result->thread_id == pthread_self()) {
        RaiseError(ERuntimeError, "DEADLOCK!!! URN %s is already locked (w). "
                   " Chances are you did not call cache_return() properly.", urn->value);
        goto exit;
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
        talloc_steal(self, result);

        // Unlock the resolver.
        while(pthread_mutex_unlock(&self->mutex) != EPERM) count ++;

        // Take the object
        pthread_mutex_lock(&result->mutex);
        result->thread_id = pthread_self();

        // Relock the resolver mutex the required number of times:
        for(;count > 0;count--) pthread_mutex_lock(&self->mutex);
      };

      goto exit;
    } else {
      // Need to make a new object
      result = create_new_object(self, urn, type_obj->data, mode);

      // Take the object
      pthread_mutex_lock(&result->mutex);
      result->thread_id = pthread_self();

      goto exit;
    };
  };

exit:
  UNLOCK_RESOLVER;
  return result;
};


static inline void trim_cache(Cache cache) {
  Cache i,j;

  list_for_each_entry_safe(i,j,&cache->cache_list, cache_list) {
    AFFObject obj = (AFFObject)i->data;

    // If the cache size is not too full its ok
    if(cache->cache_size < RESOLVER_CACHE_SIZE) break;

    // Only remove objects which are not currently locked.
    if(obj->thread_id == 0) {
      talloc_free(i);
    };
  };
};


/** Return the object to the cache. Callers may not make a reference
    to it after that. We also trim back the cache if needed.
*/
static void Resolver_cache_return(Resolver self, AFFObject obj) {
  LOCK_RESOLVER;
  Cache cache = self->read_cache;

  /* In read mode we just free the object. */
  if(obj->mode == 'w') {
    cache = self->write_cache;
  };

  // Return it back to the cache.
  CALL(cache, put, ZSTRING(obj->urn->value), obj);

  // We are done with the object now
  obj->thread_id = 0;
  pthread_mutex_unlock(&obj->mutex);

  DEBUG_LOCK("Unlocking object %s\n", STRING_URNOF(obj));

  ClearError();

  UNLOCK_RESOLVER;
  return;
};

// A helper method to construct the class.
static AFFObject Resolver_create(Resolver self, RDFURN urn, char *type, char mode) {
  LOCK_RESOLVER;

  AFFObject result = create_new_object(self, urn, type, mode);
  // Failed to create a new object
  if(result) {
    DEBUG_OBJECT("Created %s with URN %s\n", type, result->urn->value);
  };

  UNLOCK_RESOLVER;
  return result;
};


static int remove_object_from_cache(void *ctx) {
  AFFObject obj = (AFFObject)ctx;
  Cache cache = obj->resolver->read_cache;

  if(obj->mode == 'w')
    cache = obj->resolver->write_cache;

  /* Remove us from the cache */
  CALL(cache, get, NULL, ZSTRING(obj->urn->value));

  return 1;
};


static void Resolver_manage(Resolver self, AFFObject obj) {
  Cache cache = self->read_cache;
  LOCK_RESOLVER;

  if(obj->mode == 'w')
    cache = self->write_cache;

  if(obj->urn) {
    // If we are writing, place the result on the cache.
    CALL(cache, put, ZSTRING(obj->urn->value), (Object)obj);

    // And lock the mutex
    pthread_mutex_lock(&obj->mutex);
    obj->thread_id = pthread_self();
  };

  UNLOCK_RESOLVER;
};

static AFFObject Resolver_own(Resolver self, RDFURN urn, char mode) {
  /* Check that the object is not already in the write cache */
  AFFObject result;
  Cache cache = self->read_cache;

  LOCK_RESOLVER;

  if(mode == 'w')
    cache = self->write_cache;

  result = (AFFObject)CALL(cache, borrow, ZSTRING(urn->value));
  if(result) {
    // Is it already locked by us?
    if(result->thread_id == pthread_self()) {
      RaiseError(ERuntimeError, "DEADLOCK!!! URN %s is already locked (w). "
                 " Chances are you did not call cache_return() properly.", urn->value);
      goto exit;
    };

    /* We need to lock the object here, but we are holding a recursive lock on
       the entire cache. If we just took the lock here, other threads will be
       unable to proceed and unlock it. So we must release the cache lock here
       to give them a chance to unlock it while we block on acquiring the
       lock. The matter is complicated by the fact that we could be holding a
       recursive lock on the resolver which means even if we release it once,
       other threads may still not get to run.

       The solution is to completely unlock the cache as many times as
       needed here, recording the recursion level, then block on
       acquiring the object lock. Finally we acquire the required
       number of locks on the resolver mutex.
    */
    {
      int count = 0;

      // We dont want it to be freed from right under us
      talloc_steal(self, result);

      // Unlock the resolver.
      while(pthread_mutex_unlock(&self->mutex) != EPERM) count ++;

      // Take the object
      pthread_mutex_lock(&result->mutex);
      result->thread_id = pthread_self();

      // Relock the resolver mutex the required number of times:
      for(;count > 0;count--) pthread_mutex_lock(&self->mutex);
    };
  };

exit:
  UNLOCK_RESOLVER;
  return result;
};


static void Resolver_register_rdf_value_class(Resolver self, RDFValue class_ref) {
  register_rdf_value_class(class_ref);
  talloc_increase_ref_count(class_ref);
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

#if 0
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
#endif

  ClearError();
  UNLOCK_RESOLVER;
  return 0;
};

static void Resolver_set_logger(Resolver self, Logger logger) {
  if(self->logger) talloc_free(self->logger);

  if(logger) {
    self->logger = logger;
    talloc_reference(self, logger);
  } else {
    self->logger = CONSTRUCT(Logger, Logger, Con, self);
  };
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
     VMETHOD(own) = Resolver_own;
     VMETHOD(manage) = Resolver_manage;
     VMETHOD(cache_return) = Resolver_cache_return;

     VMETHOD(resolve) = Resolver_resolve;
     VMETHOD(open) = Resolver_open;
     VMETHOD(set) = Resolver_set;
     VMETHOD(add) = Resolver_add;
     VMETHOD(del) = Resolver_del;

     VMETHOD(register_rdf_value_class) = Resolver_register_rdf_value_class;
     VMETHOD(new_rdfvalue) = Resolver_new_rdfvalue;

#if HAVE_OPENSSL
     VMETHOD(register_security_provider) = Resolver_register_security_provider;
#endif

     VMETHOD(load) = Resolver_load;
     VMETHOD(register_logger) = Resolver_set_logger;
     VMETHOD(log) = Resolver_log;
     VMETHOD(flush) = Resolver_flush;
} END_VIRTUAL

/************************************************************
  AFFObject - This is the base class for all other objects
************************************************************/
static AFFObject AFFObject_Con(AFFObject self, RDFURN uri, char mode, Resolver resolver) {
  self->resolver = resolver;

  if(uri) {
    self->urn = CALL((RDFValue)uri, clone, self);
  } else {
    self->urn = new_RDFURN(self);
  };

  // The mutex controls access to the object
  {
    pthread_mutexattr_t mutex_attr;

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&self->mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);
  };

  self->mode = mode;

  return self;
};


static void AFFObject_set(AFFObject self, char *attribute, RDFValue value) {
  CALL(self->resolver, set, self->urn, attribute, value);
};

static void AFFObject_add(AFFObject self, char *attribute, RDFValue value) {
  CALL(self->resolver, add, self->urn, attribute, value);
};

static int AFFObject_finish(AFFObject self) {
  uuid_t uuid;
  char uuid_str[BUFF_SIZE];

  if(!self->urn->parser->scheme) {
    // URN not set - we create an annonymous urn
    strcpy(uuid_str, FQN);
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str + strlen(FQN));
    CALL(self->urn, set, uuid_str);
  };

  self->complete = 1;

  return 1;
};

static int AFFObject_close(AFFObject self) {
  Cache cache = self->resolver->read_cache;

  if(self->mode == 'w')
    cache = self->resolver->write_cache;

  /* Remove us from the cache */
  CALL(cache, get, NULL, ZSTRING(self->urn->value));

  return 1;
};

VIRTUAL(AFFObject, Object) {
     VMETHOD(finish) = AFFObject_finish;
     VMETHOD(set) = AFFObject_set;
     VMETHOD(add) = AFFObject_add;
     VMETHOD(close) = AFFObject_close;

     VMETHOD(Con) = AFFObject_Con;
} END_VIRTUAL


#if 0
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
#endif

static Logger Logger_Con(Logger self) {
  return self;
};

static void Logger_message(Logger self, int level, char *service, RDFURN subject, char *message) {
  char *s;

  if(subject) s=subject->value;
  else s="";

  printf("(%d)%s: %s %s\n", level, service, s, message);
};

VIRTUAL(Logger, Object) {
  VMETHOD(Con) = Logger_Con;
  VMETHOD(message) = Logger_message;
} END_VIRTUAL


// Note that we preceed the init function name with an altitude to
// control the order at which these will be called.
AFF4_MODULE_INIT(A100_resolver) {
  // Create a global logger.
  AFF4_LOGGER = CONSTRUCT(Logger, Logger, Con, NULL);
};

DLL_PUBLIC void aff4_free(void *ptr) {
  if(ptr)
    talloc_unlink(NULL, ptr);
};

DLL_PUBLIC void aff4_incref(void *ptr) {
  talloc_reference(NULL, ptr);
};

DLL_PUBLIC Resolver AFF4_get_resolver(DataStore store, void *ctx) {
  if(!store)
    store = new_MemoryDataStore(ctx);

  return CONSTRUCT(Resolver, Resolver, Con, ctx, store, 0);
};

extern Cache KeyCache;

DLL_PUBLIC void aff4_end() {
  int *current_error;
  char *buff = NULL;

  talloc_free(RDF_Registry);
  talloc_free(type_dispatcher);
  talloc_free(AFF4_SECURITY_PROVIDER);
  raptor_finish();

  current_error = aff4_get_current_error(&buff);
  if(current_error) {
    talloc_free(current_error);
  };

  if(buff) talloc_free(buff);
};

DLL_PUBLIC void print_error_message() {
  PrintError();
};

Logger AFF4_LOGGER = NULL;
