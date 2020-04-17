#ifndef APC_REACTOR_HEADER
#define APC_REACTOR_HEADER

#define APC_POLLIN  1
#define APC_POLLOUT 2
#define APC_POLLERR 4
#define APC_POLLHUP 8

typedef struct apc_reactor_ apc_reactor;
typedef struct apc_event_watcher_ apc_event_watcher;
typedef void (*event_watcher_cb)(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);

int apc_reactor_init(apc_reactor *reactor);
void apc_reactor_close(apc_reactor *reactor);
int apc_event_watcher_init(apc_event_watcher *w, event_watcher_cb cb, int fd);
void apc_event_watcher_register(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);
void apc_event_watcher_deregister(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);
void apc_event_watcher_close(apc_reactor *reactor, apc_event_watcher *w);
int apc_event_watcher_active(const apc_event_watcher *w, unsigned int events);
void apc_reactor_poll(apc_reactor *reactor, int timeout);

#endif