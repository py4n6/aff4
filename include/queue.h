/** This is an implementation of a queue */
#include "class.h"
#include <pthread.h>
#include "list.h"

CLASS(Queue, Object)
     struct list_head list;

     // This condition variable is waited on for clients which want to
     // get Queue objects
     pthread_mutex_t mutex;
     pthread_cond_t condition;

     // The queue object is stored here (it will be stolen)
     void *data;

     Queue METHOD(Queue, Con);

     // Gets the first object in the queue - or blocks until an object
     // appears on the queue. The object is stolen to belong to the
     // new context
     void *METHOD(Queue, get, void *context);

     // Add a new object to the end of the queue
     void METHOD(Queue, put, void *data);

     // Remove data from the queue (wherever it is)
     void *METHOD(Queue, remove, void *ctx, void *data);
END_CLASS
