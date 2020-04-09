#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "../../apc.h"

const char *descr =                      \
"a simple example for the use of the udp \
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
        free(buf->base);
        apc_close(handle);
        return;
    }

    if(nread < 0){
        printf("%s\n", apc_strerror(nread));
        free(buf->base);
        apc_close(handle);
        return;
    }

    printf("recvd: %s\n", buf->base);
    free(buf->base);
    apc_close(handle);
}

void written(apc_write_req *req, apc_buf *bufs, int error){
    if(error < 0){
        printf("%s\n", apc_strerror(error));
        printf("errno: %s\n", strerror(errno));
        apc_close(req->handle);
    }
}

int main(int argc, char *argv[]){
    printf("%s", descr);
    if(argc != 2){
        printf("wrong argument count\n");
        return 1;
    }

    msg = argv[1];

    apc_loop loop;
    apc_udp client;
    apc_write_req wreq; 

    apc_loop_init(&loop);
    apc_udp_init(&loop, &client);

    apc_udp_connect(&client, "localhost", "8080");
    apc_buf buf = apc_buf_init(msg ,strlen(msg));
    apc_udp_write(&client, &wreq, &buf, 1, written);
    apc_udp_start_read(&client, my_alloc, recvd);

    apc_loop_run(&loop);
    return 0;
}