/** Implementation of Caches */

#include "aff4_internal.h"

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


VIRTUAL(ThreadPoolJob, Object) {
  UNIMPLEMENTED(ThreadPoolJob, run);
} END_VIRTUAL


static void *ThreadPool_worker(void *ctx) {
  ThreadPool pool = (ThreadPool) ctx;

  while(1) {
    /* One hundredth of a second time resolution for cancelation. */
    ThreadPoolJob job = CALL(pool->jobs, get, 100000);
    if(job) {
      // Run the job
      CALL(job, run);

      /* Now place it on the completed queue */
      CALL(pool->completed_jobs, put, job, 100000);

      /* Only quit if the pool is not active and there are no more
         waiting tasks.
      */
    } else if(!pool->active) break;
  };

  return NULL;
};


static ThreadPool ThreadPool_Con(ThreadPool self, int number) {
  int i = 0;

  self->jobs = CONSTRUCT(Queue, Queue, Con, self, number);
  self->completed_jobs = CONSTRUCT(Queue, Queue, Con, self, number);
  self->number_of_threads = number;
  self->threads = talloc_array(self, pthread_t, number + 1);
  self->active = True;

  /* Start up all the threads. */
  for(i = 0; i<number; i++) {
    pthread_create(&self->threads[i], NULL, ThreadPool_worker, self);
  };

  return self;
};

static void ThreadPool_completion(ThreadPool self) {
  /* Run any completion routines. */
  while(1) {
    ThreadPoolJob job = (ThreadPoolJob)CALL(self->completed_jobs, get, 0);
    if(!job) break;
    CALL(job, complete);
    talloc_free(job);
  };
};

static int ThreadPool_schedule(ThreadPool self, ThreadPoolJob job, 
                               int timeout) {
  ThreadPool_completion(self);

  return CALL(self->jobs, put, job, timeout * 1000000);
};

static void ThreadPool_join(ThreadPool self) {
  int i;

  self->active = False;

  /* Wait for the workers to quite. */
  for(i=0; i < self->number_of_threads; i++) {
    pthread_join(self->threads[i], NULL);
  };

  ThreadPool_completion(self);
};


VIRTUAL(ThreadPool, Object) {
  VMETHOD(Con) = ThreadPool_Con;
  VMETHOD(schedule) = ThreadPool_schedule;
  VMETHOD(complete) = ThreadPool_completion;
  VMETHOD(join) = ThreadPool_join;
} END_VIRTUAL
