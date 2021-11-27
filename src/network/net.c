#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "net.h"

int apc_ip4_fill(char *ip, int port, struct sockaddr_in *addr){
	assert(addr != NULL);
	assert(ip != NULL);
	assert(port >= 0);

	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	int err = inet_pton(AF_INET, ip, &addr->sin_addr.s_addr);
	if(err != 1){
		return APC_EINVALIDADDR;
	}
	return 0;
}

int apc_ip6_fill(char *ip, int port, struct sockaddr_in6 *addr){
	assert(addr != NULL);
	assert(ip != NULL);
	assert(port >= 0);

	memset(addr, 0, sizeof(*addr));
	addr->sin6_family = AF_INET;
	addr->sin6_port = htons(port);
	int err = inet_pton(AF_INET, ip, &addr->sin6_addr.s6_addr);
	if(err != 1){
		return APC_EINVALIDADDR;
	}
	return 0;
}

int apc_ip_to_string(struct sockaddr *addr, char *dest, size_t size){
	const char *err = NULL;
	if(addr->sa_family == AF_INET){
		struct sockaddr_in *inaddr = (struct sockaddr_in *) addr;
		err = inet_ntop(AF_INET, &inaddr->sin_addr.s_addr, dest, size);
	}

	if(addr->sa_family == AF_INET6){
		struct sockaddr_in6 *in6addr = (struct sockaddr_in6 *) addr;
		err = inet_ntop(AF_INET6, &in6addr->sin6_addr.s6_addr, dest, size);
	}

	if(err == NULL){
		return APC_EINVALIDADDR;
	}
	return 0;
}

int apc_fill_hints(struct addrinfo *hints, int family, int type, int flags){
	assert(hints != NULL);
	*hints = (struct addrinfo) {0};
	hints->ai_family = family;
	hints->ai_socktype = type;
	hints->ai_flags = flags;
	return 0;
}

static void getaddrinfo_work(apc_work_req *work){
	apc_getaddrinfo_req *req = (apc_getaddrinfo_req *) work;
	req->err = getaddrinfo(req->host, req->service, req->hints, &req->res);
	if(req->err != 0){
		req->err = APC_EGETADDRINFO;
	}
}

static void getaddrinfo_done(apc_work_req *work){
	apc_getaddrinfo_req *req = (apc_getaddrinfo_req *) work;
	if(req->host != NULL){
		apc_free(req->host);
	}else if(req->service != NULL){
		apc_free(req->service);
	}else if(req->hints != NULL){
		apc_free(req->hints);
	}

	req->host = NULL;
	req->service = NULL;
	req->hints = NULL;
	if(req->cb != NULL){
		req->cb(req, req->res, req->err);
	}
}

int apc_getaddrinfo(apc_loop *loop, apc_getaddrinfo_req *req, struct addrinfo *hints, char *host, char *service, apc_on_getaddrinfo cb){
	assert(loop != NULL);
	assert(req != NULL);

	size_t host_size = host != NULL ? strlen(host) : 0;
	size_t service_size = service != NULL ? strlen(service) : 0; 
	size_t hints_size = hints != NULL ? sizeof(*hints) : 0;

	size_t size = host_size + service_size + hints_size;
	char *tmp = apc_malloc(size);
	if(tmp == NULL){
		return APC_ENOMEM;
	}

	size_t index = 0;
	req->host = NULL;
	if(host != NULL){
		req->host = memcpy(tmp + index, host, host_size);
		index += host_size;
	}

	req->service = NULL;
	if(service != NULL){
		req->service = memcpy(tmp + index, service, service_size);
		index += service_size;
	}

	req->hints = NULL;
	if(hints != NULL){
		req->hints = memcpy(tmp + index, hints, hints_size);
		index += hints_size;
	}

	req->res = NULL;
	req->cb = cb;
	req->err = 0;

	if(cb != NULL){
		return apc_add_work(loop, (apc_work_req *) req, getaddrinfo_work, getaddrinfo_done);
	}

	getaddrinfo_work((apc_work_req *) req);
	getaddrinfo_done((apc_work_req *) req);
	return req->err;
}

int apc_freeaddrinfo(struct addrinfo *res){
	if(res != NULL){
		freeaddrinfo(res);
	}
	return 0;
}

int socket_connect_error(int socket){
	int error;
	socklen_t errorsize = sizeof(int);
	getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &errorsize);
	return -(error);
}

int socket_bind(/* int type, const char *port */struct sockaddr *addr, int type){
	/* struct addrinfo hints = fill_addrinfo(AF_UNSPEC, type, AI_PASSIVE);
	struct addrinfo *res = apc_getaddrinfo(NULL, port, hints);

	int yes = 1, sock = -1;
	struct addrinfo *p;
	for(p = res; p != NULL; p = p->ai_next){
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(sock == -1){
			continue;
		}
		
		if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) == -1){
			exit(1);
		}
		
		break;
	}

	if(p == NULL){
		return -1;
	} */

	int err = socket(addr->sa_family, type, 0);
	if(err < 0){
		return APC_ESOCK;
	}
	int sock = err;

	err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if(err != 0){
		close(sock);
		return APC_ESOCK;
	}

	err = fd_nonblocking(sock) | fd_cloexec(sock);
	if(err != 0){
		close(sock);
		return err;
	}

	err = bind(sock, addr, sizeof(*addr));
	if(err == -1){
		close(sock);
		return APC_ESOCKBIND;
	}
	// freeaddrinfo(res);

	return sock;
}

int socket_connect(struct sockaddr *addr, int type, struct sockaddr_storage *peeraddr){
	/* int sock = -1;
	struct addrinfo *p = NULL;
	for(p = res; p != NULL; p = p->ai_next){
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(sock != -1){
			break;
		}
	}

	if(p == NULL){
		return -1;
	} */

	int err = socket(addr->sa_family, type, 0);
	if(err < 0){
		return APC_ESOCK;
	}
	int sock = err;

	err = fd_nonblocking(sock) | fd_cloexec(sock);
	if(err != 0){
		close(sock);
		return err;
	}
	
	err = connect(sock, addr, sizeof(*addr));	
	if(err == -1 && errno != EINPROGRESS){
		close(sock);
		return APC_ECONNECT;
	}

	if(addr->sa_family == AF_INET){
		*((struct sockaddr_in *) peeraddr) = *((struct sockaddr_in *) addr);
	}else if(addr->sa_family == AF_INET6){
		*((struct sockaddr_in6 *) peeraddr) = *((struct sockaddr_in6 *) addr);
	}

	// freeaddrinfo(res);
	return sock;
}

void net_start_read(apc_net *net, apc_alloc alloc, apc_on_read on_read){
    net->alloc = alloc;
    net->on_read = on_read;
    apc_register_handle_(net, net->loop);
    net->flags |= HANDLE_READABLE;
    apc_event_watcher_register(&net->loop->reactor, &net->watcher, APC_POLLIN);
}

void net_stop_read(apc_net *net){
    net->alloc = NULL;
    net->on_read = NULL;
    net->flags &= ~HANDLE_READABLE;
    apc_event_watcher_deregister(&net->loop->reactor, &net->watcher, APC_POLLOUT);
    if(!apc_event_watcher_active(&net->watcher, APC_POLLIN)){
        apc_deregister_handle_(net, net->loop);
    }
}

int net_write(apc_net *net, apc_write_req *req, const apc_buf bufs[], size_t nbufs, apc_on_write cb){
    apc_request_init_(req, APC_WRITE);
    req->on_write = cb;
    req->handle = (apc_handle *) net;
    req->err = 0;
    req->peer = (struct sockaddr_storage) {0};
	req->tmp = bufs[0];
    QUEUE_INIT(&req->write_queue);    
	
    req->bufs = malloc(nbufs * sizeof(apc_buf));
    if(req->bufs == NULL){
        return APC_ENOMEM;
    }

    memcpy(req->bufs, bufs, nbufs * sizeof(apc_buf));
    req->nbufs = nbufs;
    req->write_index = 0;
    net->write_queue_size += apc_size_bufs(bufs, nbufs);      
    QUEUE_ADD_TAIL(&net->write_queue, &req->write_queue);
    return 0;
}