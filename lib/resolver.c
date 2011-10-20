/** This file implemnts the resolver */
#include "aff4_internal.h"
#include "aff4_utils.h"

extern Cache RDF_Registry;

/* Special variants which also take the graph name.

   These should not be called externally.
*/
PRIVATE int Graph_add_value(RDFURN graph, RDFURN urn, char *attribute_str,
                            RDFValue value);

#define RESOLVER_CACHE_SIZE 20

/* Thread control within the AFF4 library:

   In order to ensure the AFF4 library is thread safe, there is a
   global lock mutex to protect all aff4 library calls. A similar
   strategy is used by the python interpreter.

   - All user code outsize the library is allowed to run with the
     global lock released.

   - Upon entry to every AFF4 function, the global lock is acquired,
     and release on exit.

   - When operations are performed in the AFF4 core which can release
     the lock (e.g. system calls or compression opeations), the global
     lock is released. This allows other threads a chance to run.

   - All new threads must acquire the lock before starting and release
     it when finishing.

   This strategy ensures that there is only a single thread at a time
   which can touch any aff4 data structures. This is done to protect
   talloc contexts from data races.
 */
pthread_mutex_t aff4_gl_mutex;
pthread_mutex_t aff4_gl_current_mutex;

// This is a big dispatcher of all AFFObjects we know about. We call
// their AFFObjects::Con(urn, mode) constructor.
static Cache type_dispatcher = NULL;

/** This registers new AFFObject implementations. */
void register_type_dispatcher(char *type, AFFObject *classref) {
  AFF4_GL_LOCK;

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

  AFF4_GL_UNLOCK;
};


/**********************************************************
   The following are utilities that will be needed later
***********************************************************/
#define VOLATILE_NS "aff4volatile:"



static Resolver Resolver_Con(Resolver self, DataStore store, int mode) {
  AFF4_GL_LOCK;

  self->logger = CONSTRUCT(Logger, Logger, Con, self);

  if(!store) {
    store = new_MemoryDataStore(self);
  };

  talloc_steal(self, store);

  self->store = store;

  if(mode & RESOLVER_MODE_DEBUG_MEMORY)
    talloc_enable_leak_report_full();

  // Create local read and write caches
  CALL(self, flush);

  // Cache this so we dont need to rebuilt it all the time.
  self->type = new_RDFURN(self);

  AFF4_GL_UNLOCK;

  return self;
};


/** This sets a single triple into the resolver replacing previous
    values set for this attribute
*/
static int Resolver_set(Resolver self, RDFURN urn, char *attribute_str,
                        RDFValue value) {
  DataStoreObject obj;

  AFF4_GL_LOCK;

  obj = CALL(value, encode, urn, self);

  // The DataStore will steal the object.
  CALL(self->store, set, urn->value, attribute_str, obj);

  AFF4_GL_UNLOCK;
  return 1;
};

/** This removes all values for this attribute
 */
static void Resolver_del(Resolver self, RDFURN urn, char *attribute_str) {
  AFF4_GL_LOCK;

  CALL(self->store, del, urn->value, attribute_str);

  AFF4_GL_UNLOCK;
};

/** This sets a single triple into the resolver replacing previous
    values set for this attribute
*/
static int Resolver_add(Resolver self, RDFURN urn, char *attribute_str,
                        RDFValue value) {
  DataStoreObject obj;

  AFF4_GL_LOCK;
  CALL(self->store, unlock);

  obj = CALL(value, encode, urn, self);

  // The DataStore will steal the object.
  CALL(self->store, add, urn->value, attribute_str, obj);

  AFF4_GL_UNLOCK;
  return 1;
};


/* Allocate and return all the RDFValues which match the urn and attribute.
 */
static RDFValue Resolver_resolve(Resolver self, void *ctx, RDFURN urn, char *attribute) {
  Object iter;
  RDFValue result = NULL;

  AFF4_GL_LOCK;
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
  AFF4_GL_UNLOCK;
  return result;
};


static AFFObject create_new_object(Resolver self, RDFURN urn, char *type, char mode) {
  AFFObject result = NULL;
  AFFObject classref = NULL;

  AFF4_GL_LOCK;

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

  AFF4_GL_UNLOCK;
  return result;

error:
  AFF4_GL_UNLOCK;
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

  AFF4_GL_LOCK;
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
      goto exit;
    } else {
      // Need to make a new object
      result = create_new_object(self, urn, type_obj->data, mode);

      goto exit;
    };
  };

exit:
  AFF4_GL_UNLOCK;
  return result;
};


/** Return the object to the cache. Callers may not make a reference
    to it after that. We also trim back the cache if needed.
*/
static void Resolver_cache_return(Resolver self, AFFObject obj) {
  AFF4_GL_LOCK;
  Cache cache = self->read_cache;

  if(obj->mode == 'w') {
    cache = self->write_cache;
  };

  // Return it back to the cache.
  CALL(cache, put, ZSTRING(obj->urn->value), (Object)obj);

  ClearError();

  AFF4_GL_UNLOCK;
  return;
};

// A helper method to construct the class.
static AFFObject Resolver_create(Resolver self, RDFURN urn, char *type, char mode) {
  AFF4_GL_LOCK;

  AFFObject result = create_new_object(self, urn, type, mode);
  // Failed to create a new object
  if(result) {
    DEBUG_OBJECT("Created %s with URN %s\n", type, result->urn->value);
  };

  AFF4_GL_UNLOCK;
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
  AFF4_GL_LOCK;

  if(obj->mode == 'w')
    cache = self->write_cache;

  if(obj->urn) {
    // If we are writing, place the result on the cache.
    CALL(cache, put, ZSTRING(obj->urn->value), (Object)obj);
  };

  AFF4_GL_UNLOCK;
};

static AFFObject Resolver_own(Resolver self, RDFURN urn, char mode) {
  /* Check that the object is not already in the write cache */
  AFFObject result;
  Cache cache = self->read_cache;

  AFF4_GL_LOCK;

  if(mode == 'w')
    cache = self->write_cache;

  result = (AFFObject)CALL(cache, borrow, ZSTRING(urn->value));

exit:
  AFF4_GL_UNLOCK;
  return result;
};


static void Resolver_register_rdf_value_class(Resolver self, RDFValue class_ref) {
  AFF4_GL_LOCK;

  register_rdf_value_class(class_ref);
  talloc_increase_ref_count(class_ref);

  AFF4_GL_UNLOCK;
};

RDFValue Resolver_new_rdfvalue(Resolver self, void *ctx, char *type) {
  RDFValue result;
  AFF4_GL_LOCK;

  result = new_rdfvalue(ctx, type);

  AFF4_GL_UNLOCK;
  return result;
};

int Resolver_load(Resolver self, RDFURN uri) {
  /** We iterate over all the type handlers which are derived from the
      AFF4Volume abstract class, and try to instantiate them in their
      preferred order.
  */
  AFF4_GL_LOCK;

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
        AFF4_GL_UNLOCK;
        return 1;
      };

      talloc_free(volume);
    };
  };
#endif

  ClearError();
  AFF4_GL_UNLOCK;
  return 0;
};

static void Resolver_set_logger(Resolver self, Logger logger) {
  AFF4_GL_LOCK;

  if(self->logger) talloc_free(self->logger);

  if(logger) {
    self->logger = logger;
    talloc_reference(self, logger);
  } else {
    self->logger = CONSTRUCT(Logger, Logger, Con, self);
  };

  AFF4_GL_UNLOCK;
};

static void Resolver_flush(Resolver self) {
  AFF4_GL_LOCK;

  if(self->read_cache)
    talloc_free(self->read_cache);

  if(self->write_cache)
    talloc_free(self->write_cache);

  self->read_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  talloc_set_name_const(self->read_cache, "Resolver Read Cache");

  self->write_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  talloc_set_name_const(self->write_cache, "Resolver Write Cache");
  //  NAMEOF(self->write_cache) = "Resolver Write Cache";
  AFF4_GL_UNLOCK;
};


#if HAVE_OPENSSL
extern struct SecurityProvider_t *AFF4_SECURITY_PROVIDER;

static void Resolver_register_security_provider(Resolver self, struct SecurityProvider_t *sec_provider) {
  AFF4_GL_LOCK;

  AFF4_SECURITY_PROVIDER = sec_provider;
  talloc_reference(self, sec_provider);

  AFF4_GL_UNLOCK;
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
  AFF4_GL_LOCK;

  self->resolver = resolver;

  if(uri) {
    self->urn = CALL(uri, copy, self);
  } else {
    self->urn = (RDFURN)new_RDFURN(self);
  };

  self->mode = mode;

  AFF4_GL_UNLOCK;
  return self;
};


static void AFFObject_set(AFFObject self, char *attribute, RDFValue value) {
  AFF4_GL_LOCK;

  CALL(self->resolver, set, self->urn, attribute, value);

  AFF4_GL_UNLOCK;
};

static void AFFObject_add(AFFObject self, char *attribute, RDFValue value) {
  AFF4_GL_LOCK;

  CALL(self->resolver, add, self->urn, attribute, value);

  AFF4_GL_UNLOCK;
};

static int AFFObject_finish(AFFObject self) {
  uuid_t uuid;
  char uuid_str[BUFF_SIZE];

  AFF4_GL_LOCK;

  if(!self->urn->parser->scheme) {
    // URN not set - we create an annonymous urn
    strcpy(uuid_str, FQN);
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str + strlen(FQN));
    CALL(self->urn, set, uuid_str);
  };

  self->complete = 1;

  AFF4_GL_UNLOCK;
  return 1;
};

static int AFFObject_close(AFFObject self) {
  Cache cache;

  AFF4_GL_LOCK;

  cache = self->resolver->read_cache;
  if(self->mode == 'w')
    cache = self->resolver->write_cache;

  /* Remove us from the cache */
  CALL(cache, get, NULL, ZSTRING(self->urn->value));

  AFF4_GL_UNLOCK;
  return 1;
};

VIRTUAL(AFFObject, Object) {
     VMETHOD(finish) = AFFObject_finish;
     VMETHOD(set) = AFFObject_set;
     VMETHOD(add) = AFFObject_add;
     VMETHOD(close) = AFFObject_close;

     VMETHOD(Con) = AFFObject_Con;
} END_VIRTUAL


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


/* The global lock object. */
AFF4GlobalLock aff4_gl_lock;


// Note that we preceed the init function name with an altitude to
// control the order at which these will be called.
AFF4_MODULE_INIT(A0000_resolver) {
  aff4_gl_lock = CONSTRUCT(AFF4GlobalLock, AFF4GlobalLock, Con, NULL);

  // Create a global logger.
  AFF4_GL_LOCK;
  AFF4_LOGGER = CONSTRUCT(Logger, Logger, Con, NULL);
  AFF4_GL_UNLOCK;
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
