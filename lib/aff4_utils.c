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
  AFF4_GL_LOCK;

  self->hash_table_width = hash_table_width;
  self->max_cache_size = max_cache_size;

  INIT_LIST_HEAD(&self->cache_list);
  INIT_LIST_HEAD(&self->hash_list);

  // Install our destructor
  talloc_set_destructor((void *)self, Cache_destructor);

  AFF4_GL_UNLOCK;
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

  AFF4_GL_LOCK;

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

  AFF4_GL_UNLOCK;
  return new_cache;
};

static Object Cache_get(Cache self, void *ctx, char *key, int len) {
  int hash;
  Cache hash_list_head;
  Cache i;

  AFF4_GL_LOCK;

  if(!self->hash_table)
    goto error;

  hash = CALL(self, hash, key, len);
  hash_list_head = self->hash_table[hash];
  if(!hash_list_head)
    goto error;

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

      AFF4_GL_UNLOCK;
      return result;
    };
  };

  RaiseError(EKeyError, "Key '%s' not found in Cache", key);
 error:
  AFF4_GL_UNLOCK;
  return NULL;
};


static Object Cache_borrow(Cache self, char *key, int len) {
  int hash;
  Cache hash_list_head;
  Cache i;

  AFF4_GL_LOCK;

  if(!self->hash_table)
    goto error;

  hash = CALL(self, hash, key, len);
  hash_list_head = self->hash_table[hash];
  if(!hash_list_head)
    goto error;

  // There are 2 lists each Cache object is on - the hash list is a
  // shorter list at the end of each hash table slot, while the cache
  // list is a big list of all objects in the cache. We find the
  // object using the hash list, but expire the object based on the
  // cache list which is also kept in sorted order.
  list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
    if(i->key_len == len && !CALL(i, cmp, key, len)) {
      AFF4_GL_UNLOCK;

      return i->data;
    };
  };

 error:
  AFF4_GL_UNLOCK;
  return NULL;
};


int Cache_present(Cache self, char *key, int len) {
  int hash;
  Cache hash_list_head;
  Cache i;

  AFF4_GL_LOCK;

  if(self->hash_table) {
    hash = CALL(self, hash, key, len);
    hash_list_head = self->hash_table[hash];
    if(!hash_list_head)
      goto exit;

    list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
      if(i->key_len == len && !CALL(i, cmp, key, len)) {
        AFF4_GL_UNLOCK;
        return 1;
      };
    };
  };

 exit:
  AFF4_GL_UNLOCK;
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

  AFF4_GL_LOCK;

  while(1) {
    /* One hundredth of a second time resolution for cancelation. */
    ThreadPoolJob job;

    job = CALL(pool->jobs, get, 100000);
    if(job) {
      // Run the job
      CALL(job, run);

      /* Only quit if the pool is not active and there are no more
         waiting tasks.
      */
    } else if(!pool->active) {
      break;
    };
  };

  AFF4_GL_UNLOCK;
  return NULL;
};


static ThreadPool ThreadPool_Con(ThreadPool self, int number) {
  int i = 0;

  AFF4_GL_LOCK;

  self->jobs = CONSTRUCT(Queue, Queue, Con, self, number);
  self->completed_jobs = CONSTRUCT(Queue, Queue, Con, self, number);
  self->number_of_threads = number;
  self->threads = talloc_array(self, pthread_t, number + 1);
  self->active = True;

  /* Start up all the threads. */
  for(i = 0; i<number; i++) {
    pthread_create(&self->threads[i], NULL, ThreadPool_worker, self);
  };

  AFF4_GL_UNLOCK;
  return self;
};


static int ThreadPool_schedule(ThreadPool self, ThreadPoolJob job, 
                               int timeout) {
  int result;

  AFF4_GL_LOCK;

  result = CALL(self->jobs, put, job, timeout * 1000000);

  AFF4_GL_UNLOCK;

  return result;
};

static void ThreadPool_join(ThreadPool self) {
  int i;
  int depth;

  AFF4_GL_LOCK;
  self->active = False;

  /* Wait for the workers to quit. */
  for(i=0; i < self->number_of_threads; i++) {
    /* Allow other threads to run while we wait here. */
    AFF4_BEGIN_ALLOW_THREADS;
    pthread_join(self->threads[i], NULL);
    AFF4_END_ALLOW_THREADS;
  };

  AFF4_GL_UNLOCK;
};


VIRTUAL(ThreadPool, Object) {
  VMETHOD(Con) = ThreadPool_Con;
  VMETHOD(schedule) = ThreadPool_schedule;
  VMETHOD(join) = ThreadPool_join;
} END_VIRTUAL


AFF4GlobalLock AFF4GlobalLock_Con(AFF4GlobalLock self) {
  pthread_mutexattr_t mutex_attr;

  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);

  pthread_mutex_init(&self->mutex, &mutex_attr);
  pthread_mutexattr_destroy(&mutex_attr);

  return self;
};

void AFF4GlobalLock_lock(AFF4GlobalLock self, int number){
  int i;

  for(i=0; i<number; i++) {
    if(pthread_mutex_lock(&self->mutex) == 0) {
      self->depth ++;
    } else {
      printf("Error locking %08X\n", pthread_self());
    };
  };
};

void AFF4GlobalLock_unlock(AFF4GlobalLock self, int number) {
  int i;
  int result;

  number = min(number, self->depth);

  for(i=0; i<number; i++) {
    self->depth --;
    pthread_mutex_unlock(&self->mutex);
  };
};

int AFF4GlobalLock_timedwait(AFF4GlobalLock self, pthread_cond_t *condition, int timeout) {
  struct timeval now;
  struct timespec deadline;
  int depth;
  int res;

  gettimeofday(&now, NULL);
  deadline.tv_sec = now.tv_sec + timeout / 1000000;
  deadline.tv_nsec = (now.tv_usec + timeout % 1000000) * 1000;

  /* Now we unlock the mutex until a depth of one, then wait on the
     condition variable. This ensures the mutex becomes completely
     unlocked and other threads can run.
   */
  depth = self->depth;

  CALL(self, unlock, depth - 1);

  /* waiting on the condition variable unlocks the mutex implicitely
     so we need to track it.
  */
  self->depth --;
  res = pthread_cond_timedwait(condition,
                               &self->mutex, &deadline);
  self->depth ++;

  /* Restore the lock level. */
  CALL(self, lock, depth - 1);
  return res;
};


static int AFF4GlobalLock_allow_threads(AFF4GlobalLock self) {
  int depth = self->depth;

  CALL(self, unlock, depth);
  return depth;
};


VIRTUAL(AFF4GlobalLock, Object) {
  VMETHOD(Con) = AFF4GlobalLock_Con;
  VMETHOD(lock) = AFF4GlobalLock_lock;
  VMETHOD(unlock) = AFF4GlobalLock_unlock;
  VMETHOD(timedwait) = AFF4GlobalLock_timedwait;
  VMETHOD(allow_threads) = AFF4GlobalLock_allow_threads;
} END_VIRTUAL
