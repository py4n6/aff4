#include "queue.h"
#include <time.h>
#include <pthread.h>
#include <sys/time.h>

static Queue Queue_Con(Queue self, int maxsize) {
  INIT_LIST_HEAD(&self->items);
  self->maxsize = maxsize;
  self->queue_size = 0;

  pthread_cond_init(&self->not_empty, NULL);
  pthread_cond_init(&self->not_full, NULL);
  pthread_cond_init(&self->all_tasks_done, NULL);

  return self;
};


static void *Queue_get(Queue self, int timeout) {
  QueueItem item;
  void *result = NULL;
  int res;

  AFF4_GL_LOCK;

  // If there is nothing available we just wait for something new:
  while(self->queue_size==0) {
    res = CALL(aff4_gl_lock, timedwait, &self->not_empty, timeout);
    if(res) {
      goto exit;
    };
  };

  list_next(item, &self->items, list);
  list_del(&item->list);
  result = item->data;
  talloc_free(item);

  // Let waiters know there is a new item in the queue
  pthread_cond_signal(&self->not_full);
  self->queue_size --;
  if(self->queue_size <= 0) {
    pthread_cond_signal(&self->all_tasks_done);
  };

 exit:
  AFF4_GL_UNLOCK;
  return result;
};

static int Queue_put(Queue self, void *data, int timeout) {
  QueueItem item;

  AFF4_GL_LOCK;

  // If there is nothing available we just wait for something new:
  if(self->queue_size >= self->maxsize) {
    int res = CALL(aff4_gl_lock, timedwait, &self->not_full, timeout);

    // Failed to lock.
    if(res) {
      AFF4_GL_UNLOCK;
      return 0;
    };
  };

  item = talloc_size(self, sizeof(struct QueueItem_t));
  item->data = data;

  /* We now own the data */
  talloc_steal(self, data);
  list_add_tail(&item->list, &self->items);

  self->queue_size ++;

  // Let waiters know there is a new item in the queue
  pthread_cond_signal(&self->not_empty);

  AFF4_GL_UNLOCK;

  return 1;
};

/** Search for data in the queue and remove it */
static void *Queue_remove(Queue self, void *ctx, void *data) {
  QueueItem i;
  void *result = NULL;

  AFF4_GL_LOCK;

  list_for_each_entry(i, &self->items, list){ 
    if(i->data == data) {
      result = i->data;
      list_del(&i->list);
      talloc_steal(ctx, data);
      talloc_free(i);
      goto exit;
    };
  }

 exit:
  AFF4_GL_UNLOCK;
  return result;
};

VIRTUAL(Queue, Object)
     VMETHOD_BASE(Queue, Con) = Queue_Con;
     VMETHOD_BASE(Queue, get) = Queue_get;
     VMETHOD_BASE(Queue, put) = Queue_put;
     VMETHOD_BASE(Queue, remove) = Queue_remove;
END_VIRTUAL
