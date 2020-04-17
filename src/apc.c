#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include "apc.h"
#include "apc-internal.h"
#include "core.h"
#include "threadpool.h"
#include "net.h"
#include "fd.h"
#include "fs.h"    
#include "timer.h"
#include "heap.h"
#include "queue.h"
#include "reactor/reactor.h"

#define apc_loop_active_(loop)                                          \
    ((loop)->active_handles > 0 || (loop)->active_requests > 0)         \

int timer_cmp(heap_node *a, heap_node *b){
    apc_timer *t1 = container_of(a, apc_timer, node);
    apc_timer *t2 = container_of(b, apc_timer, node);

    if(t1->end < t2->end){
        return 1;
    }

    if(t2->end < t1->end){
        return 0;
    }

    return (t1->id < t2->id);
}

static int get_timeout(apc_loop *loop){
    if(!apc_loop_active_(loop)){
        return 0;
    }

    if(get_heap(loop)->head == NULL){
        return -1;
    }

    apc_timer *t = container_of(get_heap(loop)->head, apc_timer, node);
    if(t->end <= loop->now){
        return 0;
    }

    time_t diff = (t->end - loop->now) * 1000;
    diff = diff > INT_MAX ? INT_MAX : diff;
    return (int) diff;
}

int apc_loop_init(apc_loop *loop){
    assert(loop != NULL);

    QUEUE_INIT(&loop->handle_queue);
    QUEUE_INIT(&loop->work_queue);
    loop->active_handles = 0;
    loop->active_requests = 0;
    loop->now = time(NULL);
    loop->timerid = 0;
    heap_init((heap *) &loop->timerheap, timer_cmp);
    int err = apc_reactor_init(&loop->reactor);
    if(err == -1){
        err = APC_EFDPOLL;
        goto backenderr;
    }

    err = pthread_mutex_init(&loop->workmtx, NULL);
    if(err != 0){
        err = APC_ERROR;
        goto mtxerr;
    }

    int pipefds[2];
    err = fd_pipe(pipefds);
    if(err != 0){
        goto pipeerr;
    }
    apc_event_watcher_init(&loop->wakeup_watcher, wakeup_io, pipefds[0]);
    apc_event_watcher_register(&loop->reactor, &loop->wakeup_watcher, APC_POLLIN);
    loop->wakeup_fd = pipefds[1];
    err = tpool_init();
    if(err != 0){
        goto tpoolerr;
    }

    return 0;

tpoolerr:
    close(pipefds[0]);
    close(pipefds[1]);

pipeerr:
    pthread_mutex_destroy(&loop->workmtx);

mtxerr:
    apc_reactor_close(&loop->reactor);

backenderr:
    return err;
}

void apc_loop_run(apc_loop *loop){
    tpool_start();
    while(apc_loop_active_(loop)){
        loop->now = time(NULL);
        run_timers(loop);
        int timeout = get_timeout(loop);
        apc_reactor_poll(&loop->reactor, timeout);
    }
    
    tpool_cleanup();    
    while(!QUEUE_EMPTY(&loop->handle_queue)){
        queue *q = QUEUE_NEXT(&loop->handle_queue);
        apc_handle *handle = container_of(q, apc_handle, handle_queue);
        QUEUE_REMOVE(q);
        apc_close(handle);
    }
    pthread_mutex_destroy(&loop->workmtx);
    apc_event_watcher_close(&loop->reactor, &loop->wakeup_watcher);
    close(loop->wakeup_fd);
    close(loop->wakeup_watcher.fd);
    apc_reactor_close(&loop->reactor);
}

const char *apc_strerror(enum apc_error_code_ err){
#define X(e, str)                       \
    case (e):                           \
        return (str);                   \

    switch(err){
        X(success, "success")
        X(error, "APC_ERROR: a general error occured")
        X(inval, "APC_EINVAL: invalid argument passed")
        X(nomem, "APC_ENOMEM: failed to allocate memory")
        X(notsupported, "APC_ENOTSUPPORTED: functionality not supported by OS")
        X(handleclosed, "APC_EHANDLECLOSED: handle was already closed")
        X(unknownhandle, "APC_EUNKNOWNHANDLE: unknown handle type")
        X(busy, "APC_EBUSY: the handle is already used")
        X(invalidpath, "APC_EINVALIDPATH: invalid path")
        X(createpipe, "APC_ECREATEPIPE: failed to create internal pipe")
        X(sockbind, "APC_ESOCKBIND: failed to bind to port")
        X(sockaccept, "APC_ECONNACCEPT: failed to accept incoming connection")
        X(sockconnect, "APC_ECONNECT: failed to connect to remote host")
        X(fileopen, "APC_EFILEOPEN: failed to open file")
        X(fileexists, "APC_EFILEEXISTS: file already exists")
        X(fdpoll, "APC_EFDPOLL: failed to poll file descriptors")
        X(fdread, "APC_EFDREAD: failed to read from file descriptor")
        X(fdwrite, "APC_EFDWRITE: failed to write to file descriptor")
        X(fdflags, "APC_EFDFLAGS: failed to set flags on file descriptor")
        X(wouldblock, "APC_EWOULDBLOCK: operation would block")
        default:
            return "unknown error";
    }

#undef X
}

apc_buf apc_buf_init(void *base, size_t len){
    return (apc_buf) {base, len};
}

int apc_close(apc_handle *handle){
    assert(handle);
    switch(handle->type){
        case APC_UDP:
            udp_close((apc_udp *) handle);
            break;
        case APC_TCP:
            tcp_close((apc_tcp *) handle); 
            break;
        case APC_FILE:
            file_close((apc_file *) handle);
            break;
        case APC_TIMER:
            timer_close((apc_timer *) handle);
            break;
        default:
            return APC_EUNKNOWNHANDLE;
    }
    apc_handle_close_(handle);
    return 0;
}

int apc_add_work(apc_loop *loop, apc_work_req *req, apc_work work, apc_work done){
    assert(loop != NULL);
    assert(req != NULL);
    assert(work != NULL);

    apc_request_init_(req, APC_WORK);
    QUEUE_INIT(&req->work_queue);
    req->loop = loop;
    req->work = work;
    req->done = done;
    apc_register_request_(req, loop);
    apc_event_watcher_register(&loop->reactor, &loop->wakeup_watcher, APC_POLLIN);
    tpool_add_work(req);
    return 0;
}