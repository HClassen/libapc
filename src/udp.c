#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <assert.h>

#include "apc.h"
#include "apc-internal.h"
#include "net.h"
#include "core.h"

static int udp_socket_bind(const char *port){
	struct addrinfo hints = fill_addrinfo(AF_UNSPEC, SOCK_DGRAM, AI_PASSIVE);
	struct addrinfo *res = apc_getaddrinfo(NULL, port, hints);
	if(res == NULL){
		return -1;
	}

	int sock = socket_bind(res);
	return sock;
}

static int udp_socket_connect(const char *host, const char *port, struct sockaddr_storage *peeraddr){
	struct addrinfo hints = fill_addrinfo(AF_UNSPEC, SOCK_DGRAM, 0);
	struct addrinfo *res = apc_getaddrinfo(host, port, hints);
	if(res == NULL){
		return -1;
	}

	int sock = socket_connect(res, peeraddr);
	return sock;
}

int apc_udp_init(apc_loop *loop, apc_udp *udp){
    assert(loop != NULL);
    assert(udp != NULL);

    apc_handle_init_(udp, loop, APC_UDP);
    apc_net_init_(udp, fd_watcher_udp_io);
    udp->peer = (struct sockaddr_storage) {0};
    return 0;
}

void udp_close(apc_udp *udp){
    apc_udp_stop_read(udp);
    apc_net_close_(udp); 
    udp->peer = (struct sockaddr_storage) {0}; 
}

int apc_udp_bind(apc_udp *udp, const char *port){
    assert(udp != NULL);
    assert(port != NULL);

    int err = udp_socket_bind(port);
    if(err < 0){
        return err;
    }

    udp->watcher.fd = err;
    return 0;
}

int apc_udp_connect(apc_udp *udp, const char *host, const char *service){
    assert(udp != NULL);
    assert(udp->watcher.fd == -1);

    int err = udp_socket_connect(host, service, &udp->peer);
    if(err < 0){
        return err;
    }

    udp->watcher.fd = err;
    return 0;
}

int apc_udp_start_read(apc_udp *udp, apc_alloc alloc, apc_on_read on_read){
    assert(udp != NULL);
    assert(on_read != NULL);
    assert(udp->watcher.fd >= 0);

    net_start_read((apc_net *) udp, alloc, on_read);
    return 0;
}

int apc_udp_stop_read(apc_udp *udp){
    assert(udp != NULL);

    net_stop_read((apc_net *) udp);
    return 0;
}

int apc_udp_write(apc_udp *udp, apc_write_req *req, const apc_buf bufs[], size_t nbufs, apc_on_write cb){
    assert(udp != NULL);
    assert(req != NULL);
    assert(nbufs > 0);
    
    if(udp->watcher.fd < 0){
        return APC_EINVAL;
    }

    int err = net_write((apc_net *) udp, req, bufs, nbufs, cb);
    if(err != 0){
        return err;
    }
    req->peer = udp->peer;

    apc_register_request_(req, udp->loop);
    apc_register_handle_(udp, udp->loop);
    udp->flags |= HANDLE_WRITABLE;
    fd_watcher_start(udp->loop, &udp->watcher, POLLOUT);
    return 0;
}