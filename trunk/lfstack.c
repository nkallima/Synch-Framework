#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <omp.h>
#include <string.h>
#include <stdint.h>


#include "config.h"
#include "primitives.h"
#include "backoff.h"
#include "rand.h"
#include "pool.h"


#define POP                        -1

int MIN_BAK;
int MAX_BAK;


typedef struct Node{
    struct Node *next;
    Object value;
} Node;

// Shared variables
volatile Node *top = null;

// Each thread owns a private copy of the following variables
__thread PoolStruct pool;
__thread BackoffStruct backoff;


void SHARED_OBJECT_INIT(void) {
    top = null;
    FullFence();
}

inline static void push(ArgVal arg) {
    Node *n;

    n = alloc_obj(&pool);
#ifndef sparc
    reset_backoff(&backoff);
#endif
    n->value = arg;
    do {
        Node *old_top = (Node *)top;   // top is volatile
        n->next = old_top;
        if (CAS(&top, old_top, n) == true)
            break;
        else {
#ifndef sparc
            backoff_delay(&backoff);
#else
            sched_yield();
            sched_yield();
            sched_yield();
            sched_yield();
            sched_yield();
#endif
        }
    } while(true);
}

inline static RetVal pop(void) {
    reset_backoff(&backoff);
    do {
        Node *old_top = (Node *) top;
        if (old_top == null)
            return (RetVal)0;
        if(CAS(&top, old_top, old_top->next))
            return old_top->value;
        else backoff_delay(&backoff);
    } while (true) ;
}


pthread_barrier_t barr;
int64_t d1 CACHE_ALIGN, d2;

inline static void Execute(void* Arg) {
    long i;
    long id = (long) Arg;
    long rnum;
    volatile long j;

    simSRandom(id + 1);

    init_backoff(&backoff, MIN_BAK, MAX_BAK, 1);
    init_pool(&pool, sizeof(Node));
    if (id == N_THREADS - 1)
        d1 = getTimeMillis();
    // Synchronization point
    int rc = pthread_barrier_wait(&barr);
    if (rc != 0 && rc != PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("Could not wait on barrier\n");
        exit(-1);
    }

    for (i = 0; i < RUNS; i++) {
        push(id + 1);
        rnum = simRandomRange(1, MAX_WORK);
        for (j = 0; j < rnum; j++)
            ;
        pop();
        rnum = simRandomRange(1, MAX_WORK);
        for (j = 0; j < rnum; j++)
            ;
    }
}

inline static void* EntryPoint(void* Arg) {
    Execute(Arg);
    return null;
}

inline pthread_t StartThread(int arg) {
    long id = (long) arg;
    void *Arg = (void*) id;
    pthread_t thread_p;
    int thread_id;

    pthread_attr_t my_attr;
    pthread_attr_init(&my_attr);
    thread_id = pthread_create(&thread_p, &my_attr, EntryPoint, Arg);

    return thread_p;
}

int main(int argc, char *argv[]) {
    pthread_t threads[N_THREADS];
    int i;

    if (argc != 3) {
        fprintf(stderr, "Please set upper and lower bound for backoff!\n");
        exit(EXIT_SUCCESS);
    } else {
        sscanf(argv[1], "%d", &MIN_BAK);
        sscanf(argv[2], "%d", &MAX_BAK);
    }

    if (pthread_barrier_init(&barr, NULL, N_THREADS)) {
        printf("Could not create the barrier\n");
        return -1;
    }

    SHARED_OBJECT_INIT();
    for (i = 0; i < N_THREADS; i++)
        threads[i] = StartThread(i);

    for (i = 0; i < N_THREADS; i++)
        pthread_join(threads[i], NULL);
    d2 = getTimeMillis();

    printf("%d\n", (int) (d2 - d1));
    
#ifdef DEBUG
    int counter = 0;

    while(top != null) {
        counter++;
        top = top->next;
    }

    fprintf(stderr, "%d nodes were left in the stack!\n", counter);
#endif

    if (pthread_barrier_destroy(&barr)) {
        printf("Could not destroy the barrier\n");
        return -1;
    }
    return 0;
}
