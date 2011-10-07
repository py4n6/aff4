/*************************************************
This file tests the resolver.
***************************************************/

#include "aff4_internal.h"

INIT() {
  Cache_init((Object)&__Cache);

  return 0;
};

/**********************************************
Test a Cache object - similar to a python dict.
***********************************************/
TEST(CacheTest1) {
  Cache test = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 0);

  // Keys can not be static
  char *key1 = talloc_strdup(test, "hello");
  char *key2 = talloc_strdup(test, "world");
  RDFURN url = new_RDFURN(test);
  RDFURN url2;

  url->set(url, "http://www.example.com");
  CU_ASSERT_EQUAL(test->cache_size, 0);

  test->put(test, ZSTRING(key1), (Object)url);

  CU_ASSERT_EQUAL(test->cache_size, 1);

  // Present
  CU_ASSERT_TRUE(test->present(test, ZSTRING(key1)));
  CU_ASSERT_FALSE(test->present(test, ZSTRING(key2)));

  // Borrow
  // Its the same object returned
  CU_ASSERT_EQUAL(test->borrow(test, ZSTRING(key1)), (Object)url);
  CU_ASSERT_EQUAL(test->cache_size, 1);

  // Get
  // Missing key
  CU_ASSERT_EQUAL(NULL, test->get(test, test, ZSTRING(key2)));

  // Key hit
  url2 = (RDFURN)test->get(test, test, ZSTRING(key1));
  CU_ASSERT_EQUAL(url2, url);
  CU_ASSERT_EQUAL(test->cache_size, 0);


  aff4_free(test);
};

TEST(CacheTestExpiry) {
  // Expire more than 10 objects
  Cache test = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 10);
  int i;

  // Fill Cache with first 10 entries
  for(i=1; i < 10; i++) {
    char *key = talloc_strdup(test, "Ahello");
    RDFURN url = new_RDFURN(test);

    key[0] = i + 'A';
    url->set(url, "http://www.example.com");

    test->put(test, ZSTRING(key), (Object)url);
    CU_ASSERT_EQUAL(test->cache_size, i);
  };

  // Fill Cache with the next 5 - older ones will be expired
  for(i=10; i < 15; i++) {
    char *key = talloc_strdup(test, "Ahello");
    RDFURN url = new_RDFURN(test);

    key[0] = i + 'A';
    url->set(url, "http://www.example.com");

    test->put(test, ZSTRING(key), (Object)url);
    CU_ASSERT_EQUAL(test->cache_size, 10);
  };

  // Old one expired
  CU_ASSERT_PTR_NULL(test->borrow(test, ZSTRING("Ahello")));

  // New one still there
  CU_ASSERT_PTR_NOT_NULL(test->borrow(test, ZSTRING("Ghello")));
  CU_ASSERT_PTR_NOT_NULL(test->borrow(test, ZSTRING("Mhello")));

  aff4_free(test);
};


/**********************************************
Test MemoryDataStore object
***********************************************/
TEST(MemoryDataStoreTestSet) {
  DataStore store = new_MemoryDataStore(NULL);
  DataStoreObject value = CONSTRUCT(DataStoreObject, DataStoreObject, Con, store,
                                    ZSTRING("hello"), "xsd:string");

  DataStoreObject value1 = CONSTRUCT(DataStoreObject, DataStoreObject, Con, store,
                                    ZSTRING("world"), "xsd:string");
  DataStoreObject test;

  /* Lock the store */
  CALL(store, lock);

  /* Check that we can set and get the same string */
  CALL(store, set, "url", "attribute", value);
  test = CALL(store, get, "url", "attribute");
  CU_ASSERT_NSTRING_EQUAL(test->data, value->data, value->length);
  CU_ASSERT_STRING_EQUAL(test->data, "hello");
  CU_ASSERT_EQUAL(test->length, value->length);

  /* Check that setting again displaces old values. */
  CALL(store, set, "url", "attribute", value1);
  test = CALL(store, get, "url", "attribute");
  CU_ASSERT_STRING_EQUAL(test->data, "world");

  CALL(store, unlock);
  talloc_free(store);
};


TEST(MemoryDataStoreTestAdd) {
  DataStore store = new_MemoryDataStore(NULL);
  DataStoreObject value = CONSTRUCT(DataStoreObject, DataStoreObject, Con, store,
                                    ZSTRING("hello"), "xsd:string");

  DataStoreObject value1 = CONSTRUCT(DataStoreObject, DataStoreObject, Con, store,
                                    ZSTRING("world"), "xsd:string");
  DataStoreObject test;

  Object iter;

  /* Lock the store */
  CALL(store, lock);

  /* Check that we can set and get the same string */
  CALL(store, set, "url", "attribute", value);
  CALL(store, add, "url", "attribute", value1);

  /* Now iterate */
  iter = CALL(store, iter, "url", "attribute");
  CU_ASSERT_PTR_NOT_NULL(iter);

  test = CALL(store, next, &iter);
  CU_ASSERT_STRING_EQUAL(test->data, "hello");
  CU_ASSERT_PTR_NOT_NULL(iter);

  test = CALL(store, next, &iter);
  CU_ASSERT_STRING_EQUAL(test->data, "world");
  CU_ASSERT_PTR_NULL(iter);

  CALL(store, unlock);

  aff4_free(store);
};


/**********************************************
Test Resolver object
***********************************************/
TEST(AFF4ResolverTest) {
  Resolver resolver = AFF4_get_resolver(NULL, NULL);
  RDFURN urn = new_RDFURN(resolver);
  XSDString value = new_XSDString(resolver);

  value->set(value, ZSTRING("hello"));
  urn->set(urn, "http://www.test.com/foobar");

  CALL(resolver, set, urn, "attribute", (RDFValue)value);

  value = (XSDString)CALL(resolver, resolve, resolver, urn, "attribute");

  CU_ASSERT_STRING_EQUAL(value->value, "hello");
  CU_ASSERT_STRING_EQUAL(NAMEOF(value), "XSDString");

  aff4_free(resolver);
};
