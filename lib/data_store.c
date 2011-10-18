/****************************************************
  An implementation of the data store.
****************************************************/
#include "aff4_internal.h"


static DataStoreObject DataStoreObject_Con(DataStoreObject self, char *data,
                                           unsigned int length, char *rdf_type) {
  self->data = talloc_memdup(self, data, length);
  self->length = length;
  self->rdf_type = talloc_strdup(self, rdf_type);

  return self;
};

VIRTUAL(DataStoreObject, Object)
  VMETHOD(Con) = DataStoreObject_Con;
END_VIRTUAL


static DataStore DataStore_Con(DataStore this) {
  MemoryDataStore self = (MemoryDataStore)this;
  AFF4_GL_LOCK;

  self->urn_db = CONSTRUCT(Cache, Cache, Con, self, 100, 0);
  self->attribute_db = CONSTRUCT(Cache, Cache, Con, self, 100, 0);
  self->data_db = CONSTRUCT(Cache, Cache, Con, self, 100, 0);
  self->id_counter = 0;

  AFF4_GL_UNLOCK;
  return this;
};

static void DataStore_lock(DataStore this) {
};

static void DataStore_unlock(DataStore this) {
};

static XSDInteger get_or_create(MemoryDataStore self, Cache cache, char *key) {
  XSDInteger result;

  AFF4_GL_LOCK;
  result = (XSDInteger)CALL(cache, borrow, ZSTRING_NO_NULL(key));

  if(!result) {
    self->id_counter++;
    result = CONSTRUCT(XSDInteger, RDFValue, Con, self);
    CALL(result, set, self->id_counter);
    CALL(cache, put, ZSTRING_NO_NULL(key), (Object)result);
  };

  AFF4_GL_UNLOCK;
  return result;
};


static void DataStore_del(DataStore this, char *uri, char *attribute) {
  MemoryDataStore self = (MemoryDataStore)this;
  uint64_t data_ptr[2];
  XSDInteger uri_index;
  XSDInteger attr_index;

  AFF4_GL_LOCK;
  uri_index = get_or_create(self, self->urn_db, uri);
  attr_index = get_or_create(self, self->attribute_db,attribute);

  // Combine the values as an index to the data_db.
  data_ptr[0] = uri_index->value;
  data_ptr[1] = attr_index->value;

  // Remove all the objects from the cache.
  while (1) {
    Object obj = CALL(self->data_db, get, NULL, (char *)data_ptr, sizeof(data_ptr));
    if (!obj) break;

    talloc_free(obj);
  };

  AFF4_GL_UNLOCK;
};

static void DataStore_set(DataStore this, char *uri, char *attribute,
                          DataStoreObject value) {
  MemoryDataStore self = (MemoryDataStore)this;
  uint64_t data_ptr[2];
  XSDInteger uri_index;
  XSDInteger attr_index;

  AFF4_GL_LOCK;

  uri_index = get_or_create(self, self->urn_db, uri);
  attr_index = get_or_create(self, self->attribute_db, attribute);

  // Combine the values as an index to the data_db
  data_ptr[0] = uri_index->value;
  data_ptr[1] = attr_index->value;

  // Remove all the old objects from the cache.
  while (1) {
    Object obj = CALL(self->data_db, get, NULL, (char *)data_ptr, sizeof(data_ptr));
    if (!obj) break;

    talloc_free(obj);
  };

  // Set the new object.
  CALL(self->data_db, put, (char *)data_ptr, sizeof(data_ptr), (Object)value);

  AFF4_GL_UNLOCK;
};


static void DataStore_add(DataStore this, char *uri, char *attribute,
                          DataStoreObject value) {
  MemoryDataStore self = (MemoryDataStore)this;
  uint64_t data_ptr[2];
  XSDInteger uri_index;
  XSDInteger attr_index;

  AFF4_GL_LOCK;
  uri_index = get_or_create(self, self->urn_db, uri);
  attr_index = get_or_create(self, self->attribute_db, attribute);

  // Combine the values as an index to the data_db
  data_ptr[0] = uri_index->value;
  data_ptr[1] = attr_index->value;

  // Set the new object.
  CALL(self->data_db, put, (char *)data_ptr, sizeof(data_ptr), (Object)value);

  AFF4_GL_UNLOCK;
};

static DataStoreObject DataStore_get(DataStore this, char *uri, char *attribute) {
  MemoryDataStore self = (MemoryDataStore)this;
  uint64_t data_ptr[2];
  XSDInteger uri_index;
  XSDInteger attr_index;
  DataStoreObject value;

  AFF4_GL_LOCK;
  uri_index = get_or_create(self, self->urn_db, uri);
  attr_index = get_or_create(self, self->attribute_db, attribute);

  // Combine the values as an index to the data_db
  data_ptr[0] = uri_index->value;
  data_ptr[1] = attr_index->value;

  // Set the new object.
  value = (DataStoreObject)CALL(self->data_db, borrow, (char *)data_ptr, sizeof(data_ptr));

  AFF4_GL_UNLOCK;
  return value;
};

static Object DataStore_iter(DataStore this, char *uri, char *attribute) {
  MemoryDataStore self = (MemoryDataStore)this;
  uint64_t data_ptr[2];
  XSDInteger uri_index;
  XSDInteger attr_index;
  Object iter;

  AFF4_GL_LOCK;
  uri_index = get_or_create(self, self->urn_db, uri);
  attr_index = get_or_create(self, self->attribute_db, attribute);

  // Combine the values as an index to the data_db
  data_ptr[0] = uri_index->value;
  data_ptr[1] = attr_index->value;

  // Set the new object.
  iter = CALL(self->data_db, iter, (char *)data_ptr, sizeof(data_ptr));

  AFF4_GL_UNLOCK;
  return iter;
};

static DataStoreObject DataStore_next(DataStore this, Object *iter) {
  MemoryDataStore self = (MemoryDataStore)this;
  DataStoreObject result;

  AFF4_GL_LOCK;
  result = (DataStoreObject)CALL(self->data_db, next, iter);
  AFF4_GL_UNLOCK;

  return result;
};


/* This is an abstract class so it does not implement anything. */
VIRTUAL(DataStore, Object)
  UNIMPLEMENTED(DataStore, Con);
  UNIMPLEMENTED(DataStore, lock);
  UNIMPLEMENTED(DataStore, unlock);
  UNIMPLEMENTED(DataStore, del);
  UNIMPLEMENTED(DataStore, set);
  UNIMPLEMENTED(DataStore, add);
  UNIMPLEMENTED(DataStore, get);
  UNIMPLEMENTED(DataStore, iter);
  UNIMPLEMENTED(DataStore, next);
END_VIRTUAL

VIRTUAL(MemoryDataStore, DataStore)
  VMETHOD_BASE(DataStore, Con) = DataStore_Con;
  VMETHOD_BASE(DataStore, lock) = DataStore_lock;
  VMETHOD_BASE(DataStore, unlock) = DataStore_unlock;
  VMETHOD_BASE(DataStore, del) = DataStore_del;
  VMETHOD_BASE(DataStore, set) = DataStore_set;
  VMETHOD_BASE(DataStore, add) = DataStore_add;
  VMETHOD_BASE(DataStore, get) = DataStore_get;
  VMETHOD_BASE(DataStore, iter) = DataStore_iter;
  VMETHOD_BASE(DataStore, next) = DataStore_next;
END_VIRTUAL


DataStore new_MemoryDataStore(void *ctx) {
  return CONSTRUCT(MemoryDataStore, DataStore, Con, ctx);
};
