#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "apc.h"
#include "fd.h"
#include "fs.h"
#include "apc-internal.h"

#if defined(PATH_MAX)
    #define MY_PATH_MAX PATH_MAX
#else

#endif

static int file_open(apc_file *file, const char *path, apc_file_flags flags){
/*     char result[MY_PATH_MAX]; 
    if(realpath(path, result) == NULL){
        DEBUG_MSG("invalid path\n");
        return APC_EFILEPATH;
    } */
    mode_t mode = 0;
    if(flags & APC_OPEN_R){
        file->oflags |= O_RDONLY;
    }else if(flags & APC_OPEN_W){
        file->oflags |= O_WRONLY;
    }else if(flags & APC_OPEN_RW){
        file->oflags |= O_RDWR;
    }else{
        return APC_EINVAL;
    }

    if(flags & APC_OPEN_TRUNC){
        if(!((flags & APC_OPEN_W) || (flags & APC_OPEN_RW))){
            return APC_EINVAL;
        }
        file->oflags |= O_TRUNC;
    }

    if(flags & APC_OPEN_CREATE){
        if(!((flags & APC_OPEN_W) || (flags & APC_OPEN_RW))){
            return APC_EINVAL;
        }
        file->oflags |= O_CREAT | O_EXCL | O_TRUNC;
        mode |= S_IRUSR | S_IWUSR;
    }

    if(flags & APC_OPEN_APPEND){
        file->oflags |= O_APPEND;
    }

    if(flags & APC_OPEN_TMP){
        file->oflags |= O_TMPFILE;
        mode |= S_IRUSR | S_IWUSR;
    }

    file->fd = open(path, file->oflags | O_CLOEXEC, mode);

    size_t size = strlen(path);
    char *fpath = NULL;
    if(file->fd == -1 && (errno == EISDIR || errno == ENOENT) && (flags & APC_OPEN_TMP)){
        fpath = malloc((14 + size + 1) * sizeof(char));
        if(fpath == NULL){
            return APC_ENOMEM;
        }

        memcpy(fpath, path, size);
        memcpy(fpath + size, "apc-tmp-XXXXXX", 15);
        file->fd = mkostemp(fpath, O_CLOEXEC);
        unlink(fpath);
    }

    if(file->fd == -1){
        if(errno == EEXIST){
            return APC_EFILEEXISTS;
        }
        if(errno == EISDIR || errno == ENOTDIR){
            return APC_EINVALIDPATH;
        }
        return APC_EFILEOPEN;
    }

    if(fpath == NULL){
        fpath = malloc(size * sizeof(char));
        if(fpath == NULL){
            return APC_ENOMEM;
        }
        memcpy(fpath, path, size + 1);
    }

    file->path = fpath;
    return 0;
} 

int apc_file_init(apc_loop *loop, apc_file *file){
    assert(loop != NULL);
    assert(file != NULL);

    apc_handle_init_(file, loop, APC_FILE);
    file->fd = -1;
    file->active_work = 0;
    file->flags = 0;
    file->path = NULL;
    file->stat = NULL;
    QUEUE_INIT(&file->work_queue);
    return 0;
}

int apc_file_open(apc_file *file, const char *path, apc_file_flags flags){
    assert(file != NULL);
    assert(path != NULL);
    assert(file->fd == -1);
    return file_open(file, path, flags);
}

int apc_file_link_tmp(apc_file *file, const char *path){
    assert(file->oflags & O_TMPFILE);
    char fpath[PATH_MAX];
    snprintf(fpath, PATH_MAX,  "/proc/self/fd/%d", file->fd);
    int err = linkat(AT_FDCWD, fpath, AT_FDCWD, path, AT_SYMLINK_FOLLOW);
    if(err < 0){
        if(errno == EEXIST){
            return APC_EFILEEXISTS;
        }

        if(errno == ENOTDIR){
            return APC_EINVALIDPATH;
        }
        return APC_ERROR;
    }
    return 0;
}

static void file_flush_work_queue(apc_file *file){
    while (!QUEUE_EMPTY(get_queue(&file->work_queue))){
        queue *q = QUEUE_NEXT(&file->work_queue);
        apc_file_op_req *op = container_of(q, apc_file_op_req, work_queue);
        QUEUE_REMOVE(q);
        op->err = -1;
        if(op->done != NULL){
            op->done((apc_work_req *) op);
        }
    }    
}

void file_close(apc_file *file){
    file_flush_work_queue(file);  
    QUEUE_INIT(&file->work_queue);
    close(file->fd);              
    file->fd = -1;     
    file->oflags = 0;           
    if(file->path != NULL){       
        free((char *) file->path);
        file->path = NULL;        
    }                               
    if(file->stat != NULL){       
        free(file->stat);         
        file->stat = NULL;        
    }                               
}

static void file_read(apc_work_req *req){
    apc_file_op_req *op = (apc_file_op_req *) req;
    if(op->offset > -1){
        op->err = fd_pread(op->file->fd, op->bufs, op->nbufs, op->offset);
    }else{
        op->err = fd_read(op->file->fd, op->bufs, op->nbufs);
    }
}

static void file_write(apc_work_req *req){
    apc_file_op_req *op = (apc_file_op_req *) req;
    if(op->offset > -1){
        op->err = fd_pwrite(op->file->fd, op->bufs, op->nbufs, op->offset);
    }else{
        op->err = fd_write(op->file->fd, op->bufs, op->nbufs);
    }
}

int apc_file_stat(apc_file *file){
    struct apc_stat_ *apcst = file->stat;
    if(apcst == NULL){
        apcst = malloc(sizeof(struct apc_stat_));
    }
    
    if(apcst == NULL){
        return APC_ENOMEM;
    }
    struct stat st;
    int err = stat(file->path, &st);
    if(err < 0){
        return APC_ERROR;
    }

    apcst->st_dev = st.st_dev;
    apcst->st_ino = st.st_ino;
    apcst->st_mode = st.st_mode;
    apcst->st_nlink = st.st_nlink;
    apcst->st_uid = st.st_uid;
    apcst->st_gid = st.st_gid;
    apcst->st_rdev = st.st_rdev;
    apcst->st_size = st.st_size;
    apcst->st_blksize = st.st_blksize;
    apcst->st_blocks = st.st_blocks;

    file->stat = apcst;
    return 0;
}

static void file_after_op(apc_work_req *req){
    apc_file_op_req *op = (apc_file_op_req *) req;
    apc_file *file = op->file;
    apc_buf *tmp = op->bufs;
    if(op->cb){
        op->cb(op->file, op, op->bufs, op->err);
    }
    
    if(tmp != NULL){
        free(tmp);
    }

    if(!QUEUE_EMPTY(get_queue(&file->work_queue))){
        queue *q = QUEUE_NEXT(get_queue(&file->work_queue));
        op = container_of(q, apc_file_op_req, work_queue);
        QUEUE_REMOVE(q);
        apc_deregister_request_(op, file->loop);
        if(op->op == APC_FILE_READ){
            apc_add_work(file->loop, (apc_work_req *) op, file_read, file_after_op);
        }else if(op->op == APC_FILE_WRITE){
            apc_add_work(file->loop, (apc_work_req *) op, file_write, file_after_op);
        }
        return;
    }
    file->active_work = 0;
    apc_deregister_handle_(file, file->loop);
}

int apc_file_pread(apc_file *file, apc_file_op_req *req, apc_buf bufs[], size_t nbufs, apc_on_file_op cb, off_t offset){
    assert(file != NULL);
    assert(file->fd != -1);
    assert(bufs != NULL);
    assert(nbufs > 0);
    assert(req != NULL);

    req->bufs = malloc(nbufs * sizeof(apc_buf));
    if(req->bufs == NULL){
        return APC_ENOMEM;
    }

    memcpy(req->bufs, bufs, nbufs * sizeof(apc_buf));
    apc_request_init_(req, APC_WORK);
    req->nbufs = nbufs;
    req->cb = cb;
    req->file = file;
    req->err = 0;
    req->offset = offset;
    req->op = APC_FILE_READ;
    req->loop = NULL;
    req->work = NULL;
    req->done = NULL;
    QUEUE_INIT(&req->work_queue);

    if(cb != NULL){
        apc_register_handle_(file, file->loop);
        if(file->active_work == 0){
            file->active_work = 1;
            apc_add_work(file->loop, (apc_work_req *) req, file_read, file_after_op);
        }else{
            apc_register_request_(req, file->loop);
            QUEUE_ADD_TAIL(get_queue(&file->work_queue), get_queue(&req->work_queue));
        }
        return 0;
    }
    file_read((apc_work_req *) req);
    free(req->bufs);
    if(req->err < 0){
        return APC_EFDREAD;
    }
    return 0;
}

int apc_file_pwrite(apc_file *file, apc_file_op_req *req, apc_buf bufs[], size_t nbufs, apc_on_file_op cb, off_t offset){
    assert(file != NULL);
    assert(file->fd != -1);
    assert(bufs != NULL);
    assert(nbufs > 0);

    req->bufs = malloc(nbufs * sizeof(apc_buf));
    if(req->bufs == NULL){
        return APC_ENOMEM;
    }

    memcpy(req->bufs, bufs, nbufs * sizeof(apc_buf));
    apc_request_init_(req, APC_WORK);
    req->nbufs = nbufs;
    req->cb = cb;
    req->file = file;
    req->err = 0;
    req->offset = offset;
    req->op = APC_FILE_WRITE;
    req->loop = NULL;
    req->work = NULL;
    req->done = NULL;
    QUEUE_INIT(&req->work_queue);

    if(cb != NULL){
        apc_register_handle_(file, file->loop);
        if(file->active_work == 0){
            file->active_work = 1;
            apc_add_work(file->loop, (apc_work_req *) req, file_write, file_after_op);
        }else{
            apc_register_request_(req, file->loop);
            QUEUE_ADD_TAIL(get_queue(&file->work_queue), get_queue(&req->work_queue));
        }
        return 0;
    }
    file_write((apc_work_req *) req);
    free(req->bufs);
    if(req->err < 0){
        req->err = APC_EFDWRITE;
    }
    return 0;
}

int apc_file_read(apc_file *file, apc_file_op_req *req, apc_buf bufs[], size_t nbufs, apc_on_file_op cb){
    return apc_file_pread(file, req, bufs, nbufs, cb, -1);
}

int apc_file_write(apc_file *file,apc_file_op_req *req,  apc_buf bufs[], size_t nbufs, apc_on_file_op cb){
    return apc_file_pwrite(file, req, bufs, nbufs, cb, -1);
}