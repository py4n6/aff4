/*************************************************
This file tests the resolver.
***************************************************/

#include "aff4_internal.h"

INIT() {
  Cache_init((Object)&__Cache);

  return 0;
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
