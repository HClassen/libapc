#ifndef APC_TPOOL_HEADER
#define APC_TPOOL_HEADER

#include "../apc.h"
#include "../internal.h"

int tpool_running();

/* adds work to the threadpool, returns 0 on success or error 
 * @param apc_work_req *req
 * @return int
 */
int tpool_add_work(apc_work_req *req);

/* starts the threadpool, returns 0 on success or error
 * @return int
 */
int tpool_start();

/* initializes the threadpool, returns 0 on success or error
 * @return int
 */
int tpool_init();

/* waits till the queue is empty then cleans everything up, returns 0 on success or error
 * @return int
 */
int tpool_cleanup();

#endif