#ifndef APC_FD_HEADER
#define APC_FD_HEADER

#include "../apc.h"
#include "../internal.h"

/* sets the O_NONBLOCKUNG flag on fd, returns 0 on success or error code
 * @param int fd
 * @return int
 */
int fd_nonblocking(int fd);

/* sets the FD_CLOEXEC flag on fd, returns 0 on success or error code
 * @param int fd
 * @return int
 */
int fd_cloexec(int fd);

/* accepts new connection on fd and sets it to non-blocking,
 * return a new fd on success or -errno on error
 * @param int fd
 * @return int
 */
int fd_accept(int fd);

/* reads from fd to bufs, return bytes read or -1 on error
 * @param int fd
 * @param const apc_buf *bufs
 * @param const size_t nbufs
 * @return ssize_t
 */
ssize_t fd_read(int fd, const apc_buf *bufs, const size_t nbufs);

/* writes from bufs to fd, return bytes written or -1 on error
 * @param int fd
 * @param const apc_buf *bufs
 * @param const size_t nbufs
 * @return ssize_t
 */
ssize_t fd_write(int fd, const apc_buf *bufs, const size_t nbufs);

/* reads from fd to bufs (for connectionless), return bytes read or -1 on error
 * @param int fd
 * @param const apc_buf *bufs
 * @param const size_t nbufs
 * @return ssize_t
 */
ssize_t fd_recvfrom(int fd, const apc_buf *bufs, const size_t nbufs, const struct sockaddr_storage *peeraddr);

/* writes from bufs to fd (for connectionless), return bytes written or -1 on error
 * @param int fd
 * @param const apc_buf *bufs
 * @param const size_t nbufs
 * @return ssize_t
 */
ssize_t fd_sendto(int fd, const apc_buf *bufs, const size_t nbufs, const struct sockaddr_storage *peeraddr);

/* does the same as fd_read but with an offset as extra argument
 * @param int fd
 * @param const apc_buf *bufs
 * @param const size_t nbufs
 * @return ssize_t
 */
ssize_t fd_pread(int fd, const apc_buf *bufs, size_t nbufs, off_t offset);

/* does the same as fd_write but with an offset as extra argument
 * @param int fd
 * @param const apc_buf *bufs
 * @param const size_t nbufs
 * @return ssize_t
 */
ssize_t fd_pwrite(int fd, const apc_buf *bufs, size_t nbufs, off_t offset);

/* creates a pipe and sets both ends to non-blocking an close-on-exec, return 0 on success or -1 on error
 * @param int pipefds[2]
 * @return int
 */
int fd_pipe(int pipefds[2]);

#endif