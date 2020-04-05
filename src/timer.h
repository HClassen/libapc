#ifndef TIMER_HEADER
#define TIMER_HEADER

#include "apc.h"

/* close a apc_timer handle
 * @param apc_timer *timer
 * @return void
 */
void timer_close(apc_timer *timer);

/* run all due timers
 * @param apc_loop *loop
 * @return void
 */
void run_timers(apc_loop *loop);

#endif