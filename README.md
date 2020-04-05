# Libapc

I made this project in order to understand the machanisms of [libuv](https://github.com/libuv/libuv), in doing so I copied some parts of the code of libuv.
If you want to use an event loop in production go to libuv.

Howerver if your interested in an simpler version of an event loop feel free to check this project out. I written 
and tested the code on linux (epoll is used as poll mechanism), so other operating systems are not suported.

## What can libapc do

There are 5 handle types:
    Base handle `apc_handle` (each handle below is also an `apc_handle`),
    TCP `apc_tcp`,
    UDP `apc_udp`,
    Timer `apc_timer` and
    File `apc_file`.

On these handles are then issued requests:
    `apc_connect_req` on the TCP handle,
    `apc_write_req` on the TCP and UDP handles,
    `apc_file_op_req` on the File handle.

Additionally a threadpool is provided where the user can submit some work with an `apc_wor_req`
request type.

A few example programs are provided in the example folder.

## How does it work

![Alt diagram of the inner workings](./docs/apc_loop_diagram.svg)

## Usage

A Makefile is provided. Create the static library `libapc.a` with `make lib`. Include the header file in your project an link to the library as well as to pthread while compiling.

### Basic functions
Initialize a loop with:
```C
    int apc_loop_init(apc_loop *loop)
```

Run the loop with:
```C
    void apc_loop_run(apc_loop *loop)
```

Close any handle with (typecast to `apc_handle *`):
```C
    int apc_close(apc_handle *handle)
```

Each function return either 0 on succes or an error code.
This function returns a short explanation for a given error code err:
```C
    const char *apc_strerror(enum apc_error_code_ err)
```

If you want to use a custom memory allocator you can pass the corresponding
functions to libapc so that they get used for internal memory allocations.
Default is `free(), malloc(), calloc(), realloc`. This function doesn't accept `NULL` values.
```C
   int apc_set_allocator(									
	void (*cust_free)(void *ptr), void *(*malloc)(size_t size), 
	void *(*cust_malloc)(size_t size),							
	void *(*cust_calloc)(size_t n, size_t size),				
	void *(*cust_realloc)(void *ptr, size_t size)				
  )
```

Init a basic buffer with:
```C
    apc_buf apc_buf_init(void *base, size_t len)
```
The base pointer isnt't freed by libapc, that is the users responsibility.

### Basic types
___

The loop type.
```C
    apc_loop
```

Base handle, each specific handle is a base handle.
The shown members are 'public'. The data pointer is meant for some user data and gets not
touched by libapc.
```C
    apc_handle{
        void *data;    
        apc_loop *loop;
        apc_handle_type type; 
    }
```

A write request used for writing to a TCP or UDP handle.
```C
    apc_write_req 
```

A connect request used for connecting to a remote host. Only with TCP.
```C
    apc_connect_req
```

A work request used for submitting work to the threadpool.
```C
    apc_work_req 
```

A file operation reequest used for either writing or reading from a file.
```C
    apc_file_op_req 
```

The basic buffer used by libapc. The base pointer an len are set by the user.
```C
    apc_buf{
        char *base;
        size_t len;
    }
```

### Callback types
___

The `apc_alloc` callback type gets called each time before data is read from either a TCP or UDP handle.
The memory should be freed in the folowed `apc_on_read` callback.
```C
    void (*apc_alloc)(apc_handle *handle, apc_buf *buf)
```
Usually the implemented callback looks like this:
```C
    void my_alloc(apc_handle *handle, apc_buf *buf){
        buf->base = malloc(128 * sizeof(char));
        buf->len = 128 * sizeof(char);
    }
```

The `apc_on_read` callback type gets called each time there is data to beread from a TCP or UDP handle.
```C
    void (*apc_on_read)(apc_handle *handle, apc_buf *buf, ssize_t nread)
```

The `apc_on_connection` callback is used when implementing a TCP server. Each time a new client connects,
a function of this type gets called.
```C
   void (*apc_on_connection)(apc_tcp *tcp, int error) 
```

The `apc_on_connected` callback type gets called when the TCP handle connected to a remote host or an
error occured doing so.
```C
    void (*apc_on_connected)(apc_tcp *tcp, apc_connect_req *req, int error)  
```

The `apc_on_write` callback type gets called when a write to a TCP or UDP handle has finished, either 
successfull or with an error.
```C
    void (*apc_on_write)(apc_write_req *req, apc_buf *bufs, int error)
```

The `apc_work` callback type is used for submitted work.
```C
    void (*apc_work)(apc_work_req *work)
```

The `apc_on_file_op` callback type gets called on a finnished read or write operation on a file handle.
```C
    void (*apc_on_file_op)(apc_file *file, apc_file_op_req *req, apc_buf *bufs, ssize_t nbytes)
```
The bufs pointer either contains the read data or the written data. Allocated memory should be freed
here.

The `apc_on_timeout` callback type gets called on a due timer.
```C
    void (*apc_on_timeout)(apc_timer *handle) 
```

### TCP functions
___

The TCP handle type
```C
    apc_tcp
```

Initialize a TCP handle with:
```C
   int apc_tcp_init(apc_loop *loop, apc_tcp *tcp)
```
No socket is created in this call.

Bind the TCP handle to `port` with:
```C
   int apc_tcp_bind(apc_tcp *tcp, const char *port)
```
In this function the socket is created.

Connect the TCP handle to a `host` on `service` with:
```C
   int apc_tcp_connect(apc_tcp *tcp, apc_connect_req *req, const char *host, const char *service, apc_on_connected cb) 
```
Service can be a port number or a protocoll like http.

Write to the TCP handle wit:
```C
    int apc_tcp_write(apc_tcp *tcp, apc_write_req *req, const apc_buf bufs[], size_t nbufs, apc_on_write cb)
```
The content of bufs is copied so only the memory pointed to by each buf with base needs to
stay valid till the callback.

Get a callback called each time there is data to be read from the TCP handle with:
```C
    int apc_tcp_start_read(apc_tcp *tcp, apc_alloc alloc, apc_on_read on_read)
```

Stop invoking a callback if there is data to be read from the TCP handle with:
```C
   int apc_tcp_stop_read(apc_tcp *tcp) 
```

```C
    
```
### UDP functions
___

UDP handle
```C
    apc_udp 
```

### File functions
___

File handle
```C
    apc_file
```

### Timer functions
___

Timer handle
```C
    apc_timer
```