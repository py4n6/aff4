#include "queue.h"
#include <time.h>
#include <pthread.h>
#include <sys/time.h>

static Queue Queue_Con(Queue self, int maxsize) {
  INIT_LIST_HEAD(&self->items);
  self->maxsize = maxsize;
  self->queue_size = 0;

  pthread_mutex_init(&self->mutex, NULL);
  pthread_cond_init(&self->not_empty, NULL);
  pthread_cond_init(&self->not_full, NULL);
  pthread_cond_init(&self->all_tasks_done, NULL);

  return self;
};


static void *Queue_get(Queue self, void *ctx, int timeout) {
  QueueItem item;
  void *result = NULL;
  struct timeval now;
  struct timespec deadline;

  pthread_mutex_lock(&self->mutex);
  gettimeofday(&now, NULL);
  deadline.tv_sec = now.tv_sec + timeout / 1000000;
  deadline.tv_nsec = (now.tv_usec + timeout % 1000000) * 1000;

  // If there is nothing available we just wait for something new:
  if(self->queue_size==0) {
    if(pthread_cond_timedwait(&self->not_empty,
                              &self->mutex, &deadline)) {
      goto exit;
    };
  };

  list_next(item, &self->items, list);
  list_del(&item->list);
  result = item->data;
  talloc_steal(ctx, result);
  talloc_free(item);

  // Let waiters know there is a new item in the queue
  pthread_cond_signal(&self->not_full);
  self->queue_size --;
  if(self->queue_size <= 0) {
    pthread_cond_signal(&self->all_tasks_done);
  };

 exit:
  pthread_mutex_unlock(&self->mutex);
  return result;
};

static int Queue_put(Queue self, void *data, int timeout) {
  QueueItem item;
  struct timeval now;
  struct timespec deadline;

  pthread_mutex_lock(&self->mutex);

  gettimeofday(&now, NULL);
  deadline.tv_sec = now.tv_sec + timeout / 1000000;
  deadline.tv_nsec = (now.tv_usec + timeout % 1000000) * 1000;

  // If there is nothing available we just wait for something new:
  if(self->queue_size >= self->maxsize) {
    int res = pthread_cond_timedwait(&self->not_full,
                                     &self->mutex, &deadline);

    // Failed to lock.
    if(res) {
      pthread_mutex_unlock(&self->mutex);
      return 0;
    };
  };

  item = talloc_size(self, sizeof(struct QueueItem_t));
  item->data = data;
  talloc_steal(item, data);
  list_add_tail(&item->list, &self->items);

  self->queue_size ++;

  // Let waiters know there is a new item in the queue
  pthread_cond_signal(&self->not_empty);

  pthread_mutex_unlock(&self->mutex);
  return 1;
};

/** Search for data in the queue and remove it */
static void *Queue_remove(Queue self, void *ctx, void *data) {
  QueueItem i;
  void *result = NULL;

  pthread_mutex_lock(&self->mutex);
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
  pthread_mutex_unlock(&self->mutex);
  return result;
};

VIRTUAL(Queue, Object)
     VMETHOD_BASE(Queue, Con) = Queue_Con;
     VMETHOD_BASE(Queue, get) = Queue_get;
     VMETHOD_BASE(Queue, put) = Queue_put;
     VMETHOD_BASE(Queue, remove) = Queue_remove;
END_VIRTUAL
