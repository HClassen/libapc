#include <assert.h>
#include <limits.h>

#include "../apc.h"
#include "../internal.h"
#include "../common/queue.h"
#include "../common/heap.h"

int apc_timer_init(apc_loop *loop, apc_timer *timer){
    assert(loop != NULL);
    assert(timer != NULL);

    apc_handle_init_(timer, loop, APC_TIMER);
    timer->duration = -1;
    timer->end = -1;
    timer->restart = 0;
    timer->cb = NULL;
    return 0;
}

int apc_timer_start(apc_timer *timer, time_t duration, int restart, apc_on_timeout cb){
    assert(timer != NULL);
    assert(cb != NULL);
    assert(duration >= 0);
    assert(restart == 0 || restart == 1);

    timer->duration = duration;
    timer->end = timer->loop->now + duration;
    if(timer->end < timer->loop->now){
        timer->end = LONG_MAX;
    }
    timer->restart = restart;
    timer->cb = cb;
    timer->id = timer->loop->timerid;
    timer->loop->timerid += 1;
    heap_insert(get_heap(timer->loop), get_node(timer));
    apc_register_handle_(timer, timer->loop);
    return 0;
}

int apc_timer_stop(apc_timer *timer){
    assert(timer != NULL);
    if(!timer->flags & HANDLE_ACTIVE){
        return 0;
    }
    
    heap_remove(get_heap(timer->loop), get_node(timer));
    apc_deregister_handle_(timer, timer->loop);
    return 0;
}

int apc_timer_restart(apc_timer *timer){
    if(timer->cb == NULL){
        return APC_EINVAL;
    }

    apc_timer_stop(timer);
    apc_timer_start(timer, timer->duration, timer->restart, timer->cb);
    return 0;
}

void apc_timer_close(apc_timer *timer){
    apc_timer_stop(timer);
    timer->duration = -1;
    timer->end = -1;
    timer->restart = 0;
    timer->cb = NULL;
}