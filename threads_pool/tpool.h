#ifndef __TPOOL_H__
#define __TPOOL_H__

#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

typedef void (*thread_func_t)(void* arg);

typedef struct tpool_work {
    thread_func_t func;
    void* arg;
    struct tpool_work* next;
} tpool_work_t;

typedef struct tpool {
    tpool_work_t* work_first;
    tpool_work_t* work_last;
    pthread_mutex_t work_mutex;
    pthread_cond_t work_cond;
    pthread_cond_t working_cond;
    size_t working_cnt;
    size_t thread_cnt;
    bool stop;
} tpool_t;

// create a threads pool
tpool_t* tpool_create(size_t num);
// destroy a threads pool
void tpool_destroy(tpool_t* tm);

// add a work to the queue
bool tpool_add_work(tpool_t* tm, thread_func_t func, void* arg);

void tpool_wait(tpool_t* tm);





#endif // __TPOOL_H__