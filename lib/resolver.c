/** This file implemnts the resolver */
#include "zip.h"
#include <uuid/uuid.h>
#include <pthread.h>
#include <tdb.h>
#include <raptor.h>
#include <errno.h>

// This is a big dispatcher of all AFFObjects we know about. We call
// their AFFObjects::Con(urn, mode) constructor.
struct dispatch_t dispatch[] = {
  // Must be first - fallback position
  { 1, "file://",                 (AFFObject)&__FileBackedObject },

  { 0, AFF4_SEGMENT,              (AFFObject)&__ZipFileStream },
  { 0, AFF4_LINK,                 (AFFObject)&__Link },
  { 0, AFF4_IMAGE,                (AFFObject)&__Image },
  { 0, AFF4_MAP,                  (AFFObject)&__MapDriver},
  { 0, AFF4_ENCRYTED,             (AFFObject)&__Encrypted},
  { 0, AFF4_IDENTITY,             (AFFObject)&__Identity},
  { 0, AFF4_ZIP_VOLUME,           (AFFObject)&__ZipFile },
  { 0, AFF4_DIRECTORY_VOLUME,     (AFFObject)&__DirVolume },

  // All handled by libcurl
#ifdef HAVE_LIBCURL
  { 1, "http://",                 (AFFObject)&__HTTPObject },
  { 1, "https://",                (AFFObject)&__HTTPObject },
  { 1, "ftp://",                  (AFFObject)&__HTTPObject },
#endif

#ifdef HAVE_LIBAFFLIB
  { 0, AFF4_LIBAFF_VOLUME,        (AFFObject)GETCLASS(AFF1Volume) },
  { 0, AFF4_LIBAFF_STREAM,        (AFFObject)GETCLASS(AFF1Stream) },
#endif

  { 0, NULL, NULL}
};

// This dispatcher is for the volume handlers - we attempt to open
// each volume with one of these handlers. Note that we actually call
// their ZipFile::Con(fileurn, mode) constructor (since they all
// inherit from ZipFile).
struct dispatch_t volume_handlers[] = {
  { 0, AFF4_ZIP_VOLUME,           (AFFObject)&__ZipFile },
  { 0, AFF4_DIRECTORY_VOLUME,     (AFFObject)&__DirVolume },
  // This is legacy support for AFF1 volumes.
#ifdef HAVE_LIBAFFLIB
  { 0, AFF4_LIBAFF_VOLUME,        (AFFObject)&__AFF1Volume },
#endif

#if 0 // Not implemented yet
#ifdef HAVE_LIBEWF
  { 0, AFF4_LIBEWF_VOLUME,        (AFFObject)&__EWFVolume },
#endif
#endif
  { 0, NULL, NULL}
};

/** This is the global oracle - it knows everything about everyone. */
Resolver oracle = NULL;

/** We need to call these initialisers explicitely or the class
    references wont work.
*/
/** Our own special add routine */
static void oracle_add(Resolver self, char *uri, char *attribute, 
		       void *value, enum resolver_data_type type,
		       int unique) {
  Resolver i;

  // Call the real add method for the oracle itself
  __Resolver.add(self, uri, attribute, value, type, unique);

  // Add these to all the other identities
  list_for_each_entry(i, &self->identities, identities) {
    CALL(i, add, uri, attribute, value, type, unique);
  };
};

// If attribute is NULL - it means we remove all attributes for this
// URN and then the URN itself
static void oracle_del(Resolver self, char *uri, char *attribute) {
  Resolver i;

  // Call the real add method for the oracle itself
  __Resolver.del(self, uri, attribute);

  // Add these to all the other identities
  list_for_each_entry(i, &self->identities, identities) {
    CALL(i, del, uri, attribute);
  };
};

void AFF4_Init(void) {
  FileLikeObject_init();
  FileBackedObject_init();
  ZipFile_init();
  ZipFileStream_init();
  Image_init();
  MapDriver_init();
  Resolver_init();

#ifdef HAVE_LIBCURL
  HTTPObject_init();
#endif

#ifdef HAVE_LIBAFFLIB
  AFF1Volume_init();
  AFF1Stream_init();
#endif

#ifdef HAVE_LIBEWF
  EWFVolume_init();
  EWFStream_init();
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
      // Try to free the data object and if that succeeds we free the
      // Cache itself. This allows the data to set a destructor which
      // blocks this free. Sometimes its crucial to ensure that a
      // cached object does not get expired suddenly when its not
      // ready.
      if(talloc_free(i->data) != -1) {
	talloc_free(i);
      };
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
      //      printf("Getting %p\n", i);
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

    max_key.dptr = (unsigned char *)MAX_KEY;
    max_key.dsize = strlen(MAX_KEY);

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

static Resolver Resolver_Con(Resolver self) {
  char buff[BUFF_SIZE];
  // The location of the TDB databases
  char *path = getenv("AFF4_TDB_PATH");
  
  if(!path)  path = ".";

  // Open the TDB databases
  if(snprintf(buff, BUFF_SIZE, "%s/urn.tdb", path) >= BUFF_SIZE)
    goto error;
  
  self->urn_db = tdb_open(buff, 1024,
			  0,
			  O_RDWR | O_CREAT, 0644);
  if(!self->urn_db)
    goto error;

  if(snprintf(buff, BUFF_SIZE, "%s/attribute.tdb", path) >= BUFF_SIZE)
    goto error1;

  self->attribute_db = tdb_open(buff, 1024,
				0,
				O_RDWR | O_CREAT, 0644);
  
  if(!self->attribute_db) 
    goto error1;

  if(snprintf(buff, BUFF_SIZE, "%s/data.tdb", path) >= BUFF_SIZE)
    goto error2;

  self->data_db = tdb_open(buff, 1024,
			   0,
			   O_RDWR | O_CREAT, 0644);
  
  if(!self->data_db)
    goto error2;
  
  if(snprintf(buff, BUFF_SIZE, "%s/data_store.tdb", path) >= BUFF_SIZE)
    goto error3;
  
  self->data_store_fd = open(buff, O_RDWR | O_CREAT, 0644);
  if(self->data_store_fd < 0)
    goto error3;

  // This ensures that the data store never has an offset of 0 (This
  // indicates an error)
  // Access to the tdb_store is managed via locks on the data.tdb
  tdb_lockall(self->data_db);
  if(lseek(self->data_store_fd, 0, SEEK_END)==0) {
    (void)write(self->data_store_fd, "data",4);
  };
  tdb_unlockall(self->data_db);

  // Create local read and write caches
  self->read_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  self->write_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);  

  INIT_LIST_HEAD(&self->identities);

  // Make sure we close our files when we get freed
  talloc_set_destructor((void *)self, Resolver_destructor);

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
    specified. Return 1 if found, 0 if not found. 
*/
static uint32_t get_data_head(Resolver self, TDB_DATA uri, TDB_DATA attribute, 
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
	return offset;
      };
      
      free(offset_serialised.dptr);
    };
  };

  return 0;
};

static inline int get_data_next(Resolver self, TDB_DATA_LIST *i){
  if(i->next_offset > 0) {
    lseek(self->data_store_fd, i->next_offset, SEEK_SET);
    if(read(self->data_store_fd, i, sizeof(*i)) == sizeof(*i)) {
      return 1;
    };
  };

  return 0;
};

/** Resolves a single attribute and fills into value. Value needs to
    be initialised with a valid dptr and dsize will indicate the
    buffer size
*/
static int resolve(Resolver self, TDB_DATA urn, TDB_DATA attribute, TDB_DATA *value) {
  TDB_DATA_LIST i;
  
  if(get_data_head(self, urn, attribute, &i)) {
    int length = min(value->dsize, i.length);
    
    // Read this much from the file
    if(read(self->data_store_fd, value->dptr, length) < length) {
      // Oops cant read enough
      goto error;
    };

    value->dsize= length;
    return 1;
  };

 error:
  value->dsize = 0;

  return 0;
};


/** Resolves a single attribute and returns the value. Value will be
    talloced with the context of ctx.
*/
static void *Resolver_resolve(Resolver self, void *ctx, char *urn_str, char *attribute_str,
			      enum resolver_data_type type) {
  TDB_DATA_LIST i;
  TDB_DATA urn, attribute;

  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);
  attribute.dptr = (unsigned char *)attribute_str;
  attribute.dsize = strlen(attribute_str);

  DEBUG("Getting %s, %s\n", urn_str, attribute_str);
  if(get_data_head(self, urn, attribute, &i)) {
    char *result = talloc_size(ctx, i.length+1);

    // Read this much from the file
    if(read(self->data_store_fd, result, i.length) < i.length) {
      // Oops cant read enough
      talloc_free(result);
      return NULL;
    };

    // NULL terminate it
    result[i.length]=0;
    return result;
  };
  
  return NULL;
};

/** Resolves a single attribute and returns the value. Value will be
    talloced with the context of ctx.
*/
static int Resolver_resolve2(Resolver self, char *urn_str, char *attribute_str,
			     void *result, int length,
			     enum resolver_data_type type) {
  TDB_DATA_LIST i;
  TDB_DATA urn, attribute;

  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);
  attribute.dptr = (unsigned char *)attribute_str;
  attribute.dsize = strlen(attribute_str);

  DEBUG("Getting %s, %s\n", urn_str, attribute_str);
  if(get_data_head(self, urn, attribute, &i)) {
    int to_read = min(length, i.length);

    // Make sure its the type our caller expects
    if(i.encoding_type != type) {
      RaiseError(ERuntimeError, "Request for attribute %s with type %d - but its stored with type %d\n", attribute_str, type, i.encoding_type);
      goto not_found;
    };

    // Read this much from the file
    if(read(self->data_store_fd, result, to_read) < to_read) {
      // Oops cant read enough
      goto not_found;
    };

    // NULL terminate it if its a string like type
    switch(type) {
    case RESOLVER_DATA_URN:
    case RESOLVER_DATA_STRING:
      ((char *)result)[to_read++] = 0;
      break;
    default:
      break;
    };

    return to_read;
  };

 not_found:  
  return 0;
};

// check if the value is already set for urn and attribute - honors inheritance
static int is_value_present(Resolver self,TDB_DATA urn, TDB_DATA attribute,
			    TDB_DATA value, int follow_inheritence) {
  TDB_DATA_LIST i;
  char buff[BUFF_SIZE];
  TDB_DATA tmp;

  while(1) {
    // Check the current urn,attribute set
    if(get_data_head(self, urn, attribute, &i)) {
      do {
	if(value.dsize == i.length && i.length < 100000) {
	  char buff[value.dsize];
	  
	  // Read this much from the file
	  if(read(self->data_store_fd, buff, i.length) < i.length) {
	    return 0;
	  };
	  
	  if(!memcmp(buff, value.dptr, value.dsize)) {
	    // Found it:
	    return 1;
	  };
	};
      } while(get_data_next(self, &i));
    };

    if(!follow_inheritence) break;

    // Follow inheritence - FIXME - limit recursion here!!!
    tmp.dptr = (unsigned char *)buff;
    tmp.dsize = BUFF_SIZE;
    // Substitute our urn with a possible inherited URN
    if(resolve(self, urn, INHERIT, &tmp)) { // Found - put in urn
      // Copy the urn
      urn.dptr = (unsigned char *)buff;
      urn.dsize = tmp.dsize;
    } else {
      break;
    };

    // Do it all again with the inherited URN
  };
    
  return 0;
};

static int set_new_value(Resolver self, TDB_DATA urn, TDB_DATA attribute, 
			 TDB_DATA value, enum resolver_data_type type) {
  TDB_DATA key,offset;
  char buff[BUFF_SIZE];
  char buff2[BUFF_SIZE];
  uint32_t new_offset;
  TDB_DATA_LIST i;

  // Update the value in the db and replace with new value
  key.dptr = (unsigned char *)buff;
  key.dsize = calculate_key(self, urn, attribute, buff, BUFF_SIZE, 1);

  // Lock the database
  tdb_lockall(self->data_db);

  // Go to the end and write the new record
  new_offset = lseek(self->data_store_fd, 0, SEEK_END);
  // The offset to the next item in the list
  i.next_offset = 0;
  i.length = value.dsize;
  i.encoding_type = type;

  write(self->data_store_fd, &i, sizeof(i));
  write(self->data_store_fd, value.dptr, value.dsize);

  offset.dptr = (unsigned char *)buff2;
  offset.dsize = tdb_serialise_int(new_offset, buff2, BUFF_SIZE);

  tdb_store(self->data_db, key, offset, TDB_REPLACE);

  //Done
  tdb_unlockall(self->data_db);

  return 1;
};


static TDB_DATA *make_tdb_value(void *ctx, void *value_str, 
			       enum resolver_data_type type) {
  TDB_DATA *value = talloc_size(ctx, sizeof(TDB_DATA));

  // Now serialise the data according to its type
  switch(type) {
  case RESOLVER_DATA_URN:
  case RESOLVER_DATA_STRING: {
    value->dptr = (unsigned char *)value_str;
    value->dsize = strlen(value_str);
    break;
  };

  case RESOLVER_DATA_TDB_DATA: {
    TDB_DATA *data = (TDB_DATA *)value_str;

    value->dptr = data->dptr;
    value->dsize = data->dsize;
    break;
  };

  case RESOLVER_DATA_UINT16: {
    value->dptr = talloc_size(value, sizeof(uint16_t));
    *(uint16_t *)(value->dptr) = *(uint16_t *)value_str;

    value->dsize = sizeof(uint16_t);
    break;
  };

  case RESOLVER_DATA_UINT32: {
    value->dptr = talloc_size(value, sizeof(uint32_t));
    *(uint32_t *)(value->dptr) = *(uint32_t *)value_str;

    value->dsize = sizeof(uint32_t);

    break;
  };

  case RESOLVER_DATA_UINT64: {
    value->dptr = talloc_size(value, sizeof(uint64_t));
    *(uint64_t *)(value->dptr) = *(uint64_t *)value_str;

    value->dsize = sizeof(uint64_t);
    break;
  };
    
  default:
    RaiseError(ERuntimeError, "Unable to serialise items of value %d\n", type);
  };

  return value;
};

/** This sets a single triple into the resolver replacing previous
    values set for this attribute
*/
static void Resolver_set(Resolver self, char *urn_str, 
			 char *attribute_str, char *value_str,
			 enum resolver_data_type type) {
  TDB_DATA urn, attribute, *value;

  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);
  attribute.dptr = (unsigned char *)attribute_str;
  attribute.dsize = strlen(attribute_str);
  value = make_tdb_value(NULL, value_str, type);

  // Grab the lock
  tdb_lockall(self->data_db);

  /** If the value is already in the list, we just ignore this
      request.
  */
  if(!is_value_present(self, urn, attribute, *value, 1)) {
    set_new_value(self, urn, attribute, *value, type);
  };

  tdb_unlockall(self->data_db);

  talloc_free(value);
};

/** Given a head, this function will traverse the data_store list and
    append the values to the attribute list in result (which is really
    a char **.

    We assume self->data_store_fd is seeked to the right place when we
    start.
*/
static int retrieve_attribute_list(Resolver self, 
				   StringIO result, TDB_DATA_LIST head,
				   enum resolver_data_type type) {
  int count = 0;
  do {
    if(head.encoding_type == type) {
      char *buff = talloc_size(result, head.length+1);

      // Read this much from the file
      if(read(self->data_store_fd, buff, head.length) < head.length) {
	// Oops cant read enough
	goto exit;
      };
      
      count++;
      buff[head.length]=0;
      
      // Add the data to the list
      CALL(result, write, (char *)&buff, sizeof(char *));
    };
  } while(get_data_next(self, &head));
  
 exit:
  return count;
};

/** 
    Returns a list of values belonging to the urn, attribute. The list
    is allocated to the ctx provided.
*/
static void **Resolver_resolve_list(Resolver self, void *ctx,
				    char *urn_str, char *attribute_str,
				    enum resolver_data_type type) {
  StringIO result = CONSTRUCT(StringIO, StringIO, Con, ctx);
  TDB_DATA urn,attribute;
  TDB_DATA_LIST i;

  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);
  attribute.dptr = (unsigned char *)attribute_str;
  attribute.dsize = strlen(attribute_str);

  if(get_data_head(self, urn, attribute, &i)) {
    retrieve_attribute_list(self, result, i, type);
  };
  
  // NULL terminate it
  {
    int i=0;
    CALL(result, write, (char *)&i, sizeof(i));
    talloc_steal(ctx, result->data);
  };

  return (void **)result->data;
};

static int Resolver_get_iter(Resolver self, 
			     RESOLVER_ITER *iter, 
			     char *urn_str,
			     char *attribute_str,
			     enum resolver_data_type type) {
  TDB_DATA urn, attribute;
  int result;

  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);
  attribute.dptr = (unsigned char *)attribute_str;
  attribute.dsize = strlen(attribute_str);

  result = get_data_head(self, urn, attribute, &iter->head);

  // Note the current offset of the data_store
  iter->offset = lseek(self->data_store_fd, 0, SEEK_CUR);

  // Make sure that if the iterator is used in iter_next we ignore it
  // since its invalid.
  if(!result || iter->head.encoding_type != type) {
    iter->head.encoding_type = RESOLVER_DATA_UNKNOWN;
  };

  return iter->head.encoding_type != RESOLVER_DATA_UNKNOWN;
};

static int Resolver_iter_next(Resolver self,
			      RESOLVER_ITER *iter, 
			      void *result, int length) {
  // We assume the iter is valid and we write the contents of the iter
  // on here.
  int to_read = min(length-1, iter->head.length);
  int type = iter->head.encoding_type;

  // This is our iteration exit condition
  if(type == RESOLVER_DATA_UNKNOWN) 
    return 0;

  // Read this much from the file
  if(lseek(self->data_store_fd, iter->offset, SEEK_SET) == iter->offset) {
    if(read(self->data_store_fd, result, to_read) < to_read) {
      // Oops cant read enough
      return 0;
    };
  };

  // NULL terminate it if its a string like type
  switch(type) {
  case RESOLVER_DATA_URN:
  case RESOLVER_DATA_STRING:
    ((char *)result)[to_read++] = 0;
    break;
  default:
    break;
  };

  // Update the iterator to the next value
  if(iter->head.next_offset == 0) {
    iter->head.encoding_type = RESOLVER_DATA_UNKNOWN;
  } else {
    get_data_next(self, &iter->head);
    iter->offset = lseek(self->data_store_fd, 0, SEEK_CUR);
  };

  return 1;
};

/** Instantiates a single instance of the class using this
    resolver. The returned object MUST be returned to the cache using
    cache_return(). We initially create the object using the NULL
    context, and when the object is returned to the cache, it will be
    stolen again.
*/
static AFFObject Resolver_open(Resolver self, char *urn, char mode) {
  int i;
  char *stream_type;
  struct dispatch_t *dispatch_ptr=NULL;
  AFFObject result;
  Cache tmp;

  if(!urn) return NULL;

  // Is this object cached?
  if(mode =='r') {
    tmp = CALL(self->read_cache, get, urn);
  } else {
    tmp = CALL(self->write_cache, get, urn);
  };

  if(tmp) {
    result = (AFFObject)tmp->data;
    talloc_steal(NULL, result);
    talloc_free(tmp);

    // If its a FileLikeObject seek it to 0
    if(ISSUBCLASS(result, FileLikeObject)) {
      CALL((FileLikeObject)result, seek, 0, SEEK_SET);
    };

    // Lock it
    CALL(self, lock, URNOF(result), mode);
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
    stream_type = (char *)CALL(self, resolve, self, urn, AFF4_TYPE,
			       RESOLVER_DATA_URN);
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
      goto error;
    } else {
      RaiseError(ERuntimeError, "Unable to open %s", urn);
      goto error;
    };
  };
  
  // A special constructor from a class reference
  result = CONSTRUCT_FROM_REFERENCE(dispatch[i].class_ptr, 
				    Con, NULL, urn, mode);
  
  // Make sure the object mode is set
  if(result) {
    talloc_set_name(result, "%s %s", NAMEOF(result), URNOF(result));
    ((AFFObject)result)->mode = mode;

    // Lock it
    CALL(self, lock, URNOF(result), mode);
  };

  return result; 

 error:
  return NULL;
};

/** Return the object to the cache. Callers may not make a reference
    to it after that.
*/
static void Resolver_return(Resolver self, AFFObject obj) {
  // Cache it
  if(!obj) return;

  // Grab the lock
  if(obj->mode == 'r') {
    char *urn = talloc_strdup(self, obj->urn);
    CALL(self->read_cache, put, urn, obj, sizeof(*obj));
  } else if(obj->mode == 'w') {
    char *urn = talloc_strdup(self, obj->urn);
    Cache_put(self->write_cache, urn, obj, sizeof(*obj));
  } else {
    RaiseError(ERuntimeError, "Programming error. %s has no valid mode", NAMEOF(obj));
  };
  
  // Unlock the URN
  CALL(self, unlock, URNOF(obj), obj->mode);
};

static void Resolver_add(Resolver self, char *urn_str, 
			 char *attribute_str, void *value_str,
			 enum resolver_data_type type,
			 int unique) {
  TDB_DATA urn, attribute, *value, key;
  TDB_DATA_LIST i;
  uint32_t previous_offset=0;
  uint32_t new_offset;
  TDB_DATA offset;
  char buff[BUFF_SIZE];
  char buff2[BUFF_SIZE];
  
  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);
  attribute.dptr = (unsigned char *)attribute_str;
  attribute.dsize = strlen(attribute_str);
  value = make_tdb_value(NULL, value_str, type);

  DEBUG("Adding %s, %s\n", urn_str, attribute_str);

  /** If the value is already in the list, we just ignore this
      request.
  */
  if(unique && is_value_present(self, urn, attribute, *value, 1)) {
    goto exit;
  };

  // Ok if we get here, the value is not already stored there.
  key.dptr = (unsigned char *)buff;
  key.dsize = calculate_key(self, urn, attribute, buff, BUFF_SIZE, 1);

  // Lock the data_db to synchronise access to the store:
  tdb_lockall(self->data_db);

  offset = tdb_fetch(self->data_db, key);
  if(offset.dptr) {
    previous_offset = tdb_to_int(offset);
    free(offset.dptr);
  };

  // Go to the end and write the new record
  new_offset = lseek(self->data_store_fd, 0, SEEK_END);
  i.next_offset = previous_offset;
  i.length = value->dsize;
  i.encoding_type = type;

  write(self->data_store_fd, &i, sizeof(i));
  write(self->data_store_fd, value->dptr, value->dsize);    

  // Now store the offset to this in the tdb database
  value->dptr = (unsigned char *)buff2;
  value->dsize = tdb_serialise_int(new_offset, buff2, BUFF_SIZE);

  tdb_store(self->data_db, key, *value, TDB_REPLACE);

  // Done
  tdb_unlockall(self->data_db);

 exit:
  talloc_free(value);
  return;
};

static int Resolver_is_set(Resolver self, char *uri, char *attribute, char *value) {
};

/** Format all the attributes of the object specified by urn */
static char *Resolver_export_urn(Resolver self, char *urn, char *context) {
};

static char *Resolver_export_all(Resolver self, char *context) {
};

static AFFObject Resolver_create(Resolver self, AFFObject *class_reference) {
};

static void Resolver_del(Resolver self, char *urn_str, char *attribute_str) {
  TDB_DATA urn;
  TDB_DATA attribute;
  TDB_DATA key;
  char buff[BUFF_SIZE];

  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);
  if(attribute_str) {
    attribute.dptr = (unsigned char *)attribute_str;
    attribute.dsize = strlen(attribute_str);

    key.dptr = (unsigned char *)buff;
    key.dsize = calculate_key(self, urn, attribute, buff, BUFF_SIZE, 0);

    // Remove the key from the database
    tdb_delete(self->data_db, key);
  } else {
    TDB_DATA max_key;
    int max_id,i;

    max_key.dptr = (unsigned char *)MAX_KEY;
    max_key.dsize = strlen(MAX_KEY);

    max_key = tdb_fetch(self->attribute_db, max_key);
    if(max_key.dptr) {
      max_id = tdb_to_int(max_key);
      free(max_key.dptr);
    };
    
    for(i=0;i<max_id;i++) {
      char buff[BUFF_SIZE];
      int urn_id =get_id(self->urn_db, urn, 0);
      
      key.dptr = (unsigned char *)buff;
      key.dsize = snprintf(buff, BUFF_SIZE, "%d:%d", urn_id, i);

      // Remove the key from the database
      tdb_delete(self->data_db, key);
    };
  };
};
  
// Parse a properties file (implicit context is context - if the file
// does not specify a subject URN we use context instead).
static void Resolver_parse(Resolver self, char *context_urn, char *text, int len) {
};

/** Synchronization methods. */
int Resolver_lock(Resolver self, char *urn_str, char mode) {
  TDB_DATA attribute;
  uint64_t offset;
  TDB_DATA_LIST data_list;
  TDB_DATA urn;

  urn.dptr = (unsigned char *)urn_str;
  urn.dsize = strlen(urn_str);

  if(mode == 'r') attribute=RLOCK;
  else if(mode =='w') attribute=WLOCK;
  else {
    RaiseError(ERuntimeError, "Invalid mode %c", mode);
    return 0;
  };

  offset = get_data_head(self, urn, attribute, &data_list);
  if(!offset){
    // The attribute is not set - make it now:
    set_new_value(self,urn, attribute, LOCK, RESOLVER_DATA_STRING);
    offset = get_data_head(self, urn, attribute, &data_list);
    if(!offset) {
      RaiseError(ERuntimeError, "Unable to set lock attribute");
      return 0;
    };
  };

  // If we get here data_list should be valid:
  lseek(self->data_store_fd, offset, SEEK_SET);
  if(lockf(self->data_store_fd, F_LOCK, data_list.length)==-1){
    RaiseError(ERuntimeError, "Unable to lock: %s", strerror(errno));
    return 0;
  };

  return 1;
};

int Resolver_unlock(Resolver self, char *urn, char mode) {

};

/** Here we implement the resolver */
VIRTUAL(Resolver, AFFObject)
     VMETHOD(Con) = Resolver_Con;
     VMETHOD(create) = Resolver_create;

     VMETHOD(resolve) = Resolver_resolve;
     VMETHOD(resolve2) = Resolver_resolve2;
     VMETHOD(resolve_list) = Resolver_resolve_list;
     VMETHOD(get_iter) = Resolver_get_iter;
     VMETHOD(iter_next) = Resolver_iter_next;
     VMETHOD(add)  = Resolver_add;
     VMETHOD(export_urn) = Resolver_export_urn;
     VMETHOD(export_all) = Resolver_export_all;
     VMETHOD(open) = Resolver_open;
     VMETHOD(cache_return) = Resolver_return;
     VMETHOD(set) = Resolver_set;
     VMETHOD(is_set) = Resolver_is_set;
     VMETHOD(del) = Resolver_del;
     VMETHOD(parse) = Resolver_parse;
     VMETHOD(lock) = Resolver_lock;
     VMETHOD(unlock) = Resolver_unlock;
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

  self->mode = mode;

  return self;
};


static void AFFObject_set_property(AFFObject self, char *attribute, void *value,
				   enum resolver_data_type type) {
  CALL(oracle, set,
       self->urn,
       attribute, value, type);
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
void *resolver_get_with_default(Resolver self, char *urn, char *attribute, 
				void *default_value,
				enum resolver_data_type type) {
  void *result = CALL(self, resolve, self, urn, attribute, type);

  if(!result) {
    CALL(self, add, urn, attribute, default_value, type, 1);
    result = default_value;
  };

  return result;
};

