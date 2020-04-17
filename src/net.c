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

#include "apc.h"
#include "apc-internal.h"
#include "core.h"
#include "net.h"
#include "fd.h"
#include "reactor/reactor.h"

struct addrinfo fill_addrinfo(int family, int type, int flags){
	struct addrinfo hints = (struct addrinfo) {0};
	hints.ai_family = family;
	hints.ai_socktype = type;
	hints.ai_flags = flags;
	return hints;
}

struct addrinfo *apc_getaddrinfo(const char *host, const char *port, struct addrinfo hints){
	struct addrinfo *res = NULL;
	int status;
	if((status = getaddrinfo(host,port,&hints,&res)) != 0){
		// fprintf(stderr,"getaddrinfo: %s, %d\n", gai_strerror(status), status);
		return NULL;
	}
	return res;
}

int socket_connect_error(int socket){
	int error;
	socklen_t errorsize = sizeof(int);
	getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &errorsize);
	return -(error);
}

int socket_bind(struct addrinfo *res){
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
	}

	int err = fd_nonblocking(sock) | fd_cloexec(sock);
	if(err != 0){
		close(sock);
		return err;
	}

	if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
		close(sock);
		return APC_ESOCKBIND;
	}
	freeaddrinfo(res);

	return sock;
}

int socket_connect(struct addrinfo *res, struct sockaddr_storage *peeraddr){
	int sock = -1;
	struct addrinfo *p = NULL;
	for(p = res; p != NULL; p = p->ai_next){
		sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(sock != -1){
			break;
		}
	}

	if(p == NULL){
		return -1;
	}

	int err = fd_nonblocking(sock) | fd_cloexec(sock);
	if(err != 0){
		close(sock);
		return err;
	}
	
	err = connect(sock, p->ai_addr, p->ai_addrlen);	
	if(err == -1 && errno != EINPROGRESS){
		close(sock);
		return APC_ECONNECT;
	}

	if(p->ai_addr->sa_family == AF_INET){
		*((struct sockaddr_in *) peeraddr) = *((struct sockaddr_in *) p->ai_addr);
	}else if(p->ai_addr->sa_family == AF_INET6){
		*((struct sockaddr_in6 *) peeraddr) = *((struct sockaddr_in6 *) p->ai_addr);
	}

	freeaddrinfo(res);
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

//Convert a struct sockaddr address to a string, IPv4 and IPv6:
/* static char *get_ip_str(const struct sockaddr *sa){
	char *s = NULL;
	size_t maxlen;
    switch(sa->sa_family) {
        case AF_INET:
			maxlen = INET_ADDRSTRLEN;
			s = calloc(sizeof(char), maxlen);
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),s, maxlen);
            break;

        case AF_INET6:
			maxlen = INET6_ADDRSTRLEN;
			s = calloc(sizeof(char), maxlen);
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),s, maxlen);
            break;

		default:
			maxlen = 8;
			s = calloc(sizeof(char), maxlen);
			snprintf(s, maxlen, "%s", "Unknown");
			break;
    }

    return s;
} */