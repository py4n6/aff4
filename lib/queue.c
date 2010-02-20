#include "queue.h"

Queue Queue_Con(Queue self) {
  INIT_LIST_HEAD(&self->list);

  // this will be locked when we are busy
  pthread_mutex_init(&self->mutex, NULL);
  pthread_cond_init(&self->condition, NULL);

  self->data = NULL;

  return self;
};


void *Queue_get(Queue self, void *ctx) {
  Queue item;
  void *result;

  pthread_mutex_lock(&self->mutex);
  // If there is nothing available we just wait for something new:
  while(list_empty(&self->list)) {
    pthread_cond_wait( &self->condition, &self->mutex);
  };

  list_next(item, &self->list, list);
  list_del(&item->list);
  result = item->data;
  talloc_steal(ctx, result);
  talloc_free(item);

  pthread_mutex_unlock(&self->mutex);
  return result;
};

void Queue_put(Queue self, void *data) {
  Queue item = talloc_size(self, sizeof(__Queue));

  pthread_mutex_lock(&self->mutex);
  item->data = data;
  talloc_steal(self, data);
  list_add_tail(&item->list, &self->list);

  // Let waiters know there is a new item in the queue
  pthread_cond_signal(&self->condition);

  pthread_mutex_unlock(&self->mutex);
};

/** Search for data in the queue and remove it */
void *Queue_remove(Queue self, void *ctx, void *data) {
  Queue i;

  pthread_mutex_lock(&self->mutex);
  list_for_each_entry(i, &self->list, list){ 
    if(i->data == data) {
      void *result = i->data;
      list_del(&i->list);
      talloc_steal(ctx, data);
      talloc_free(i);

      pthread_mutex_unlock(&self->mutex);
      return result;
    };
  }

  pthread_mutex_unlock(&self->mutex);
  return NULL;
};

VIRTUAL(Queue, Object)
     VMETHOD_BASE(Queue, Con) = Queue_Con;
     VMETHOD_BASE(Queue, get) = Queue_get;
     VMETHOD_BASE(Queue, put) = Queue_put;
     VMETHOD_BASE(Queue, remove) = Queue_remove;
END_VIRTUAL
