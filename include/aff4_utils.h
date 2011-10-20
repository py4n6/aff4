/*
** aff4_utils.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:43:54 2009 mic
** Last update Wed Jan 27 11:02:12 2010 mic
*/

#ifndef   	AFF4_UTILS_H_
# define   	AFF4_UTILS_H_

#include "class.h"
#include "tdb.h"
#include "list.h"


/* Thread control within the AFF4 library:

   In order to ensure the AFF4 library is thread safe, there is a
   global lock mutex to protect all aff4 library calls. A similar
   strategy is used by the python interpreter. The following rules
   must be followed:

   1 All user code outsize the library is allowed to run with the
     global lock released. This allows user space code to manage their
     own threads as they see fit.

   2 Upon entry to every AFF4 function, the global lock is acquired
     using AFF4_GL_LOCK, and released on exit using
     AFF4_GL_UNLOCK. Since the global lock is recursive, this ensures
     that the lock is held for every library call.

   3 When operations are performed in the AFF4 core which can release
     the lock (e.g. system calls or compression opeations), the global
     lock is completely released using the AFF4_BEGIN_ALLOW_THREADS
     and re-acquired with AFF4_END_ALLOW_THREADS. Note that it is
     imperative that no memory allocation is performed with threads
     allowed or race conditions can occur.

   4 All new threads which require access to AFF4 data structures must
     acquire the lock before starting and release it when
     finishing. This restriction is consistent with rule 2 above.

   This strategy ensures that there is only a single thread at a time
   which can touch any aff4 data structures. This is done to protect
   talloc contexts from data races. It is usually sufficient to allow
   threads to run when performing cpu intensive operations
   (e.g. crypto or compression), or waiting in system calls. In
   particular if there is a possibility that code may block for long
   time - threads must be allowed to run during this. Please also pay
   attention to memory ownership to ensure another thread does not
   free an object currently in use by the blocked thread - use the
   resolver's manage/own/cache_return semantics to ensure that.
 */
CLASS(AFF4GlobalLock, Object)
   pthread_mutex_t mutex;
   pthread_t current_thread;
   /* The recursiveness of this lock. */
   int depth;

   AFF4GlobalLock METHOD(AFF4GlobalLock, Con);
   void METHOD(AFF4GlobalLock, lock, int times);
   void METHOD(AFF4GlobalLock, unlock, int times);
   int METHOD(AFF4GlobalLock, timedwait, pthread_cond_t *condition, int timeout);

   int METHOD(AFF4GlobalLock, allow_threads);
END_CLASS

extern AFF4GlobalLock aff4_gl_lock;

   /* Use these on entry and exit from each function. */
#define AFF4_GL_LOCK CALL(aff4_gl_lock, lock, 1);
#define AFF4_GL_UNLOCK CALL(aff4_gl_lock, unlock, 1);

   /* Use these when it is safe to allow other threads to run
      concurrently. Code between the begin and end macro must not
      allocate any AFF4 memory or access any AFF4 objects.
   */
#define AFF4_BEGIN_ALLOW_THREADS {int _depth = CALL(aff4_gl_lock, allow_threads);
#define AFF4_END_ALLOW_THREADS   CALL(aff4_gl_lock, lock, _depth); };


/* This is a function that will be run when the library is imported. It
   should be used to initialise static stuff.

   It will only be called once. If you require some other subsystem to
   be called first you may call it from here.
*/
#define AFF4_MODULE_INIT(name) static int name ## _initialised = 0;     \
  static inline void name ## _init_2();                                 \
  void name ## _init() {                                                \
    if(name ## _initialised) return;                                    \
    name ## _initialised = 1;                                           \
    name ## _init_2();                                                  \
  };                                                                    \
  inline void name ## _init_2()

#define HASH_TABLE_SIZE 256
#define CACHE_SIZE 15

/** A cache is an object which automatically expires data which is
    least used - that is the data which is most used is put at the end
    of the list, and when memory pressure increases we expire data
    from the front of the list.
*/
PRIVATE enum Cache_policy {
  CACHE_EXPIRE_FIRST,
  CACHE_EXPIRE_LEAST_USED
};

/** The Cache is an object which manages a cache of Object instances
    indexed by a key.

    Each Object can either be in the Cache (in which case it is owned
    by this cache object and will not be freed) or out of the cache
    (in which case it is owned by NULL, and can be freed. When the
    cache gets too full it will start freeing objects it owns.

    Objects are cached with a key reference so this is like a python
    dict or a hash table, except that we can have several objects
    indexed with the same key. If you want to iterate over all the
    objects you need to use the iterator interface.

    NOTE: After putting the Object in the cache you do not own it -
    and you must not use it (because it might be freed at any time).
*/
PRIVATE CLASS(Cache, Object)
/* The key which is used to access the data */
     char *key;
     int key_len;

     /* An opaque data object and its length. The object will be
        talloc_stealed into the cache object as we will be manging its
        memory.
     */
     Object data;

     /* Cache objects are put into two lists - the cache_list contains
        all the cache objects currently managed by us in order of
        least used to most used at the tail of the list. The same
        objects are also present on one of the hash lists which hang
        off the respective hash table. The hash_list should be shorter
        to search linearly as it only contains objects with the same
        hash.
     */
     struct list_head cache_list;
     struct list_head hash_list;

     /* This is a pointer to the head of the cache */
     struct Cache_t *cache_head;
     enum Cache_policy policy;

     /* The current number of objects managed by this cache */
     int cache_size;

     /* The maximum number of objects which should be managed */
     int max_cache_size;

     /* A hash table of the keys */
     int hash_table_width;
     Cache *hash_table;

     /* These functions can be tuned to manage the hash table. The
        default implementation assumes key is a null terminated
        string.
     */
     int METHOD(Cache, hash, char *key, int len);
     int METHOD(Cache, cmp, char *other, int len);

     /** hash_table_width is the width of the hash table.
         if max_cache_size is 0, we do not expire items.

         DEFAULT(hash_table_width) = 100;
         DEFAULT(max_cache_size) = 0;
     */
     Cache METHOD(Cache, Con, int hash_table_width, int max_cache_size);

     /* Return a cache object or NULL if its not there. The
        object is removed from the cache. Memory ownership is ctx.
     */
     Object METHOD(Cache, get, void *ctx, char *key, int len);

     /* Returns a reference to the object. The object is still owned
      by the cache. Note that this should only be used in caches which
      do not expire objects or if the cache is locked, otherwise the
      borrowed reference may disappear unexpectadly. References may be
      freed when other items are added.
     */
     BORROWED Object METHOD(Cache, borrow, char *key, int len);

     /* Store the key, data in a new Cache object. The key and data will be
        stolen.
     */
     BORROWED Cache METHOD(Cache, put, char *key, int len, Object data);

     /* Returns true if the object is in cache */
     int METHOD(Cache, present, char *key, int len);

     /* This returns an opaque reference to a cache iterator. Note:
        The cache must be locked the entire time between receiving the
        iterator and getting all the objects.
     */
     BORROWED Object METHOD(Cache, iter, char *key, int len);
     BORROWED Object METHOD(Cache, next, Object *iter);

     int METHOD(Cache, print_cache);
END_CLASS


struct RDFURN_t;
     /** A logger may be registered with the Resolver. Any objects
         obtained from the Resolver will then use the logger to send
         messages to the user.
     */
CLASS(Logger, Object)
     Logger METHOD(Logger, Con);
     /* The message sent to the logger is sent from a service (The
     source of the message), at a particular level (how critical it
     is). It talks about a subject (usually the URN of the subject),
     and a message about it.
     */
     void METHOD(Logger, message, int level,\
                 char *service, struct RDFURN_t *subject, char *message);
END_CLASS

PROXY_CLASS(Logger);


/** Global logger */
extern Logger AFF4_LOGGER;


#define RESOLVER (((AFFObject)self)->resolver)

#include "queue.h"

/* A generic thread pool implementation. */
CLASS(ThreadPoolJob, Object)
  /* The thread which is running this job. */
  pthread_t thread_id;

  /* This actual function will be run in another thread. */
  void METHOD(ThreadPoolJob, run);

  /* This function will be run in the main thread when the pool calls
     complete().
  */
  int METHOD(ThreadPoolJob, complete);
END_CLASS


struct Queue_t;

CLASS(ThreadPool, Object)
    struct Queue_t *jobs;
    struct Queue_t *completed_jobs;

    int number_of_threads;
    pthread_t *threads;

    // This can be set to False to cause all workers to quit.
    int active;

    ThreadPool METHOD(ThreadPool, Con, int number);

    /* Schedule the job on the thread pool. Timeout is the number of
       seconds we are prepared to wait to be scheduled.  Returns value
       is 1 for success and 0 for failure to schedule the task.
    */
    int METHOD(ThreadPool, schedule, ThreadPoolJob job, int timeout);

    /* Can be called regularly by the main thread to complete
       any outstanding threads.
    */
    int METHOD(ThreadPool, complete);

    /* Terminate and Join all the workers. */
    void METHOD(ThreadPool, join);

END_CLASS




#endif 	    /* !AFF4_UTILS_H_ */
