#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NTHREADS 4
#define ARRAY_SIZE 8

typedef struct {
  int id;
  int* array;
  pthread_barrier_t* barrier;
} ThreadData;

void* worker(void* arg) {
  ThreadData* data = (ThreadData*)arg;

  // Each thread fills two elements of the array
  int start = data->id * (ARRAY_SIZE / NTHREADS);
  int end   = start + (ARRAY_SIZE / NTHREADS);
  for (int i = start; i < end; i++) {
    data->array[i] = data->id;
  }

  printf("Thread %d finished filling\n", data->id);

  // Synchronize all threads here
  pthread_barrier_wait(data->barrier);

  // After barrier, all threads can safely read the whole array
  if (data->id == 0) {
    printf("Final array: ");
    for (int i = 0; i < ARRAY_SIZE; i++) {
      printf("%d ", data->array[i]);
    }
    printf("\n");
  }

  return NULL;
}

int main() {
  pthread_t threads[NTHREADS];
  ThreadData threadData[NTHREADS];
  int* array = malloc(sizeof(int) * ARRAY_SIZE);

  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, NULL, NTHREADS);

  for (int i = 0; i < NTHREADS; i++) {
    threadData[i].id = i;
    threadData[i].array = array;
    threadData[i].barrier = &barrier;
    pthread_create(&threads[i], NULL, worker, &threadData[i]);
  }

  for (int i = 0; i < NTHREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  pthread_barrier_destroy(&barrier);
  free(array);
  return 0;
}
