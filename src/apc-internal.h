#ifndef APC_INTERNAL_HEADER
#define APC_INTERNAL_HEADER

#include <stddef.h>
#include <stdio.h>

#include "apc.h"
#include "queue.h"
#include "heap.h"

#define container_of(ptr, type, member) ((type *)  ((char *) ptr - offsetof(type, member)))
#define get_queue(qs) ((queue *) (qs))
#define get_node(timer) ((heap_node *) &(timer)->node)
#define get_heap(loop) ((heap *) &(loop)->timerheap)

#define HANDLE_ACTIVE   1
#define HANDLE_WRITABLE 2
#define HANDLE_READABLE 4

#ifdef DEBUG
    #define DEBUG_MSG(msg, ...) do{fprintf(stdout,"%s:%d:%s(): " msg, __FILE__, __LINE__, __func__, ##__VA_ARGS__);}while(0)
#else
    #define DEBUG_MSG(msg, ...)
#endif

#define apc_register_handle_(handle, loop)                                                      \
    do{                                                                                         \
        if((handle)->flags & HANDLE_ACTIVE){                                                    \
            break;                                                                              \
        }                                                                                       \
        (loop)->active_handles += 1;                                                            \
        (handle)->flags |= HANDLE_ACTIVE;                                                       \
    }while(0)                                                                                   \

#define apc_deregister_handle_(handle, loop)                                                    \
    do{                                                                                         \
        if(!((handle)->flags & HANDLE_ACTIVE)){                                                 \
            break;                                                                              \
        }                                                                                       \
        assert((loop)->active_handles > 0);                                                     \
        (loop)->active_handles -= 1;                                                            \
        (handle)->flags &= ~HANDLE_ACTIVE;                                                      \
    }while(0)                                                                                   \

#define apc_register_request_(request, loop)                                                    \
    do{                                                                                         \
        (loop)->active_requests += 1;                                                           \
    }while(0)                                                                                   \

#define apc_deregister_request_(handle, loop)                                                   \
    do{                                                                                         \
        assert((loop)->active_requests > 0);                                                    \
        (loop)->active_requests -= 1;                                                           \
    }while(0)                                                                                   \
    

#define apc_handle_init_(handle, loop_, type_)                                                  \
    do{                                                                                         \
        (handle)->data = NULL;                                                                  \
        (handle)->loop = loop_;                                                                 \
        (handle)->type = type_;                                                                 \
        (handle)->flags = 0;                                                                    \
        QUEUE_ADD_TAIL(get_queue(&(loop)->handle_queue), get_queue(&(handle)->handle_queue));   \
    }while(0)                                                                                   \

#define apc_handle_close_(handle)                                                               \
    do{                                                                                         \
        apc_deregister_handle_(handle, (handle)->loop);                                         \
        (handle)->data = NULL;                                                                  \
        (handle)->loop = NULL;                                                                  \
        (handle)->flags = 0;                                                                    \
        QUEUE_REMOVE(get_queue(&(handle)->handle_queue));                                       \
    }while(0)                                                                                   \
    

#define apc_request_init_(req, type_)                                                           \
    do{                                                                                         \
        (req)->data = NULL;                                                                     \
        (req)->type = type_;                                                                    \
    }while(0)                                                                                   \


#define HANDLE_FIELDS                                                                           \
    void *data;                                                                                 \
    apc_loop *loop;                                                                             \
    apc_handle_type type;                                                                       \
    queue handle_queue;                                                                         \
    unsigned int flags;                                                                         \

#define NETWORK_FIELDS                                                                          \
    fd_watcher watcher;                                                                         \
    apc_alloc alloc;                                                                            \
    apc_on_read on_read;                                                                        \
    queue write_queue;                                                                          \
    size_t write_queue_size;                                                                    \
    queue write_done_queue;                                                                     \


typedef struct apc_net_ apc_net;

struct apc_net_{
    HANDLE_FIELDS
    NETWORK_FIELDS
};

#undef HANDLE_FIELDS
#undef NETWORK_FIELDS

#endif