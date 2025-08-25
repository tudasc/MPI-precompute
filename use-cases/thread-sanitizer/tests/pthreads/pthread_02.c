#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void* increment(void* arg) {
  int* counter = (int*)arg;  // shared pointer
  for (int i = 0; i < 1000000; i++) {
    (*counter)++;  // data race here!
  }
  return NULL;
}

int main() {
  pthread_t t1, t2;
  int* counter = malloc(sizeof(int));
  *counter = 0;

  pthread_create(&t1, NULL, increment, counter);
  pthread_create(&t2, NULL, increment, counter);

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  printf("Final counter = %d\n", *counter);

  free(counter);
  return 0;
}
