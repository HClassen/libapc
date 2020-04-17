#define _POSIX_C_SOURCE 200809L
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>

#include "apc.h"
#include "apc-internal.h"
#include "net.h"
#include "core.h"
#include "reactor/reactor.h"

static int tcp_socket_bind(const char *port){
	struct addrinfo hints = fill_addrinfo(AF_UNSPEC, SOCK_STREAM, AI_PASSIVE);
	struct addrinfo *res = apc_getaddrinfo(NULL, port, hints);
	if(res == NULL){
		return -1;
	}

	int sock = socket_bind(res);
	return sock;
}

static int tcp_socket_connect(const char *host, const char *port){
	struct addrinfo hints = fill_addrinfo(AF_UNSPEC, SOCK_STREAM, AI_PASSIVE);
	struct addrinfo *res = apc_getaddrinfo(host, port, hints);
	if(res == NULL){
		return -1;
	}

	struct sockaddr_storage tmp;
	int sock = socket_connect(res, &tmp);
	return sock;
}

int apc_tcp_init(apc_loop *loop, apc_tcp *tcp){
    assert(loop != NULL);
    assert(tcp != NULL);

    apc_handle_init_(tcp, loop, APC_TCP);
    apc_net_init_(tcp);
    tcp->on_connection = NULL;
    tcp->connect_req = NULL;
    tcp->accepted_fd = -1;
    return 0;
}

void tcp_close(apc_tcp *tcp){
    apc_tcp_stop_read(tcp);
    apc_net_close_(tcp);
    if(tcp->accepted_fd != -1){
        close(tcp->accepted_fd);
        tcp->accepted_fd = -1;
    }
    tcp->connect_req = NULL;
    tcp->on_connection = NULL;
}

int apc_tcp_bind(apc_tcp *tcp, const char *port){
    assert(tcp != NULL);
    assert(port != NULL);

    int err = tcp_socket_bind(port);
    if(err < 0){
        return err;
    }

    apc_event_watcher_init(&tcp->watcher, fd_watcher_tcp_io, err);
    return 0;
}

int apc_tcp_connect(apc_tcp *tcp, apc_connect_req *req, const char *host, const char *service, apc_on_connected cb){
    assert(tcp != NULL);
    assert(req != NULL);
    assert(cb != NULL);
    assert(tcp->watcher.fd == -1);

    int err = tcp_socket_connect(host, service);
    if(err < 0){
        return err;
    }
    apc_event_watcher_init(&tcp->watcher, fd_watcher_tcp_io, err);

    apc_request_init_(req, APC_CONNECT);
    req->connected = cb;
    req->handle = tcp;
    tcp->connect_req = req;
    apc_register_request_(req, tcp->loop);
    apc_register_handle_(tcp, tcp->loop);
    apc_event_watcher_register(&tcp->loop->reactor, &tcp->watcher, APC_POLLOUT);
    return 0;
}

int apc_listen(apc_tcp *tcp, int backlog, apc_on_connection cb){
    assert(tcp != NULL);
    assert(cb != NULL);
    assert(tcp->type == APC_TCP);

    int err = listen(tcp->watcher.fd, backlog);
    if(err < 0){
        return err;
    }
    tcp->on_connection = cb; 
    tcp->watcher.cb = fd_watcher_server;
    apc_event_watcher_register(&tcp->loop->reactor, &tcp->watcher, APC_POLLIN);
    tcp->flags |= HANDLE_READABLE;

    apc_register_handle_(tcp, tcp->loop);
    return 0;
}

int apc_accept(apc_tcp *server, apc_tcp *client){
    assert(client != NULL);
    apc_tcp_init(server->loop, client);

/*     if(!(client->watcher.fd == -1 ||                  
        client->watcher.fd == server->accepted_fd)){
        return APC_EBUSY;                                      
    } */                                                   
    if(server->accepted_fd == -1){                    
        return APC_ECONNACCEPT;                                      
    }
    apc_event_watcher_init(&client->watcher, fd_watcher_tcp_io, server->accepted_fd);                         
    apc_event_watcher_register(&server->loop->reactor, &client->watcher, APC_POLLIN);     
    server->accepted_fd = -1;
    return 0;
}

int apc_tcp_start_read(apc_tcp *tcp, apc_alloc alloc, apc_on_read on_read){
    assert(tcp != NULL);
    assert(on_read != NULL);
    assert(tcp->watcher.fd >= 0);

    net_start_read((apc_net *) tcp, alloc, on_read);
    return 0;
}

int apc_tcp_stop_read(apc_tcp *tcp){
    assert(tcp != NULL);

    net_stop_read((apc_net *) tcp);
    return 0;
}

int apc_tcp_write(apc_tcp *tcp, apc_write_req *req, const apc_buf bufs[], size_t nbufs, apc_on_write cb){
    assert(tcp != NULL);
    assert(req != NULL);
    assert(nbufs > 0);
    
    if(tcp->watcher.fd < 0){
        return APC_EINVAL;
    }
    
    int err = net_write((apc_net *) tcp, req, bufs, nbufs, cb);
    if(err != 0){
        return err;
    }
    if(tcp->connect_req){
        return 0;
    }
    
    apc_register_request_(req, tcp->loop);
    apc_register_handle_(tcp, tcp->loop);
    tcp->flags |= HANDLE_WRITABLE;
    apc_event_watcher_register(&tcp->loop->reactor, &tcp->watcher, APC_POLLOUT);
    return 0;
}