#ifndef TPOOL_HEADER
#define TPOOL_HEADER

#include "apc.h"

int tpool_running();

/* adds work to the threadpool 
 * @param q - pointer to queue struct
 * @return on success HTTPSERVER_OK, else error
 */
int tpool_add_work(apc_work_req *req);

/* starts the threadpool 
 * @return on success HTTPSERVER_OK, else error
 */
int tpool_start();

/* initializes the threadpool but doesnt start it
 * @return on success HTTPSERVER_OK, else error
 */
int tpool_init();

/* waits till the queue is empty then cleans everything up
 * @return on success HTTPSERVER_OK, else error
 */
int tpool_cleanup();

#endif