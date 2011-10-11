/*************************************************
This file tests various utility classes
***************************************************/

#include "aff4_internal.h"


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


static int time_difference(struct timeval *prev, struct timeval *now) {
  uint64_t prev_usec = prev->tv_sec * 1000000 + prev->tv_usec;
  uint64_t now_usec = now->tv_sec * 1000000 + now->tv_usec;

  return now_usec - prev_usec;
};


/*********************************************
  Tests the queue implementation.
*********************************************/
TEST(QueueTest) {
  Queue queue = CONSTRUCT(Queue, Queue, Con, NULL, 3);
  int timeout = 1000000;
  struct timeval now, prev;

  /* Test we can push some data */
  CU_ASSERT(1 == CALL(queue, put, talloc_strdup(queue, "1"), timeout));
  CU_ASSERT(1 == CALL(queue, put, talloc_strdup(queue, "2"), timeout));
  CU_ASSERT(1 == CALL(queue, put, talloc_strdup(queue, "3"), timeout));

  /* This should now fail because the queue is full */
  gettimeofday(&prev, NULL);
  CU_ASSERT(0 == CALL(queue, put, talloc_strdup(queue, "4"), timeout));
  gettimeofday(&now, NULL);
  CU_ASSERT(time_difference(&prev, &now) > timeout);

  /* Lets get from the queue */
  CU_ASSERT_STRING_EQUAL("1", CALL(queue, get, queue, timeout));

  /* Now there is room */
  CU_ASSERT(1 == CALL(queue, put, talloc_strdup(queue, "4"), timeout));

  CU_ASSERT_STRING_EQUAL("2", CALL(queue, get, queue, timeout));
  CU_ASSERT_STRING_EQUAL("3", CALL(queue, get, queue, timeout));
  CU_ASSERT_STRING_EQUAL("4", CALL(queue, get, queue, timeout));
 
  /* Nothing left */
  gettimeofday(&prev, NULL);
  CU_ASSERT(NULL == CALL(queue, get, queue, timeout));
  gettimeofday(&now, NULL);
  CU_ASSERT(time_difference(&prev, &now) > timeout);

  talloc_free(queue);
};
