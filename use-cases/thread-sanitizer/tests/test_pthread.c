#include <stdio.h>
#include <pthread.h>

int counter = 0;  // shared variable (not protected)

void* increment(void* arg) {
  for (int i = 0; i < 1000000; i++) {
    counter++;  // data race here!
  }
  return NULL;
}

int main() {
  pthread_t t1, t2;

  // create two threads
  pthread_create(&t1, NULL, increment, NULL);
  pthread_create(&t2, NULL, increment, NULL);

  // wait for them to finish
  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  // expected result: 2,000,000
  printf("Final counter = %d\n", counter);

  return 0;
}