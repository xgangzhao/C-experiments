#include "tpool.h"

static tpool_work_t* tpool_work_create(thread_func_t func, void* arg) {
    if (func == NULL) {
        return NULL;
    }
    tpool_work_t* work = NULL;

    work = (tpool_work_t*)malloc(sizeof(*work));
    work->func = func;
    work->arg = arg;
    work->next = NULL;

    return work;
}

static void tpool_work_destroy(tpool_work_t* work) {
    if (work != NULL) {
        free(work);
        work = NULL;
    }
}

static tpool_work_t* tpool_work_get(tpool_t* tm) {
    if (tm == NULL || tm->work_first == NULL) {
        return NULL;
    }
    tpool_work_t* work = tm->work_first;
    tm->work_first = work->next;
    if (work->next == NULL) {
        tm->work_last  == NULL;
    }

    return work;
}

static void* tpool_worker(void* arg) {
    tpool_t* tm = (tpool_t*)arg;
    tpool_work_t* work = NULL;

    while (1) {
        pthread_mutex_lock(&(tm->work_mutex));

        while (tm->work_first == NULL && !tm->stop) {
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));
        }

        if (tm->stop) {
            break;
        }

        work = tpool_work_get(tm);
        tm->working_cnt++;
        pthread_mutex_unlock(&(tm->work_mutex));

        if (work) {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;
        if (!tm->stop && tm->working_cnt == 0 && tm->work_first == NULL) {
            pthread_cond_signal(&(tm->working_cond));
        }
        pthread_mutex_unlock(&(tm->work_mutex));
    }

    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return NULL;
}

tpool_t* tpool_create(size_t num) {
    tpool_t* tm = NULL;
    pthread_t thread = 0;

    if (num == 0) {
        num = 2;
    }

    tm = (tpool_t*)calloc(1, sizeof(*tm));
    tm->thread_cnt = num;
    tm->work_first = NULL;
    tm->work_last  = NULL;

    pthread_mutex_init(&(tm->work_mutex), NULL);
    pthread_cond_init(&(tm->work_cond), NULL);
    pthread_cond_init(&(tm->working_cond), NULL);

    for (size_t i = 0; i < num; ++i) {
        pthread_create(&thread, NULL, tpool_worker, tm);
        pthread_detach(thread);
    }

    return tm;
}

void tpool_destroy(tpool_t* tm) {
    if (!tm) return;

    tpool_work_t* work;
    tpool_work_t* work2;

    pthread_mutex_lock(&(tm->work_mutex));
    // destroy all works
    work = tm->work_first;
    while (work) {
        work2 = work->next;
        tpool_work_destroy(work);
        work = work2;
    }
    tm->work_first = NULL;
    // tell the pool it's time to stop
    tm->stop = true;
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    // waiting the processing threads to finish
    tpool_wait(tm);
    // destory all mutex and COND
    pthread_mutex_destroy(&(tm->work_mutex));
    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));

    free(tm);
}

void tpool_wait(tpool_t* tm) {
    if (!tm) return;

    pthread_mutex_lock(&(tm->work_mutex));
    while (1) {
        if (tm->work_first != NULL || 
            (!tm->stop && tm->working_cnt != 0) ||
            (tm->stop && tm->thread_cnt != 0)) {
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tm->work_mutex));
}

bool tpool_add_work(tpool_t* tm, thread_func_t func, void* arg) {
    if (!tm) return false;

    tpool_work_t* work = tpool_work_create(func, arg);
    if (!work) return false;

    pthread_mutex_lock(&(tm->work_mutex));
    if (tm->work_first) {
        tm->work_last->next = work;
        tm->work_last = work;
    } else {
        tm->work_first = work;
        tm->work_last = tm->work_first;
    }
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return true;
}