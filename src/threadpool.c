#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "threadpool.h"
#include "apc-internal.h"
#include "queue.h"

static pthread_mutex_t mutex;
static pthread_cond_t cond;
static pthread_t threads[4];
static unsigned int idle_threads = 0;
static unsigned int nthreads = 0;
static int running = 0;
static queue wq;
static queue exit_sig;

unsigned int nr[4] = {1, 2, 3, 4};

int tpool_running(){
    return running;
}

int tpool_add_work(apc_work_req *req){
    pthread_mutex_lock(&mutex);
    QUEUE_ADD_TAIL(&wq, get_queue(&req->work_queue));
    if(idle_threads > 0){
        pthread_cond_signal(&cond);
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

/* core function of every thread */
static void *thread_routine(/* void *arg */){
    char *buf = "";
    size_t len = 1;
    // unsigned int nr = *((unsigned int *) arg);
    while(1){
        pthread_mutex_lock(&mutex);
        while(QUEUE_EMPTY(&wq)){
            idle_threads += 1;
            pthread_cond_wait(&cond, &mutex);
            idle_threads -= 1;
        }

        queue *q = QUEUE_NEXT(&wq);
        if(q == &exit_sig){
            idle_threads += 1;
            pthread_mutex_unlock(&mutex);
            pthread_cond_signal(&cond);
            break;
        }

        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        pthread_mutex_unlock(&mutex);
        apc_work_req *req = container_of(q, apc_work_req, work_queue);
        req->work(req);

        pthread_mutex_lock(&req->loop->workmtx);
        QUEUE_ADD_TAIL(get_queue(&req->loop->work_queue), get_queue(&req->work_queue));
        pthread_mutex_unlock(&req->loop->workmtx);

        write(req->loop->wakeup_fd, buf, len);
    }
    return NULL;
}

int tpool_start(){
    if(nthreads == 0){
        return -1;
    }
    
    pthread_mutex_lock(&mutex);
    for(unsigned int i = 0; i<nthreads; i++){
        if(pthread_create(&threads[i], NULL, &thread_routine, (void *) &nr[i]) != 0){
            return -1;
        }
    }
    running = 1;
    pthread_mutex_unlock(&mutex);
    return 0;
}

int tpool_init(){
    nthreads = 4;
    idle_threads = 0;

    if(pthread_mutex_init(&mutex, NULL) != 0){
        return -1;
    }

    if(pthread_cond_init(&cond, NULL) != 0){
        return -1;
    }

    QUEUE_INIT(&wq);
    QUEUE_INIT(&exit_sig);
    return 0;
}

int tpool_cleanup(){    
    pthread_mutex_lock(&mutex);
    QUEUE_ADD_TAIL(&wq, &exit_sig);
    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond);
    /* wait for all threads to finish up */
    pthread_mutex_lock(&mutex);
    while(idle_threads != nthreads){
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    /* join and clean up each thread */
    for(unsigned int i = 0; i<nthreads; i++){
        pthread_join(threads[i], NULL);
    }

    /* clean up mutexes and conditons */
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    return 0;
}