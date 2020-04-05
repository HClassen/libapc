#ifndef CORE_LINUX_HEADER
#define CORE_LINUX_HEADER

#include "apc.h"

int create_backend_fd();
void fd_watcher_poll(apc_loop *loop, int timeout);
void invalidate_fd(apc_loop *loop, int fd);

#endif