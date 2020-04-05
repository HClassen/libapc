#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../../apc.h"

const char *descr =                                 \
"a simple example for the use of timers:\n\
\tstarts two timers - one with a 5sec timeout \
and the other with a 10sec timeout\n";

int count = 0;

void fast_timeout(apc_timer *timer){
    printf("fast timer timeout ");
    if(count < 4){
        apc_timer_restart(timer);
        printf("/ repeat\n");
    }else{
        apc_close((apc_handle *) timer);
        printf("/ stop\n");
    }
    count += 1;
    
}

void slow_timeout(apc_timer *timer){
    printf("slow timer timeout ");
    if(count >= 4){
        apc_close((apc_handle *) timer);
        printf("/ stop\n");
    }else{
        printf("/ repeat\n");
    }
}

int main(){
    printf("%s", descr);
    apc_loop loop;
    apc_timer timer_f;
    apc_timer timer_s;

    apc_loop_init(&loop);
    apc_timer_init(&loop, &timer_f);
    apc_timer_init(&loop, &timer_s);

    apc_timer_start(&timer_f, 5, 0, fast_timeout);
    apc_timer_start(&timer_s, 10, 1, slow_timeout);
    
    apc_loop_run(&loop);
    return 0;
}