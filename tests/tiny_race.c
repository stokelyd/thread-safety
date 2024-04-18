#include <pthread.h>
#include <stdio.h>

int Global;

void* Thread1(void* x) {
    Global = 42;
    return x;
}

int main() {
    pthread_t t;
    pthread_create(&t, NULL, Thread1, NULL);
    Global = 43;
    // printf("Global: %d\n", Global);
    pthread_join(t, NULL);

    printf("Global: %d\n", Global);
    return Global;
}