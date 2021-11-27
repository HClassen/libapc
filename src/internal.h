#ifndef APC_INTERNAL_HEADER
#define APC_INTERNAL_HEADER

#include <stddef.h>
#include <assert.h>

#include "apc.h"
#include "common/queue.h"

#ifdef DEBUG
    #include <stdio.h>
    #define DEBUG_MSG(msg, ...) do{fprintf(stdout,"%s:%d:%s(): " msg, __FILE__, __LINE__, __func__, ##__VA_ARGS__);}while(0)
#else
    #define DEBUG_MSG(msg, ...)
#endif

#define container_of(ptr, type, member) ((type *)  ((char *) ptr - offsetof(type, member)))
#define get_node(timer) ((heap_node *) &(timer)->node)
#define get_heap(loop) ((heap *) &(loop)->timerheap)

#define HANDLE_ACTIVE   1
#define HANDLE_WRITABLE 2
#define HANDLE_READABLE 4
#define HANDLE_CLOSING  8

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
        (handle)->closing_cb = NULL;                                                            \
        (handle)->loop = loop_;                                                                 \
        (handle)->type = type_;                                                                 \
        (handle)->flags = 0;                                                                    \
        QUEUE_INIT(&(handle)->handle_queue);                                                    \
        QUEUE_ADD_TAIL(&(loop)->handle_queue, &(handle)->handle_queue);                         \
    }while(0)                                                                                   \

#define apc_handle_close_(handle, cb)                                                           \
    do{                                                                                         \
        QUEUE_REMOVE(&(handle)->handle_queue);                                                  \
        QUEUE_INIT(&(handle)->handle_queue);                                                    \
        apc_deregister_handle_(handle, (handle)->loop);                                         \
        if((cb) != NULL){                                                                       \
            QUEUE_ADD_TAIL(&(handle)->loop->closing_queue, &(handle)->handle_queue);            \
        }                                                                                       \
        (handle)->data = NULL;                                                                  \
        (handle)->loop = NULL;                                                                  \
        (handle)->closing_cb = (cb);                                                            \
        (handle)->flags = HANDLE_CLOSING;                                                       \
    }while(0)                                                                                   \
    

#define apc_request_init_(req, type_)                                                           \
    do{                                                                                         \
        (req)->data = NULL;                                                                     \
        (req)->type = type_;                                                                    \
    }while(0)                                                                                   \


void apc_free(void *ptr);

void *apc_malloc(size_t size);

void *apc_calloc(size_t n, size_t size);

void *apc_realloc(void *ptr, size_t size);

/* closes an apc_file handle
 * @param apf_file *file
 * @return void 
 */
void apc_file_close(apc_file *file);

/* closes an apc_udp handle
 * @param apc_tcp *tcp
 * @return void
 */
void apc_tcp_close(apc_tcp *tcp);

/* closes an apc_udp handle
 * @param apc_tcp *tcp
 * @return void
 */
void apc_udp_close(apc_udp *udp);

/* close a apc_timer handle
 * @param apc_timer *timer
 * @return void
 */
void apc_timer_close(apc_timer *timer);

#endif