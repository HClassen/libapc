#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>

#include "apc.h"
#include "apc-internal.h"

#define NR_EPOLL_EVENTS 1024

#define update_timeout_(loop, base, timeout)    \
    assert((timeout) > 0);                      \
    (timeout) -= (loop)->now - (base);          \
    if((timeout) <= 0){                         \
        return;                                 \
    }                                           \
    continue;                                   \

int create_backend_fd(){
	int epoll_fd = epoll_create1(O_CLOEXEC);
	if (epoll_fd < 0){
		return -1;
	}

	return epoll_fd;
}

void fd_watcher_poll(apc_loop *loop, int timeout){
    int nevents;
    //int count;

    if(loop->nfds == 0){
        assert(QUEUE_EMPTY(&loop->watcher_queue));
        return;
    }
    
    /* register all watchers */
    queue *q;
    fd_watcher* w;
    struct epoll_event e = (struct epoll_event) {0};
    int op;
    while(!QUEUE_EMPTY(get_queue(&loop->watcher_queue))){
        q = QUEUE_NEXT(get_queue(&loop->watcher_queue));
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        w = container_of(q, fd_watcher, watcher_queue);
        assert(w->fd >= 0);
        assert(w->events != 0);
        assert(w->fd < (int) loop->nwatchers);

        e.events = w->events;
        e.data.fd = w->fd;

        if(w->registered == 0){
            op = EPOLL_CTL_ADD;
        }else{
            op = EPOLL_CTL_MOD;
        }

        if(epoll_ctl(loop->backend_fd, op, w->fd, &e)){
            if(errno != EEXIST){
                abort();
            }

            assert(op == EPOLL_CTL_ADD);

            if (epoll_ctl(loop->backend_fd, EPOLL_CTL_MOD, w->fd, &e)){
                abort();
            }
        }

        w->registered = 1;
    }
    
    time_t base = loop->now;
    struct epoll_event events[NR_EPOLL_EVENTS];
    while(1){
        int nfds = epoll_wait(loop->backend_fd, events, NR_EPOLL_EVENTS, timeout);

        loop->now = time(NULL);
        if (nfds == 0) {
            assert(timeout != -1);

            if (timeout == 0){
                return;
            }
            
            update_timeout_(loop, base, timeout)
            // goto update_timeout;
        }

        if(nfds == -1){
            if(errno != EINTR){
                abort();
            }

            if(timeout == -1){
                continue;
            }

            if(timeout == 0){
                return;
            }

            update_timeout_(loop, base, timeout)
            // goto update_timeout;
        }

        nevents = 0;

        assert(loop->watchers != NULL);
/*         loop->watchers[loop->nwatchers] = (void*) events;
        loop->watchers[loop->nwatchers + 1] = (void*) (uintptr_t) nfds; */

        for(int i = 0; i < nfds; i++){
            struct epoll_event *pe = events + i;
            int fd = pe->data.fd;

            if(fd == -1){
                continue;
            }

            assert(fd >= 0);
            assert((unsigned) fd < loop->nwatchers);

            w = loop->watchers[fd];
            /* remove fd we've stopped watching */
            if(w == NULL){
                epoll_ctl(loop->backend_fd, EPOLL_CTL_DEL, fd, pe);
                continue;
            }

            pe->events &= w->events | POLLERR | POLLHUP;

            if(pe->events != 0){
                w->cb(loop, w, pe->events);
            }

            nevents += 1;
        }

        if(nevents != 0){
            if(nfds == NR_EPOLL_EVENTS){
                timeout = 0;
                continue;
            }

            return;
        }

        if(timeout == 0){
            return;
        }

        if(timeout == -1){
            continue;
        }
    }
}

void invalidate_fd(apc_loop *loop, int fd){
    assert(loop->watchers != NULL);
    assert(fd >= 0);

/*     struct epoll_event *events = (struct epoll_event*) loop->watchers[loop->nwatchers];
    uintptr_t nfds = (uintptr_t) loop->watchers[loop->nwatchers + 1];

    if(events != NULL){
       for(uintptr_t i = 0; i < nfds; i++){
           if(events[i].data.fd == fd){
               events[i].data.fd = -1;
           }
       }
    } */

    if(loop->backend_fd >= 0){
        struct epoll_event dummy;
        memset(&dummy, 0, sizeof(dummy));
        epoll_ctl(loop->backend_fd, EPOLL_CTL_DEL, fd, &dummy);
    }
}