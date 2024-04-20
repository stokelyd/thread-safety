#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/syscall.h>

#define NUM_THREADS 2
#define NUM_ITERATIONS 10000

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int x = 0;
int y = 0;
int x_from_thread = 0;

void* thread1(void* arg) {
    // printf("hello from thread 1\n");
    // long tid = syscall(__NR_gettid);
    // printf("I am thread1, tid: %ld\n", tid);


    pthread_mutex_lock(&mutex);     // acq(a)
    {   
        x = 1;                      // wr(x)
    }           
    pthread_mutex_unlock(&mutex);   // rel(a)

    x = 11;                         // wr(x)

    pthread_exit(NULL);
}

void* thread2(void* arg) {
    // printf("hello from thread 2\n");
    // long tid = syscall(__NR_gettid);
    // printf("I am thread2, tid: %ld\n", tid);
    int x_local;
    y = 2;                          // wr(y)

    pthread_mutex_lock(&mutex);     // acq(a)
    {
        x_local = x;                // rd(x)
    }
    pthread_mutex_unlock(&mutex);   // rel(a)

    x_from_thread = x_local;
    pthread_exit(NULL);
}

void run_n_tests(int numTests) {
    for (int i = 0; i < numTests; ++i) {
        x = 0;
        y = 0;
        x_from_thread = 0;

        pthread_t threads[2];

        if (pthread_create(&threads[1], NULL, thread1, NULL)) {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }

        if (pthread_create(&threads[0], NULL, thread2, NULL)) {
            fprintf(stderr, "Error creating thread\n");
            exit(1);
        }

        // fprintf("tid1: %d", threads[0]);
        // fprintf("tid2: %d", threads[1]);
        
        if (pthread_join(threads[0], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            exit(1);
        }

        if (pthread_join(threads[1], NULL)) {
            fprintf(stderr, "Error joining thread\n");
            exit(1);
        }
        
        // printf("Value of x from thread: %d\n", x_from_thread);
    }
}


int main()
{
    // run_n_tests(100);
    run_n_tests(5000);

    return 0;
}