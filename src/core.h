#ifndef APC_CORE_HEADER
#define APC_CORE_HEADER

#include "apc.h"
#include "apc-internal.h"

typedef struct apc_net_ apc_net;

/* returns the size of all buffers in buffs combined 
 * @param const apc_buf bufs[]
 * @param size_t nbufs
 * @return size_t
 */
size_t apc_size_bufs(const apc_buf bufs[], size_t nbufs);

/* callback function for a tcp server fd_watcher, gets set wenn apc_tcp_bind() is called
 * @param apc_loop *loop, fd_watcher *w
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void fd_watcher_server(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);

/* base tcp callback function for a fd_watcher, gets passed to fd_watcher_init()
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void fd_watcher_tcp_io(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);

/* flush all remaining entries in the write queue of a tcp/udp handle
 * @param apc_net *handle
 * @return void
 */
void apc_flush_write_queue(apc_net *handle);

/* base udp callback function for a fd_watcher, gets passed to fd_watcher_init()
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void fd_watcher_udp_io(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);

/* callback function passed to fd_watcher_init() used for internal wakeup
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void wakeup_io(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);

#endif