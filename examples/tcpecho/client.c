#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../apc.h"

const char *descr =                      \
"a simple example for the use of the tcp \
type:\n\
\t start the server first and then use the \
client to send the first cmd-line argument \
as a message\n";

char *msg = NULL;

void my_alloc(apc_handle *handle, apc_buf *buf){
    buf->base = calloc(strlen(msg) + 1, sizeof(char));
    buf->len = strlen(msg) * sizeof(char);
}

void recvd(apc_handle *handle, apc_buf *buf, ssize_t nread){
    if(nread == APC_EOF){
        printf("EOF\n");
    }

    if(nread < 0){
        printf("%s\n", apc_strerror(nread));
    }

    if(nread > 0){
        printf("recvd: %s\n", buf->base);
    }

    free(buf->base);
    apc_close(handle, NULL);
}

void written(apc_write_req *req, apc_buf *bufs, int error){
    free(req);
    if(error < 0){
        printf("%s\n", apc_strerror(error));
        apc_close(req->handle, NULL);
    }
}

void connected(apc_tcp *tcp, apc_connect_req *req, int error){
    if(error < 0){
        printf("%s\n", apc_strerror(error));
        apc_close((apc_handle *) tcp, NULL);
        return;
    }

    apc_write_req *wreq = malloc(sizeof(apc_write_req));
    apc_buf buf = apc_buf_init(msg , strlen(msg));
    apc_tcp_write(tcp, wreq, &buf, 1, written);
    apc_tcp_start_read(tcp, my_alloc, recvd);
}

int main(int argc, char *argv[]){
    printf("%s", descr);
    if(argc != 2){
        printf("wrong argument count\n");
        return 1;
    }

    msg = argv[1];

    apc_loop loop;
    apc_tcp client;
    apc_connect_req creq;

    apc_loop_init(&loop);
    apc_tcp_init(&loop, &client);

    struct sockaddr_in addr;
    apc_ip4_fill("localhost", 8080, &addr);
    apc_tcp_connect(&client, &creq, (struct sockaddr *) &addr, connected);

    apc_loop_run(&loop);
    return 0;
}