#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "apc.h"
#include "apc-internal.h"
#include "fd.h"

#if defined(UIO_MAXIOV)
	#define MY_IOV_MAX UIO_MAXIOV
#else
	#define MY_IOV_MAX 1024
#endif

int fd_set_flags(int fd, int flags){
	if(fd < 0){
		return APC_EINVAL;
	}
	
	int active_flags = fcntl(fd, F_GETFL, 0);
	if(active_flags == -1){
        return APC_EFDFLAGS;
    }

    active_flags |= flags;
    int err = fcntl(fd, F_SETFL, active_flags);
    if(err == -1){
        return APC_EFDFLAGS;
    } 
    return 0;
}

int fd_unset_flags(int fd, int flags){
	if(fd < 0){
		return APC_EINVAL;
	}

	int active_flags = fcntl(fd, F_GETFL, 0);
	if(active_flags == -1){
        return APC_EFDFLAGS;
    }

    active_flags &= ~flags;
    int err = fcntl(fd, F_SETFL, active_flags);
    if(err == -1){
        return APC_EFDFLAGS;
    } 
    return 0;
}

int fd_accept(int fd){
	int client = -1;
	static int acpt4 = 1;
	do{
#if defined(__linux__) || (defined(__FreeBSD__) && __FreeBSD__ >= 10) || defined(__NetBSD__)
		if(acpt4){
			client = accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
			if(client > -1){
				return client;
			}

			if(client == -1 && errno == ENOSYS){
				acpt4 = 0;
			}
		}
#endif
		client = accept(fd, NULL, NULL);
	}while(client == -1 && errno == EINTR);

	if(client == -1){
		return APC_ECONNACCEPT;
	}

	int err = fd_set_flags(client, O_NONBLOCK | O_CLOEXEC);
	if(err){
		close(client);
		return err;
	}

	return client;
}

ssize_t fd_read(int fd, const apc_buf *bufs, const size_t nbufs){
	if(nbufs == 1){
		return read(fd, bufs[0].base, bufs[0].len);
	}else if(nbufs > 1){
		assert(sizeof(apc_buf) == sizeof(struct iovec));
		struct iovec *iovec = (struct iovec *) bufs;
		int iovec_count = nbufs > MY_IOV_MAX ? MY_IOV_MAX : nbufs; 
		return readv(fd, iovec, iovec_count);
	}
	return APC_EINVAL;
}

ssize_t fd_write(int fd, const apc_buf *bufs, const size_t nbufs){
	if(nbufs == 1){
		return write(fd, bufs[0].base, bufs[0].len);
	}else if(nbufs > 1){
		assert(sizeof(apc_buf) == sizeof(struct iovec));
		struct iovec *iovec = (struct iovec *) bufs;
		int iovec_count = nbufs > MY_IOV_MAX ? MY_IOV_MAX : nbufs;
		return writev(fd, iovec, iovec_count);
	}
	return APC_EINVAL;
}

ssize_t fd_recvfrom(int fd, const apc_buf *bufs, const size_t nbufs, const struct sockaddr_storage *peeraddr){
	socklen_t addrlen = sizeof(*peeraddr);
	if(nbufs == 1){
		return recvfrom(fd, bufs[0].base, bufs[0].len, 0, (struct sockaddr *) peeraddr, &addrlen);
	}else if(nbufs > 1){
		assert(sizeof(apc_buf) == sizeof(struct iovec));
		struct iovec *iovec = (struct iovec *) bufs;
		int iovec_count = nbufs > MY_IOV_MAX ? MY_IOV_MAX : nbufs; 
		struct msghdr msg = (struct msghdr) {0};
		msg.msg_name = (struct sockaddr_storage *) peeraddr;
		msg.msg_namelen = sizeof(*peeraddr);
		msg.msg_iov = iovec;
		msg.msg_iovlen = iovec_count;
		return recvmsg(fd, &msg, 0);
	}
	return APC_EINVAL;
}

ssize_t fd_sendto(int fd, const apc_buf *bufs, const size_t nbufs, const struct sockaddr_storage *peeraddr){
	socklen_t addrlen = sizeof(*peeraddr);
	if(nbufs == 1){
		return sendto(fd, bufs[0].base, bufs[0].len, 0, (struct sockaddr *) peeraddr, addrlen);
	}else if(nbufs > 1){
		assert(sizeof(apc_buf) == sizeof(struct iovec));
		struct iovec *iovec = (struct iovec *) bufs;
		size_t iovec_count = nbufs > MY_IOV_MAX ? MY_IOV_MAX : nbufs; 
		struct msghdr msg = (struct msghdr) {0};
		msg.msg_name = (struct sockaddr_storage *) peeraddr;
		msg.msg_namelen = sizeof(*peeraddr);
		msg.msg_iov = iovec;
		msg.msg_iovlen = iovec_count;
		return sendmsg(fd, &msg, 0);
	}
	return APC_EINVAL;
}

int fd_pipe(int pipefds[2]){
#if defined(__linux__)
	return pipe2(pipefds, O_NONBLOCK | O_CLOEXEC);
#endif
	int err = pipe(pipefds);
	if(err == -1){
		return APC_ECREATEPIPE;
	}
	err = fd_set_flags(pipefds[0], O_NONBLOCK | O_CLOEXEC) + \
		  fd_set_flags(pipefds[1], O_NONBLOCK | O_CLOEXEC);

	if(err != 0){
		close(pipefds[0]);
		close(pipefds[1]);
		return err;
	}
	return 0;
}

ssize_t fd_pread(int fd, const apc_buf *bufs, size_t nbufs, off_t offset){
	if(nbufs == 1){
		return pread(fd, bufs[0].base, bufs[0].len, offset);
	}else if(nbufs > 1){
		assert(sizeof(apc_buf) == sizeof(struct iovec));
		struct iovec *iovec = (struct iovec *) bufs;
		int iovec_count = nbufs > MY_IOV_MAX ? MY_IOV_MAX : nbufs;
		return preadv(fd, iovec, iovec_count, offset);
	}
	return APC_EINVAL;
}

ssize_t fd_pwrite(int fd, const apc_buf *bufs, size_t nbufs, off_t offset){
	if(nbufs == 1){
		return pwrite(fd, bufs[0].base, bufs[0].len, offset);
	}else if(nbufs > 1){
		assert(sizeof(apc_buf) == sizeof(struct iovec));
		struct iovec *iovec = (struct iovec *) bufs;
		int iovec_count = nbufs > MY_IOV_MAX ? MY_IOV_MAX : nbufs;
		return pwritev(fd, iovec, iovec_count, offset);
	}
	return APC_EINVAL;
}