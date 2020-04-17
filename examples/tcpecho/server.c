#include <stdlib.h>
#include <stdio.h>

#include "../../apc.h"

const char *descr =                       \
"a simple example for the use of the tcp \
type:\n\
\t start the server first and then use the \
client to send the first cmd-line argument \
as a message\n";

void closed(apc_handle *handle){
    free(handle);
}

void my_alloc(apc_handle *handle, apc_buf *buf){
    buf->base = calloc(64, sizeof(char));
    buf->len = 63 * sizeof(char);
}

void written(apc_write_req *req, apc_buf *bufs, int error){
    if(error < 0){
        printf("written (%s)\n", apc_strerror(error));
        apc_close(req->handle, closed);
    }

    free(bufs->base);
}

void recvd(apc_handle *handle, apc_buf *buf, ssize_t nread){
    if(nread == APC_EOF){
        printf("client disconnected\n");
        free(buf->base);
        return;
    }

    if(nread < 0){
        printf("recvd (%s)\n", apc_strerror(nread));
        free(buf->base);
        apc_close(handle, closed);
        return;
    }

    printf("recvd: %s\n", buf->base);
    apc_write_req *wreq = malloc(sizeof(apc_write_req));
    apc_tcp_write((apc_tcp *) handle, wreq, buf, 1, written);
}

void new_connection(apc_tcp *server, int error){
    if(error < 0){
        printf("new_connection (%s)\n", apc_strerror(error));
        return;
    }

    printf("new connection\n");
    apc_tcp *client = malloc(sizeof(apc_tcp));
    apc_accept(server, client);
    apc_tcp_start_read(client, my_alloc, recvd);
}

int main(){
    printf("%s", descr);
    apc_loop loop;
    apc_tcp server;

    apc_loop_init(&loop);
    apc_tcp_init(&loop, &server);

    apc_tcp_bind(&server, "8080");
    apc_listen(&server, 10, new_connection);

    apc_loop_run(&loop);
    return 0;
}