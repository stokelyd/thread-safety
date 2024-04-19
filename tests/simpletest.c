#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/syscall.h>

// #ifndef N
// #  warning "N was not defined"
// #  define N 5
// #endif

#define NUM_THREADS 2
#define NUM_ITERATIONS 10000

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int x = 1;
int y = 0;
int z = 0;
int x_from_thread = 0;

pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

// nidhugg/benchmarks/from_TRACER

// shared variables
// atomic_int x;

// void *writer(void *arg) {
//     atomic_store_explicit(&x, 1, memory_order_seq_cst);
//     return NULL;
// }

// void *reader(void *arg) {
//     atomic_load_explicit(&x, memory_order_seq_cst);
//     return NULL;
// }

void* thread1(void* arg) {
    printf("hello from thread 1\n");

    // long tid = syscall(__NR_gettid);
    // printf("I am thread1, tid: %ld\n", tid);

    int x_local;
    x_local = x;                    // rd(x)
    pthread_mutex_lock(&mutex);     // acq(m)
    {   
        y = 1;                      // wr(y)
    }           
    pthread_mutex_unlock(&mutex);   // rel(m)

    x_from_thread = x_local;

    // test multiple
    pthread_mutex_lock(&mutex2);     // acq(m)
    {   

    }           
    pthread_mutex_unlock(&mutex2);   // rel(m)

    pthread_exit(NULL);
}

void* thread2(void* arg) {
    printf("hello from thread 2\n");
    // long tid = syscall(__NR_gettid);
    // printf("I am thread2, tid: %ld\n", tid);

    int z_local;
    pthread_mutex_lock(&mutex);     // acq(m)
    {
        z_local = z;                // rd(z)
    }
    pthread_mutex_unlock(&mutex);   // rel(m)
    x = 2;                          // wr(x)

    pthread_exit(NULL);
}

void run_n_tests(int numTests) {
    for (int i = 0; i < numTests; ++i) {
        x = 1;
        y = 0;
        z = 0;
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
        
        printf("Value of x from thread: %d\n", x_from_thread);
    }
}


int main()
{
    // run_n_tests(100);
    run_n_tests(1);

    return 0;
}