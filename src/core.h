#ifndef APC_CORE_HEADER
#define APC_CORE_HEADER

#include "apc.h"
#include "apc-internal.h"

void apc_free(void *ptr);

void *apc_malloc(size_t size);

void *apc_calloc(size_t n, size_t size);

void *apc_realloc(void *ptr, size_t size);

/* returns the size of all buffers in buffs combined 
 * @param const apc_buf bufs[]
 * @param size_t nbufs
 * @return size_t
 */
size_t apc_size_bufs(const apc_buf bufs[], size_t nbufs);

/* inits a fd_watcher witch callback function fd and filedescriptor fd
 * @param fd_watcher *w
 * @param fd_watcher_cb cb
 * @param int fd
 * @return void
 */
void fd_watcher_init(fd_watcher *w, fd_watcher_cb cb, int fd);

/* registers a fd_watcher for (a) specific poll-event(s)
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void fd_watcher_start(apc_loop *loop, fd_watcher *w, unsigned int events);

/* deregisters a fd_watcher for (a) specific poll-event(s)
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void fd_watcher_stop(apc_loop *loop, fd_watcher *w, unsigned int events);

/* closes a fd_watcher
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @return void
 */
void fd_watcher_close(apc_loop *loop, fd_watcher *w);

/* return wether a fd_watcher is registered for events
 * @paramconst fd_watcher *w
 * @param unsigned int events
 * @return int 
 */
int fd_watcher_active(const fd_watcher *w, unsigned int events);

/* callback function for a tcp server fd_watcher, gets set wenn apc_tcp_bind() is called
 * @param apc_loop *loop, fd_watcher *w
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void fd_watcher_server(apc_loop *loop, fd_watcher *w, unsigned int events);

/* base tcp callback function for a fd_watcher, gets passed to fd_watcher_init()
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void fd_watcher_io(apc_loop *loop, fd_watcher *w, unsigned int events);

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
void fd_watcher_udp_io(apc_loop *loop, fd_watcher *w, unsigned int events);

/* callback function passed to fd_watcher_init() used for internal wakeup
 * @param apc_loop *loop
 * @param fd_watcher *w
 * @param unsigned int events
 * @return void
 */
void wakeup_io(apc_loop *loop, fd_watcher *w, unsigned int events);

#endif