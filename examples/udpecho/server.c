#include <stdlib.h>
#include <stdio.h>

#include "../../apc.h"

const char *descr =                           \
"a simple example for the use of the udp \
type:\n\
\t start the server first and then use the \
client to send the first cmd-line argument \
as a message\n";                            

void my_alloc(apc_handle *handle, apc_buf *buf){
    buf->base = calloc(64, sizeof(char));
    buf->len = 63 * sizeof(char);
}

void written(apc_write_req *req, apc_buf *bufs, int error){
    if(error < 0){
        printf("written (%s)\n", apc_strerror(error));
        apc_close(req->handle);
    }

    free(bufs->base);
}

void recvd(apc_handle *handle, apc_buf *buf, ssize_t nread){
    if(nread == APC_EOF){
        free(buf->base);
        return;
    }

    if(nread < 0){
        printf("recvd (%s)\n", apc_strerror(nread));
        free(buf->base);
        apc_close(handle);
        return;
    }

    printf("recvd: %s\n", buf->base);
    apc_write_req *wreq = malloc(sizeof(apc_write_req));
    apc_udp_write((apc_udp *) handle, wreq, buf, 1, written);
}

int main(){
    printf("%s", descr);
    apc_loop loop;
    apc_udp server;

    apc_loop_init(&loop);
    apc_udp_init(&loop, &server);

    apc_udp_bind(&server, "8080");
    apc_udp_start_read(&server, my_alloc, recvd);

    apc_loop_run(&loop);
    return 0;
}