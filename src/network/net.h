#ifndef APC_NET_HEADER
#define APC_NET_HEADER

#include "../apc.h"
#include "../internal.h"
#include "../core.h"
#include "../common/fd.h"
#include "../reactor/reactor.h"

#define apc_net_init_(net)                                              \
    do{                                                                 \
        (net)->alloc = NULL;                                            \
        (net)->on_read = NULL;                                          \
        (net)->write_queue_size = 0;                                    \
        (net)->peer = (struct sockaddr_storage) {0};                    \
        QUEUE_INIT(&(net)->write_queue);                                \
        QUEUE_INIT(&(net)->write_done_queue);                           \
    }while(0)                                                           \

#define apc_net_close_(net)                                             \
    do{                                                                 \
        apc_event_watcher_close(&(net)->loop->reactor, &(net)->watcher);\
        apc_flush_write_queue((apc_net *) net);                         \
        if((net)->watcher.fd != -1){                                    \
            close((net)->watcher.fd);                                   \
            (net)->watcher.fd = -1;                                     \
        }                                                               \
        (net)->alloc = NULL;                                            \
        (net)->on_read = NULL;                                          \
        (net)->write_queue_size = 0;                                    \
        (net)->peer = (struct sockaddr_storage) {0};                    \
        QUEUE_INIT(&(net)->write_queue);                                \
        QUEUE_INIT(&(net)->write_done_queue);                           \
    }while(0)                                                           \

#define HANDLE_FIELDS                                                   \
    void *data;                                                         \
    apc_loop *loop;                                                     \
    apc_handle_type type;                                               \
    void *handle_queue[2];                                              \
    unsigned int flags;                                                 \
    apc_on_closing closing_cb;                                          \

#define NETWORK_FIELDS                                                  \
    apc_event_watcher watcher;                                          \
    apc_alloc alloc;                                                    \
    apc_on_read on_read;                                                \
    void *write_queue[2];                                               \
    size_t write_queue_size;                                            \
    void *write_done_queue[2];                                          \
    struct sockaddr_storage peer;                                       \

typedef struct apc_net_ apc_net;
struct apc_net_{
    HANDLE_FIELDS
    NETWORK_FIELDS
};

/* Checks connection error on socket and return it
 * @param int socket
 * @return int
 */
int socket_connect_error(int socket);

/* Create and bind a socket of type to addr 
 * @param const struct sockaddr *addr
 * @param int type
 * @return int
 */
int socket_bind(struct sockaddr *addr, int type);

/* Create and connect a socket of type to addr 
 * @param const struct sockaddr *addr
 * @param int type
 * @param struct sockaddr_storage *peeraddr
 * @return int
 */
int socket_connect(struct sockaddr *addr, int type, struct sockaddr_storage *peeraddr);

/* start reading on an apc_net handle
 * @param apc_net *net
 * @param apc_alloc alloc
 * @param apc_on_read on_read
 * @return void
 */
void net_start_read(apc_net *net, apc_alloc alloc, apc_on_read on_read);

/* stops reading on an apc_net handle
 * @param apc_net *net
 * @return void
 */
void net_stop_read(apc_net *net);

/* start writing on an apc_net handle
 * @param apc_net *net
 * @param apc_write_req *req
 * @param const apc_buf bufs[]
 * @param size_t nbufs
 * @param apc_on_write cb
 * @return void
 */
int net_write(apc_net *net, apc_write_req *req, const apc_buf bufs[], size_t nbufs, apc_on_write cb);

#undef HANDLE_FIELDS
#undef NETWORK_FIELDS

#endif