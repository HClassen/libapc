#ifndef APC_HEADER
#define APC_HEADER

#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>

#define APC_EOF -1

/* error codes */
enum apc_error_code_ {
    success,
    fileopen = -19,
#define APC_EFILEOPEN fileopen
    fileexists,
#define APC_EFILEEXISTS fileexists
    fdpoll,
#define APC_EFDPOLL fdpoll
    fdread,
#define APC_EFDREAD fdread
    fdwrite,
#define APC_EFDWRITE fdwrite
    fdflags,
#define APC_EFDFLAGS fdflags
    wouldblock,
#define APC_EWOULDBLOCK wouldblock
    sockbind,
#define APC_ESOCKBIND sockbind
    sockaccept,
#define APC_ECONNACCEPT sockaccept
    sockconnect,
#define APC_ECONNECT sockconnect
    createpipe,
#define APC_ECREATEPIPE createpipe
    invalidpath,
#define APC_EINVALIDPATH invalidpath
    busy,
#define APC_EBUSY busy
    unknownhandle,
#define APC_EUNKNOWNHANDLE unknownhandle
    handleclosed,
#define APC_EHANDLECLOSED handleclosed
    notsupported,
#define APC_ENOTSUPPORTED notsupported
    nomem,
#define APC_ENOMEM nomem
    inval,
#define APC_EINVAL inval
    error,
#define APC_ERROR error
};

typedef struct apc_loop_ apc_loop;
typedef struct apc_handle_ apc_handle;
typedef struct apc_tcp_ apc_tcp;
typedef struct apc_udp_ apc_udp;
typedef struct apc_file_ apc_file;
typedef struct apc_timer_ apc_timer;

typedef struct apc_write_req_ apc_write_req;
typedef struct apc_write_wrapper_ apc_write_wrapper;
typedef struct apc_connect_req_ apc_connect_req;
typedef struct apc_work_req_ apc_work_req;
typedef struct apc_file_op_req_ apc_file_op_req;

typedef struct acp_buf_ apc_buf;
typedef struct fd_watcher_ fd_watcher;
typedef struct apc_watcher_ apc_watcher;

typedef void (*apc_alloc)(apc_handle *handle, apc_buf *buf);
typedef void (*apc_on_read)(apc_handle *handle, apc_buf *buf, ssize_t nread);
typedef void (*apc_on_connection)(apc_tcp *tcp, int error);
typedef void (*apc_on_connected)(apc_tcp *tcp, apc_connect_req *req, int error);
typedef void (*apc_on_write)(apc_write_req *req, apc_buf *bufs, int error);
typedef void (*apc_work)(apc_work_req *work);
typedef void (*apc_on_file_op)(apc_file *file, apc_file_op_req *req, apc_buf *bufs, ssize_t nbytes);
typedef void (*apc_on_timeout)(apc_timer *handle);
typedef void (*apc_on_closing)(apc_handle *handle);

typedef struct apc_reactor_ apc_reactor;
typedef struct apc_event_watcher_ apc_event_watcher;

struct apc_reactor_{
    apc_event_watcher **event_watchers;
    int nwatchers;
    int nfds;
    int backend_fd;
    void *watcher_queue[2];
};

typedef void (*event_watcher_cb)(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);
struct apc_event_watcher_{
    int fd;
    unsigned int events;
    unsigned int registered;
    event_watcher_cb cb;
    void *watcher_queue[2];
};

struct acp_buf_{
    char *base;
    size_t len;
};

struct apc_loop_{
    void *work_queue[2];
    void *handle_queue[2];
    void *closing_queue[2];
    apc_reactor reactor;
    size_t active_handles;
    size_t active_requests;
    pthread_mutex_t workmtx;
    apc_event_watcher wakeup_watcher;
    int wakeup_fd;
    struct {
        void *head;
        int nnodes;
        void *cmp;
    }timerheap;
    time_t now;
    size_t timerid;
};

typedef enum apc_handle_type_ {APC_BASE_HANDLE, APC_TCP, APC_UDP, APC_FILE, APC_TIMER, APC_TYPE_MAX} apc_handle_type;

#define HANDLE_FIELDS                                                   \
    void *data;                                                         \
    apc_loop *loop;                                                     \
    apc_handle_type type;                                               \
    void *handle_queue[2];                                              \
    unsigned int flags;                                                 \
    apc_on_closing closing_cb;                                          \

struct apc_handle_{
    HANDLE_FIELDS
};

#define NETWORK_FIELDS                                                  \
    apc_event_watcher watcher;                                          \
    apc_alloc alloc;                                                    \
    apc_on_read on_read;                                                \
    void *write_queue[2];                                               \
    size_t write_queue_size;                                            \
    void *write_done_queue[2];                                          \

struct apc_tcp_{
    HANDLE_FIELDS
    NETWORK_FIELDS
    apc_connect_req *connect_req;
    int accepted_fd;
    apc_on_connection on_connection;
};

struct apc_udp_{
    HANDLE_FIELDS
    NETWORK_FIELDS
    struct sockaddr_storage peer;
};

struct apc_timer_{
    HANDLE_FIELDS
    struct{
        void *left;
        void *right;
        void *parent;
    }node;
    time_t end;
    time_t duration;
    int restart;
    size_t id;
    apc_on_timeout cb;
};

struct apc_stat_{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid; 
    dev_t st_rdev;
    off_t st_size;
    long int st_blksize;
    blkcnt_t  st_blocks;
};

typedef enum apc_request_type_ {APC_WRITE, APC_CONNECT, APC_WORK, APC_REQ_MAX} apc_req_type;

#define REQ_FIELDS      \
    void *data;         \
    apc_req_type type;  \

struct apc_write_req_{
    REQ_FIELDS;
    apc_handle *handle;
    void *write_queue[2];
    apc_buf *bufs;
    size_t nbufs;
    size_t write_index;
    apc_on_write on_write;
    int err;
    struct sockaddr_storage peer;
    apc_buf tmp;
};

struct apc_connect_req_{
    REQ_FIELDS
    apc_tcp *handle;
    apc_on_connected connected;
};

#define WORK_FIELDS                     \
    apc_loop *loop;                     \
    void *work_queue[2];                \
    apc_work work;                      \
    apc_work done;                      \

struct apc_work_req_{
    REQ_FIELDS
    WORK_FIELDS
};

typedef enum apc_file_op_ {APC_FILE_READ, APC_FILE_WRITE, APC_FILE_OP_MAX} apc_file_op;

struct apc_file_op_req_{
    REQ_FIELDS
    WORK_FIELDS
    apc_buf *bufs;
    size_t nbufs;
    off_t offset;
    ssize_t err;
    apc_on_file_op cb;
    apc_file *file;
    apc_file_op op;
};

struct apc_file_{
    HANDLE_FIELDS
    int fd;
    void *work_queue[2];
    const char *path;
    struct apc_stat_ *stat;
    int active_work;
    int oflags;
};

typedef enum apc_file_flags_ {
    APC_OPEN_R = 1, 
    APC_OPEN_W = 2, 
    APC_OPEN_RW = 4, 
    APC_OPEN_CREATE = 8, 
    APC_OPEN_APPEND = 16, 
    APC_OPEN_TMP = 32, 
    APC_OPEN_TRUNC = 64,
    APC_FILE_FLAGS_MAX = 128
}apc_file_flags;

int apc_loop_init(apc_loop *loop);
void apc_loop_run(apc_loop *loop);
int apc_close(apc_handle *handle, apc_on_closing cb);

const char *apc_strerror(enum apc_error_code_ err);

int apc_set_allocator(											\
	void (*cust_free)(void *ptr),                               \
	void *(*cust_malloc)(size_t size),							\
	void *(*cust_calloc)(size_t n, size_t size),				\
	void *(*cust_realloc)(void *ptr, size_t size)				\
);

apc_buf apc_buf_init(void *base, size_t len);

int apc_tcp_init(apc_loop *loop, apc_tcp *tcp);
int apc_tcp_bind(apc_tcp *tcp, const char *port);
int apc_tcp_connect(apc_tcp *tcp, apc_connect_req *req, const char *host, const char *service, apc_on_connected cb);
int apc_tcp_write(apc_tcp *tcp, apc_write_req *req, const apc_buf bufs[], size_t nbufs, apc_on_write cb);
int apc_tcp_start_read(apc_tcp *tcp, apc_alloc alloc, apc_on_read on_read);
int apc_tcp_stop_read(apc_tcp *tcp);

int apc_listen(apc_tcp *tcp, int backlog, apc_on_connection cb);
int apc_accept(apc_tcp *server, apc_tcp *client);

int apc_udp_init(apc_loop *loop, apc_udp *udp);
int apc_udp_bind(apc_udp *udp, const char *port);
int apc_udp_connect(apc_udp *udp, const char *host, const char *service);
int apc_udp_write(apc_udp *udp, apc_write_req *req, const apc_buf bufs[], size_t nbufs, apc_on_write cb);
int apc_udp_start_read(apc_udp *udp, apc_alloc alloc, apc_on_read on_read);
int apc_udp_stop_read(apc_udp *udp);

int apc_add_work(apc_loop *loop, apc_work_req *req, apc_work work, apc_work done);

int apc_file_init(apc_loop *loop, apc_file *file);
int apc_file_open(apc_file *file, const char *path, apc_file_flags flags);
int apc_file_stat(apc_file *file);
int apc_file_read(apc_file *file, apc_file_op_req *req, apc_buf bufs[], size_t nbufs, apc_on_file_op cb);
int apc_file_write(apc_file *file, apc_file_op_req *req, apc_buf bufs[], size_t nbufs, apc_on_file_op cb);
int apc_file_pread(apc_file *file, apc_file_op_req *req, apc_buf bufs[], size_t nbufs, apc_on_file_op cb, off_t offset);
int apc_file_pwrite(apc_file *file, apc_file_op_req *req, apc_buf bufs[], size_t nbufs, apc_on_file_op cb, off_t offset);
int apc_file_link_tmp(apc_file *file, const char *path);

int apc_timer_init(apc_loop *loop, apc_timer *timer);
int apc_timer_start(apc_timer *timer, time_t duration, int restart, apc_on_timeout cb);
int apc_timer_stop(apc_timer *timer);
int apc_timer_restart(apc_timer *timer);

#undef HANDLE_FIELDS
#undef NETWORK_FIELDS
#undef REQ_FIELDS
#undef WORK_FIELDS

#endif