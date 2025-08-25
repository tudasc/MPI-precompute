#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
  int value;
  pthread_mutex_t lock;
} SharedCounter;

void* increment(void* arg) {
  SharedCounter* counter = (SharedCounter*)arg;
  for (int i = 0; i < 1000000; i++) {
    pthread_mutex_lock(&counter->lock);
    counter->value++;  // protected access
    pthread_mutex_unlock(&counter->lock);
  }
  return NULL;
}

int main() {
  pthread_t t1, t2;
  SharedCounter counter;

  counter.value = 0;
  pthread_mutex_init(&counter.lock, NULL);

  pthread_create(&t1, NULL, increment, &counter);
  pthread_create(&t2, NULL, increment, &counter);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  printf("Final counter = %d\n", counter.value);

  pthread_mutex_destroy(&counter.lock);
  return 0;
}