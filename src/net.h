#ifndef APC_NET_HEADER
#define APC_NET_HEADER

#include <sys/uio.h>

#include "apc.h"
#include "apc-internal.h"

#define apc_net_init_(net, fd_cb)                                       \
    do{                                                                 \
        (net)->alloc = NULL;                                            \
        (net)->on_read = NULL;                                          \
        (net)->write_queue_size = 0;                                    \
        QUEUE_INIT(&(net)->write_queue);                                \
        QUEUE_INIT(&(net)->write_done_queue);                           \
        fd_watcher_init(&(net)->watcher, (fd_cb), -1);                  \
    }while(0)                                                           \

#define apc_net_close_(net)                                             \
    do{                                                                 \
        fd_watcher_close((net)->loop, &(net)->watcher);                 \
        apc_flush_write_queue((apc_net *) net);                         \
        if((net)->watcher.fd != -1){                                    \
            close((net)->watcher.fd);                                   \
            (net)->watcher.fd = -1;                                     \
        }                                                               \
        (net)->alloc = NULL;                                            \
        (net)->on_read = NULL;                                          \
        (net)->write_queue_size = 0;                                    \
        QUEUE_INIT(&(net)->write_queue);                                \
        QUEUE_INIT(&(net)->write_done_queue);                           \
    }while(0)                                                           \

/* Checks connection error on socket and return it
 * @param int socket
 * @return int
 */
int socket_connect_error(int socket);

struct addrinfo fill_addrinfo(int family, int type, int flags);

struct addrinfo *apc_getaddrinfo(const char *host, const char *port, struct addrinfo hints);

int socket_bind(struct addrinfo *res);

int socket_connect(struct addrinfo *res, struct sockaddr_storage *peeraddr);

/* closes an apc_udp handle
 * @param apc_tcp *tcp
 * @return void
 */
void tcp_close(apc_tcp *tcp);

/* closes an apc_udp handle
 * @param apc_tcp *tcp
 * @return void
 */
void udp_close(apc_udp *udp);

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

#endif