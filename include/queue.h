/** This is an implementation of a queue */
#include "class.h"
#include <pthread.h>
#include "list.h"

typedef struct QueueItem_t {
  struct list_head list;
  void *data;
} *QueueItem;

CLASS(Queue, Object)
     /* A list of QueueItem waiting to be fetched. */
     struct list_head items;

     // This condition variable is waited on for clients which want to
     // get Queue objects
     int maxsize;
     int queue_size;

     pthread_mutex_t mutex;
     pthread_cond_t not_empty;
     pthread_cond_t not_full;
     pthread_cond_t all_tasks_done;

     Queue METHOD(Queue, Con, int maxsize);

     /* Gets the first object in the queue - or blocks until an object
        appears on the queue. The object is stolen to belong to the
        new context. If timeout is exceeded, returns NULL. Timeout is
        given in microseconds.
     */
     void *METHOD(Queue, get, void *context, int timeout);

     /* Add a new object to the end of the queue. Returns 1 for
        success and 0 for timeout exceeded. Timeout is given in
        microseconds.
     */
     int METHOD(Queue, put, void *data, int timeout);

     // Remove data from the queue (wherever it is)
     void *METHOD(Queue, remove, void *ctx, void *data);

     // Blocks until all items in the queue were removed.
     void METHOD(Queue, join);
END_CLASS
