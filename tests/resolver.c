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
  CU_ASSERT_EQUAL(NULL, test->get(test, ZSTRING(key2)));
  
  // Key hit
  url2 = (RDFURN)test->get(test, ZSTRING(key1));
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
