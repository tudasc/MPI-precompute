#include <omp.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
  int array[2] = {0,0};

#pragma omp parallel num_threads(2)
  {

    printf("Hello World... from thread = %d\n",
           omp_get_thread_num());
    // race free
    array[omp_get_thread_num()]++;
    // with race
    //array[0]++;

  }

  printf("%d , %d\n",array[0],array[1]);
  return 0;
  }
