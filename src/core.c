#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <poll.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "core.h"
#include "apc.h"
#include "core-linux.h"
#include "net.h"
#include "fd.h"
#include "fs.h"
#include "apc-internal.h"

#define MY_IOV_MAX UIO_MAXIOV

typedef struct apc_allocator_ apc_allocator;

struct apc_allocator_{
    void (*free)(void *ptr);
    void *(*malloc)(size_t size);
    void *(*calloc)(size_t n, size_t size);
    void *(*realloc)(void *ptr, size_t size);
};

static apc_allocator apc__allocater = {
    free,
    malloc,
    calloc,
    realloc
};

int apc_set_allocator(											\
	void (*cust_free)(void *ptr), 								\
	void *(*cust_malloc)(size_t size),							\
	void *(*cust_calloc)(size_t n, size_t size),				\
	void *(*cust_realloc)(void *ptr, size_t size)				\
){
	if(cust_free == NULL || cust_malloc == NULL ||				\
	   cust_calloc == NULL || cust_realloc == NULL){
		   return APC_EINVAL;
	}

	apc__allocater.free = cust_free;
	apc__allocater.malloc = cust_malloc;
	apc__allocater.calloc = cust_calloc;
	apc__allocater.realloc = cust_realloc;
	return 0;
}

void apc_free(void *ptr){
	if(ptr == NULL){
		return;
	}

	apc__allocater.free(ptr);
}

void *apc_malloc(size_t size){
	if(size == 0){
		return NULL;
	}

	return apc__allocater.malloc(size);
}

void *apc_calloc(size_t n, size_t size){
	if(size == 0 || n == 0){
		return NULL;
	}

	return apc__allocater.calloc(n, size);
}

void *apc_realloc(void *ptr, size_t size){
	if(size == 0){
		return NULL;
	}

	if(ptr == NULL){
		return apc__allocater.malloc(size);
	}

	return apc__allocater.realloc(ptr, size);
}

size_t apc_size_bufs(const apc_buf bufs[], size_t nbufs){
	size_t size = 0;
	for(size_t i = 0; i < nbufs; i++){
		size += bufs[i].len;
	}
	return size;
}

static void maybe_resize(apc_loop *loop, size_t len){
	fd_watcher **watchers = NULL;
/* 	void* fake_watcher_list = NULL;
	void* fake_watcher_count = NULL; */

	if(len <= loop->nwatchers){
		return;
	}

/* 	if(loop->watchers != NULL) {
		fake_watcher_list = loop->watchers[loop->nwatchers];
		fake_watcher_count = loop->watchers[loop->nwatchers + 1];
  	} */

	size_t nwatchers = len + 10;
	watchers = apc_realloc(loop->watchers, nwatchers * sizeof(loop->watchers[0]));

	if(watchers == NULL){
		abort();
	}

	loop->watchers = watchers;

	for(size_t i = loop->nwatchers; i<nwatchers; i++){
		loop->watchers[i] = NULL;
	}

/* 	loop->watchers[nwatchers] = fake_watcher_list;
	loop->watchers[nwatchers + 1] = fake_watcher_count; */
	loop->nwatchers = nwatchers;
}

void fd_watcher_init(fd_watcher *w, fd_watcher_cb cb, int fd){
	assert(cb != NULL);
	assert(fd >= -1);
	QUEUE_INIT(get_queue(&w->watcher_queue));

	w->cb = cb;
	w->fd = fd;
	w->events = 0;
	w->registered = 0;
}

void fd_watcher_start(apc_loop *loop, fd_watcher *w, unsigned int events){
	assert(0 == (events & ~(POLLIN | POLLOUT)));
	assert(0 != events);
	assert(w->fd >= 0);
	assert(w->fd < INT_MAX);

	w->events |= events;
	maybe_resize(loop, (size_t) (w->fd + 1));

	if(QUEUE_EMPTY(&w->watcher_queue)){
		QUEUE_ADD_TAIL(get_queue(&loop->watcher_queue), get_queue(&w->watcher_queue));
	}

	if(loop->watchers[w->fd] == NULL){
		loop->watchers[w->fd] = w;
		loop->nfds += 1;
	}
}

void fd_watcher_stop(apc_loop *loop, fd_watcher *w, unsigned int events){
	assert(0 == (events & ~(POLLIN | POLLOUT)));
  	assert(0 != events);

	if(w->fd == -1){
		return;
	}

	assert(w->fd >= 0);

	if((unsigned) w->fd >= loop->nwatchers){
		return;
	}

	w->events &= ~events;

	if(w->events == 0){
		QUEUE_REMOVE(get_queue(&w->watcher_queue));
		QUEUE_INIT(get_queue(&w->watcher_queue));

		if(loop->watchers[w->fd] != NULL){
			assert(loop->watchers[w->fd] == w);
			assert(loop->nfds > 0);
			loop->watchers[w->fd] = NULL;
			loop->nfds -= 1;
			w->registered = 0;
		}
	}else if(QUEUE_EMPTY(get_queue(&w->watcher_queue))){
		QUEUE_ADD_TAIL(get_queue(&loop->watcher_queue), get_queue(&w->watcher_queue));
	}
}

void fd_watcher_close(apc_loop *loop, fd_watcher *w){
	fd_watcher_stop(loop, w, POLLIN | POLLOUT);

	if(w->fd >= 0){
		invalidate_fd(loop, w->fd);
	}
}

int fd_watcher_active(const fd_watcher *w, unsigned int events){
  assert(0 == (events & ~(POLLIN | POLLOUT)));
  assert(0 != events);
  return 0 != (w->events & events);
}

void fd_watcher_server(apc_loop *loop, fd_watcher *w, unsigned int events){
	assert(events & POLLIN);
	apc_tcp *handle = container_of(w, apc_tcp, watcher);

	assert(handle->accepted_fd == -1);

	fd_watcher_start(loop, w, POLLIN);

	while(handle->watcher.fd != -1){
		assert(handle->accepted_fd == -1);
		int err = fd_accept(handle->watcher.fd);
		if(err < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				return;
			}

			if(err == ECONNABORTED){
				continue;
			}

			handle->on_connection(handle, err);
			continue;
		}

		handle->accepted_fd = err;
		handle->on_connection(handle, 0);

		if(handle->accepted_fd != -1){
			fd_watcher_stop(loop, w, POLLIN);
			handle->flags &= ~HANDLE_READABLE;
			if(!fd_watcher_active(&handle->watcher, POLLOUT)){
				apc_deregister_handle_(handle, loop);
			}
			close(handle->accepted_fd);
			return;
		}
	}
}

static void core_tcp_connect(apc_tcp *handle){
	apc_connect_req *req = handle->connect_req;
	int err = socket_connect_error(handle->watcher.fd);

	if(err == -(EINPROGRESS)){
		return;
	}

	apc_deregister_request_(req, handle->loop);

	handle->connect_req = NULL;
	if(err < 0/*  || QUEUE_EMPTY(&handle->write_queue) */){
		fd_watcher_stop(handle->loop, &handle->watcher, POLLOUT);
		if(!fd_watcher_active(&handle->watcher, POLLIN)){
			apc_deregister_handle_(handle, handle->loop);
		}
		err = APC_ECONNECT;
	}

	handle->flags |= HANDLE_READABLE | HANDLE_WRITABLE;

	if(req->connected){
		req->connected(handle, req, err);
	}

	if(handle->watcher.fd == -1){
		return;
	}
}

static void core_tcp_read(apc_tcp *handle){
	assert(handle);

	ssize_t recbytes = 0;
	while(handle->on_read != NULL && handle->watcher.fd != -1 && handle->flags & HANDLE_READABLE){
		assert(handle->alloc);
		apc_buf buf = apc_buf_init(NULL, 0);
		handle->alloc((apc_handle *) handle, &buf);

		if(buf.len == 0 || buf.base == NULL){
			handle->on_read((apc_handle *) handle, NULL, APC_EINVAL);
			return;
		}

		assert(handle->watcher.fd >= 0);
		assert(buf.base != NULL);

		do {
			recbytes = fd_read(handle->watcher.fd, &buf, 1);
		}while(recbytes < 0 && errno == EINTR);

		if(recbytes < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				fd_watcher_start(handle->loop, &handle->watcher, POLLIN);
				handle->on_read((apc_handle *) handle, &buf, APC_EWOULDBLOCK);
				return;
			}

			fd_watcher_stop(handle->loop, &handle->watcher, POLLIN);
			if(!fd_watcher_active(&handle->watcher, POLLOUT)){
				apc_deregister_handle_(handle, handle->loop);
			}
			handle->flags &= ~HANDLE_READABLE;
			handle->on_read((apc_handle *) handle, &buf, APC_EFDREAD);
			return;
		}else if(recbytes == 0){
			fd_watcher_stop(handle->loop, &handle->watcher, POLLIN);
			if(!fd_watcher_active(&handle->watcher, POLLOUT)){
				apc_deregister_handle_(handle, handle->loop);
			}

			handle->on_read((apc_handle *) handle, &buf, APC_EOF);
			break;
		}else{
			size_t buflen = buf.len;
			handle->on_read((apc_handle *) handle, &buf, recbytes);

			if((size_t) recbytes < buflen){
				break;
			}
		}
	}
}

static int core_advance_write_queue(apc_tcp *handle, apc_write_req *req, size_t n){
	assert(n <= handle->write_queue_size);
	handle->write_queue_size -= n;

	apc_buf *buf = req->bufs + req->write_index;
	size_t len = 0;
	size_t index = 0;

	do{
		len = n < buf->len ? n : buf->len;
		buf->base += len;
		buf->len -= len;
		int next = (buf->len == 0);
		if(next == 1){
			*buf = req->tmp;
			buf += next;
			index += 1;
			if(index < req->nbufs){
				req->tmp = *buf;
			}	
		}
		n -= len;
	}while(n > 0);

	req->write_index = buf - req->bufs;
	return req->write_index == req->nbufs;
}

static void core_advance_write_queue_error(apc_tcp *handle, apc_write_req *req){
	assert(req->bufs != NULL);
	size_t size = apc_size_bufs(req->bufs + req->write_index, req->nbufs - req->write_index);
	assert(handle->write_queue_size >= size);

	handle->write_queue_size -= size;
}

static void core_finish_write(apc_write_req *req){
	QUEUE_REMOVE(get_queue(&req->write_queue));
	QUEUE_ADD_TAIL(get_queue(&((apc_tcp *) req->handle)->write_done_queue), get_queue(&req->write_queue));
}

static void core_tcp_write(apc_tcp *handle){
	assert(handle);
	assert(handle->watcher.fd >= 0);

	if(QUEUE_EMPTY(get_queue(&handle->write_queue))){
		return;
	}

	queue *q = QUEUE_NEXT(get_queue(&handle->write_queue));
	apc_write_req *req = container_of(q, apc_write_req, write_queue);
	assert(req->handle == (apc_handle *) handle);

	ssize_t n = 0;
	do{
		n = fd_write(handle->watcher.fd, req->bufs, req->nbufs);
	}while(n == -1 && errno == EINTR);

	if(n == -1 && !(errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS)){
		req->err = APC_EFDWRITE;
		core_advance_write_queue_error(handle, req);
		core_finish_write(req);

		fd_watcher_stop(req->handle->loop, &handle->watcher, POLLOUT);
		if(!fd_watcher_active(&handle->watcher, POLLIN)){
			apc_deregister_handle_(handle, handle->loop);
		}
		handle->flags &= ~ HANDLE_WRITABLE;
		return;
	}

	if(n >= 0 && core_advance_write_queue(handle, req, n)){
		core_finish_write(req);
		return;
	}

	fd_watcher_start(req->handle->loop, &handle->watcher, POLLOUT);
	return;
}

static void core_write_callbacks(apc_net *handle){
	if(QUEUE_EMPTY(get_queue(&handle->write_done_queue))){
		return;
	}

	queue q;
	QUEUE_INIT(&q);
	QUEUE_MOVE(get_queue(&handle->write_done_queue), &q);
	while(!QUEUE_EMPTY(&q)){
		queue *e = QUEUE_NEXT(&q);
		apc_write_req *req = container_of(e, apc_write_req, write_queue);
		QUEUE_REMOVE(e);
		apc_buf *bufs = req->bufs;

		apc_deregister_request_(req, handle->loop);

		if(req->on_write){
			req->on_write(req, req->bufs, req->err);
		}
		
		apc_free(bufs);
		bufs = NULL;
	}
}

void fd_watcher_io(apc_loop *loop, fd_watcher *w, unsigned int events){
	apc_tcp *handle = container_of(w, apc_tcp, watcher);

	if(handle->connect_req){
		core_tcp_connect(handle);
		return;
	}

	assert(handle->watcher.fd >= 0);
	
	if(events & (POLLIN)){
		core_tcp_read(handle);
	}

	if(handle->watcher.fd == -1){
		return;
	}

	if(events & (POLLOUT)){
		core_tcp_write(handle);
		core_write_callbacks((apc_net *) handle);

		if(QUEUE_EMPTY(&handle->write_queue)){
			fd_watcher_stop(loop, &handle->watcher, POLLOUT);
			if(!fd_watcher_active(&handle->watcher, POLLIN)){
				apc_deregister_handle_(handle, handle->loop);
			}
		}
	}
}

static void core_udp_read(apc_udp *handle){
	assert(handle);

	ssize_t recbytes = 0;
	apc_buf buf;
	while(handle->on_read != NULL && handle->watcher.fd != -1 && handle->flags & HANDLE_READABLE){
		assert(handle->alloc);
		buf = apc_buf_init(NULL, 0);
		handle->alloc((apc_handle *) handle, &buf);

		if(buf.len == 0 || buf.base == NULL){
			handle->on_read((apc_handle *) handle, NULL, -1);
			return;
		}

		assert(buf.base != NULL);

		do {
			recbytes = fd_recvfrom(handle->watcher.fd, &buf, 1, &handle->peer);
		}while(recbytes < 0 && errno == EINTR);

		if(recbytes < 0){
			handle->peer = (struct sockaddr_storage) {0};
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				// fd_watcher_start(handle->loop, &handle->watcher, POLLIN);
				handle->on_read((apc_handle *) handle, &buf, APC_EWOULDBLOCK);
				return;
			}

			handle->on_read((apc_handle *) handle, &buf, APC_EFDREAD);
			return;
		}else{
			size_t buflen = buf.len;
			handle->on_read((apc_handle *) handle, &buf, recbytes);

			if((size_t) recbytes < buflen){
				break;
			}
		}
	}
}

static void core_udp_write(apc_udp *handle){
	assert(handle);
	assert(handle->watcher.fd >= 0);

	if(QUEUE_EMPTY(get_queue(&handle->write_queue))){
		return;
	}

	ssize_t n = 0;
	while(!QUEUE_EMPTY(get_queue(&handle->write_queue))){
		queue *q = QUEUE_NEXT(get_queue(&handle->write_queue));
		apc_write_req *req = container_of(q, apc_write_req, write_queue);

		do{
			n = fd_sendto(handle->watcher.fd, req->bufs, req->nbufs, &req->peer);
		}while(n < 0 && errno == EINTR);

		if(n < 0){
			if(errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS){
				break;
			}
		}

		req->err = n == -1 ? APC_EFDWRITE : 0;
		handle->write_queue_size -= apc_size_bufs(req->bufs, req->nbufs);

		QUEUE_REMOVE(get_queue(&req->write_queue));
		QUEUE_ADD_TAIL(get_queue(&handle->write_done_queue), get_queue(&req->write_queue));
	}
}

void fd_watcher_udp_io(apc_loop *loop, fd_watcher *w, unsigned int events){
	apc_udp *handle = container_of(w, apc_udp, watcher);

	assert(handle->watcher.fd >= 0);
	
	if(events & (POLLIN)){
		core_udp_read(handle);
	}

	if(handle->watcher.fd == -1){
		return;
	}

	if(events & (POLLOUT)){
		core_udp_write(handle);
		core_write_callbacks((apc_net *) handle);

		if(QUEUE_EMPTY(&handle->write_queue)){
			fd_watcher_stop(loop, &handle->watcher, POLLOUT);
			if(!fd_watcher_active(&handle->watcher, POLLIN)){
				apc_deregister_handle_(handle, handle->loop);
			}
		}
	}
}

void apc_flush_write_queue(apc_net *handle){
	assert(handle != NULL);

	queue wq;
	QUEUE_INIT(&wq);
	QUEUE_MOVE(get_queue(&handle->write_queue), &wq);
	while(!QUEUE_EMPTY(&wq)){
		queue *q = QUEUE_NEXT(&wq);
		apc_write_req *req = NULL;
		QUEUE_REMOVE(q);
		req = container_of(q, apc_write_req, write_queue);
		req->err = APC_EHANDLECLOSED;
		QUEUE_ADD_TAIL(get_queue(&handle->write_done_queue), q);
	}

	core_write_callbacks(handle);
}


void wakeup_io(apc_loop *loop, fd_watcher *w, unsigned int events){
	if((!events & POLLIN)){
		return;
	}

	char char_buf[128];
	size_t len = 128;
	apc_buf buf = apc_buf_init(char_buf, len);

	ssize_t n;
	do{
		n = fd_read(w->fd, &buf, 1);
	}while(n == (ssize_t) len);
	
	queue wq;
	pthread_mutex_lock(&loop->workmtx);
	QUEUE_MOVE(get_queue(&loop->work_queue), &wq);
	pthread_mutex_unlock(&loop->workmtx);

	while(!QUEUE_EMPTY(&wq)){
		queue *q = QUEUE_NEXT(&wq);
		apc_work_req *req = container_of(q, apc_work_req, work_queue);
		QUEUE_REMOVE(q);
		if(req->done != NULL){
			req->done(req);
		}
		apc_deregister_request_(req, loop);
	}

	fd_watcher_start(loop, w, POLLIN);
}