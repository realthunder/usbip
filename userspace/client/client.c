/*
 * Copyright (C) 2013 Zheng, Lei
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <endian.h>
#include <string.h>
#include <assert.h>
#include "uv.h"
#define _GNU_SOURCE
#include "deps/bsd/queue.h"
#include "deps/pt/pt.h"
#include "../../usbip_struct.h"
#include "../libsrc/usbip_struct.h"

#define USBIP_STRUCT_PACK
#include "../../usbip_struct.h"
#include "../libsrc/usbip_struct.h"
#undef USBIP_STRUCT_PACK
#define USBIP_STRUCT_UNPACK
#include "../../usbip_struct.h"
#include "../libsrc/usbip_struct.h"
#undef USBIP_STRUCT_UNPACK

#include "libusbip-client.h"
#include "../../usbip_struct_helper.h"

#define DEVICE_BLOCK_SIZE 32
#define array_sizeof(_a) (sizeof(_a)/sizeof(_a[0]))

#define container_of(ptr, type, member) \
    ((type *) ((char *)(ptr) - offsetof(type, member)))

#define device_s libusbip_device_s
#define device_t libusbip_device_t
#define device_info_t libusbip_device_info_t
#define session_t libusbip_session_t
#define session_s libusbip_session_s
#define callback_t libusbip_callback_t
#define work_t libusbip_work_t
#define work_s libusbip_work_s
#define host_t libusbip_host_t
#define job_t libusbip_job_t

#undef err
#undef dbg
#define err(msg,...) fprintf(stderr,"err %s(%d) - " msg "\n",__func__,__LINE__,##__VA_ARGS__)
#define dbg(msg,...) fprintf(stderr,"dbg %s(%d) - " msg "\n",__func__,__LINE__,##__VA_ARGS__)
#define work_dbg_(_work,msg,...) dbg("%s(%p): " msg, _work->cb_name,_work->data,##__VA_ARGS__)
#define work_dbg(msg,...) work_dbg_(work,msg,##__VA_ARGS__)
#define work_trace dbg("%s(%p)", work->cb_name,work->data)
#define device_dbg(msg,...) dbg("device(%p): " msg, dev,##__VA_ARGS__)
#define device_trace dbg("device(%p)",dev)

enum device_state_t {
    devs_closed = 0,
    devs_opening = 1,
    devs_running = 2,
    devs_closing = 3,
};

USBIP_STRUCT_BEGIN(usb_ctrlrequest_s)
	uint8_t bRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
USBIP_STRUCT_END
typedef struct usb_ctrlrequest_s usb_ctrlrequest_t;

typedef struct urb_s {
    TAILQ_ENTRY(urb_s) node;
    struct pt pt;
    work_t *work;
    device_t *dev;
    struct usbip_header header;
    uint32_t direction;
    uint32_t seqnum;
    uv_buf_t buf;
    uv_write_t req_write;
    int *ret;
}urb_t;

#define obj_ref(_name,_obj) \
    do {\
        ++(_obj)->ref_count;\
        _name##_dbg(#_name " ref %d",(_obj)->ref_count);\
    }while(0)

#define obj_unref(_name,_obj) \
    do {\
        --(_obj)->ref_count;\
        _name##_dbg(#_name " unref %d",(_obj)->ref_count);\
        assert((_obj)->ref_count>=0);\
        if((_obj)->ref_count == 0)\
            _name##_del(_obj);\
    }while(0)

struct device_s { 
    SLIST_ENTRY(device_s) node;
    session_t *session;
    uv_tcp_t socket;
    struct sockaddr_in addr;
    uint32_t devid;
    uint32_t seqnum;
    struct pt pt;
    int ref_count;
    int state;
    union {
        struct {
            uv_write_t req_write;
            uv_connect_t req_connect;
            struct op_common op_header;
            struct op_import_request req_import;
            struct op_import_reply rpl_import;
        }connect;
        struct {
            struct usbip_header header;
            urb_t *urb;
            char buf[64*1024];
        }recv;
    };

    TAILQ_HEAD(urb_list_s, urb_s) urb_list;
};
#define device_ref(_dev) obj_ref(device,_dev)
#define device_unref(_dev) obj_unref(device,_dev)


/* The main purpose of this memory block is not for efficient
 * memory allocation, but for identification of the device object
 * ownership given a void pointer. See libusbip_device_get
 */
typedef struct device_block_s {
    SLIST_ENTRY(device_block_s) node;
    device_t devices[DEVICE_BLOCK_SIZE];
}device_block_t;

typedef struct sync_s {
    SLIST_ENTRY(sync_s) node;
    uv_mutex_t mutex;
    uv_cond_t cond;
}sync_t;

enum work_state_t {
    ws_pending,
    ws_busy,
    ws_done,
    ws_aborting,
};

typedef struct work_s work_t;

enum work_action_type_t {
    wa_process,
    wa_abort,
    wa_destroy,
};
typedef int (*work_cb_t)(work_t *, int action);
struct work_s {
    TAILQ_ENTRY(work_s) node;
    int ref_count;
    uv_timer_t timer;
    int timeout;
    sync_t *sync;
    session_t *session;
    int state;
    int ret;
    void *data;
    work_cb_t cb;
    const char *cb_name;
    union {
        int status;
        ssize_t nread;
    };
    uv_buf_t buf;

    callback_t user_cb;
    void *user_data;
};
#define work_ref(_work) obj_ref(work,_work)
#define work_unref(_work) obj_unref(work,_work)

static uv_buf_t alloc_cb(uv_handle_t *handle, size_t suggested_size) {
    work_t *work = (work_t*)handle->data;
    work_dbg("alloc cb %d,%u",work->nread,work->buf.len);
    return uv_buf_init(work->buf.base+work->nread,
            work->buf.len-work->nread);
}

static void read_cb(uv_stream_t *handle, ssize_t nread, uv_buf_t buf) {
    work_t *work = (work_t*)handle->data;
    work_dbg("read cb %d,%u",nread,buf.len);
    if(nread<0)
        work->nread = nread;
    else{
        work->nread += nread;
        if(nread < buf.len)
            return;
    }
    work->cb(work,wa_process);
}

void write_cb(uv_write_t* req, int status) {
    work_t *work = (work_t *)req->data;
    work->status = status;
    work->cb(work,wa_process);
}

void connect_cb(uv_connect_t* req, int status) {
    work_t *work = (work_t *)req->data;
    work->status = status;
    work->cb(work,wa_process);
}

typedef struct host_node_s {
    LIST_ENTRY(host_node_s) node;
    host_t host;
}host_node_t;

typedef struct session_s {
    uv_thread_t thread;
    uv_loop_t *loop;
    uv_async_t notify;
    int active;

    uv_mutex_t work_mutex;
    TAILQ_HEAD(work_queue_s, work_s) work_queue;

    uv_mutex_t mem_mutex;
    SLIST_HEAD(device_blocks_s,device_block_s) device_blocks;
    SLIST_HEAD(free_sync_list_s,sync_s) free_sync_list;
    SLIST_HEAD(free_device_list_s, device_s) free_device_list;
    TAILQ_HEAD(free_urb_list_s, urb_s) free_urb_list;
    TAILQ_HEAD(free_work_list_s, work_s) free_work_list;

    LIST_HEAD(host_list_s,host_node_s) host_list;
    size_t host_count;
}session_t;

static void work_del(work_t *work);

static void timer_close_cb(uv_handle_t *handle) {
    work_t *work = container_of(handle,work_t,timer);
    work->timer.data = 0;
    work_unref(work);
}

static void work_del(work_t *work) {
    session_t *session = work->session;
    if(work->timer.data){
        work_dbg("close timer");
        work_ref(work);
        uv_close((uv_handle_t*)&work->timer,timer_close_cb);
        return;
    }
    work_trace;
    work->cb(work,wa_destroy);
    uv_mutex_lock(&session->mem_mutex);
    if(work->sync) 
        SLIST_INSERT_HEAD(&session->free_sync_list,work->sync,node);
    TAILQ_INSERT_HEAD(&session->free_work_list,work,node);
    uv_mutex_unlock(&session->mem_mutex);
}

#define work_done(_work) \
    do{\
        work_dbg_(_work,"work done");\
        work_done_(_work);\
    }while(0)

static inline void work_done_(work_t *work) {
    if(work->timer.data) 
        uv_timer_stop(&work->timer);
    if(work->sync)
        uv_mutex_lock(&work->sync->mutex);
    work->state = ws_done;
    if(work->sync) {
        uv_cond_signal(&work->sync->cond);
        uv_mutex_unlock(&work->sync->mutex);
    }
    if(work->user_cb)
        work->user_cb(work->user_data,work->ret);
    work_unref(work);
}

static void timer_cb(uv_timer_t *timer, int status) {
    work_t *work = (work_t*)timer->data;
    if(!status) {
        work_dbg("work timeout");
        work->cb(work,wa_abort);
    }else
        work_dbg("work timeout error %d",status);
}

#define work_new(_s,_d,_cb,_t,_j) \
    work_new_(_s,_d,_cb,#_cb,_t,_j)

static work_t *work_new_(session_t *session, void *data, 
        work_cb_t cb, const char *cb_name, int timeout, job_t *job) 
{
    work_t *work;
    if(job && !job->cb) {
        err("job has no callback");
        return NULL;
    }

    uv_mutex_lock(&session->mem_mutex);
    work = TAILQ_FIRST(&session->free_work_list);
    if(work == NULL)
        work = (work_t *)malloc(sizeof(work_t));
    else
        TAILQ_REMOVE(&session->free_work_list,work,node);
    memset(work,0,sizeof(work_t));
    if(!job) {
        work->sync = SLIST_FIRST(&session->free_sync_list);
        if(work->sync == NULL){
            work->sync = (sync_t *)malloc(sizeof(sync_t));
            uv_mutex_init(&work->sync->mutex);
            uv_cond_init(&work->sync->cond);
        }else
            SLIST_REMOVE(&session->free_sync_list,work->sync,sync_s,node);
    }
    uv_mutex_unlock(&session->mem_mutex);

    work_ref(work); /* unref by work_done */
    work_ref(work); /* unref by libusbip_work_close */
    work->session = session;
    work->data = data;
    work->cb = cb;
    work->cb_name = cb_name;
    work->state = ws_pending;
    if(job) {
        work->user_cb = job->cb;
        work->user_data = job->data;
        job->work = work;
    }
    work->timeout = timeout;
    uv_mutex_lock(&session->work_mutex);
    TAILQ_INSERT_TAIL(&session->work_queue,work,node);
    uv_mutex_unlock(&session->work_mutex);
    uv_async_send(&session->notify);
    return work;
} 

static int work_abort_cb(work_t *work, int action) {
    work_t *target = (work_t*)work->data;
    if(action == wa_process) {
        work_trace;
        if(target->state == ws_busy)
            target->cb(target,wa_abort);
        if(target->state != ws_done)
            target->state = ws_aborting;
        work_done(work);
    }
    return 0;
}

int libusbip_work_abort(work_t *work) {
    work_trace;
    work_new(work->session,work,work_abort_cb,0,0);
    return 0;
}

static int work_unref_cb(work_t *work,int action) {
    if(action == wa_process) {
        work_unref((work_t*)work->data);
        work_done(work);
    }
    return 0;
}

void libusbip_work_close(work_t *work) {
    work_trace;
    /* since we are not in the loop thread, must unref the work asynchronously */
    work_new(work->session,work,work_unref_cb,0,0);
}

static inline int work_is_done(work_t *work) {
    return work->state==ws_done;
}

static void inline device_del(device_t *dev) {
    session_t *session = dev->session;
    uv_mutex_lock(&session->mem_mutex);
    SLIST_INSERT_HEAD(&session->free_device_list,dev,node);
    uv_mutex_unlock(&session->mem_mutex);
}

static inline void urb_del(urb_t *urb) {
    session_t *session = urb->dev->session;
    uv_mutex_lock(&session->mem_mutex);
    TAILQ_INSERT_HEAD(&session->free_urb_list,urb,node);
    uv_mutex_unlock(&session->mem_mutex);
}

static void socket_close_cb(uv_handle_t *handle) {
    urb_t *urb,*tmp; 
    device_t *dev = container_of(handle,device_t,socket);
    dev->state = devs_closed;
    TAILQ_FOREACH_SAFE(urb,&dev->urb_list,node,tmp) {
        TAILQ_REMOVE(&dev->urb_list,urb,node);
        urb->work->ret = -1;
        work_done(urb->work);
    }
    device_unref(dev);
}

#define device_close(_dev) \
    do{\
        if(_dev->state!=devs_closed && _dev->state!=devs_closing) {\
            _dev->state = devs_closing;\
            device_ref(_dev);\
            device_dbg("close socket");\
            uv_close((uv_handle_t*)&_dev->socket,socket_close_cb);\
        }\
    }while(0)

static int work_submit(session_t *session, void *data, work_cb_t cb, 
        int timeout, job_t *job)
{
    int ret;
    work_t *work = work_new(session,data,cb,timeout,job);
    if(work == NULL) return -1;
    if(work->sync) {
        uv_mutex_lock(&work->sync->mutex);
        do{
            uv_cond_wait(&work->sync->cond,&work->sync->mutex);
        }while(!work_is_done(work));
        uv_mutex_unlock(&work->sync->mutex);
        ret = work->ret;
        libusbip_work_close(work);
        return ret;
    }
    return 0;
}

static void session_notify(uv_async_t *handle, int status) {
    session_t *session = (session_t*)handle->data;
    int code;
    work_t *work;
    while(1) {
        uv_mutex_lock(&session->work_mutex);
        work = TAILQ_FIRST(&session->work_queue);
        if(!work) {
            uv_mutex_unlock(&session->work_mutex);
            break;
        }
        TAILQ_REMOVE(&session->work_queue,work,node);
        uv_mutex_unlock(&session->work_mutex);
        if(work->state == ws_pending) {
            work->state = ws_busy;
            /* TODO: do we need to adjust the timeout in case
             * the work is queued for too long??
             */
            if(work->timeout) {
                uv_timer_init(session->loop,&work->timer);
                work->timer.data = work;
                uv_timer_start(&work->timer,timer_cb,work->timeout,0);
            }
        }
        work->cb(work,wa_process);
    }
}

static void session_loop(void *arg) {
    uv_run(((session_t*)arg)->loop,UV_RUN_DEFAULT);
}

void libusbip_exit(session_t *session) {
    device_block_t *block,*tmp;
    host_node_t *h,*htmp;
    
    if(session->active) {
        uv_stop(session->loop);
        uv_thread_join(&session->thread);
        session->active = 0;
    }

    if(session->loop) {
        uv_loop_delete(session->loop);
        session->loop = NULL;
    }

    SLIST_FOREACH_SAFE(block,&session->device_blocks,node,tmp) {
        SLIST_REMOVE(&session->device_blocks,block,device_block_s,node);
        free(block);
    }

    LIST_FOREACH_SAFE(h,&session->host_list,node,htmp) {
        LIST_REMOVE(h,node);
        free(h);
    }

    uv_mutex_destroy(&session->work_mutex);
    uv_mutex_destroy(&session->mem_mutex);
}

int libusbip_init(session_t **_session) {
    char *hosts = getenv("LIBUSBIP_HOSTS");
    int ret;
    session_t *session = (session_t*)calloc(1,sizeof(session_t));

    if(hosts) {
        size_t len;
        char tmp[20];
        host_t host;
        const char *end;
        const char *port;
        while(1) {
            memset(&host,0,sizeof(host));
            end = strchr(hosts,';');
            port = strchr(hosts,'/');
            if(end) {
                if(port && port<end){
                    len = port-hosts-1;
                    strncpy(tmp,port,end-port-1);
                    port = tmp;
                }else{
                    port = NULL;
                    len = end-hosts;
                }
            }else if(port)
                len = port-hosts;
            else
                len = strlen(hosts);
            if(port)
                sscanf(port,"%d",&host.port);
            if(len>=sizeof(host.addr))
                err("host name too long: %*s",len,hosts);
            else {
                strncpy(host.addr,hosts,len);
                host.addr[len] = 0;
                libusbip_add_hosts(session,&host,1);
            }
            if(!hosts[len]) break;
            hosts += len+1;
        }
    }
    uv_mutex_init(&session->mem_mutex);
    SLIST_INIT(&session->device_blocks);
    SLIST_INIT(&session->free_device_list);
    TAILQ_INIT(&session->free_urb_list);
    SLIST_INIT(&session->free_sync_list);
    TAILQ_INIT(&session->free_work_list);

    uv_mutex_init(&session->work_mutex);
    TAILQ_INIT(&session->work_queue);

    session->loop = uv_loop_new();
    if(!session->loop) {
        err("failed to get uv loop");
        libusbip_exit(session);
        return -1;
    }
    session->notify.data = session;
    uv_async_init(session->loop,&session->notify,session_notify);

    if((ret=uv_thread_create(&session->thread,session_loop,session))) {
        err("failed to create thread %d",ret);
        libusbip_exit(session);
        return -1;
    }
    session->active = 1;
    *_session = session;
    return 0;
}

/* check if the given pointer is a libusbip_device_t,
 * no alignment check yet. */
device_t *libusbip_device_cast(session_t *session, void *pt) {
    device_t *dev = (device_t*)pt;
    device_t *ret = 0;
    device_block_t *block;

    uv_mutex_lock(&session->mem_mutex);
    SLIST_FOREACH(block,&session->device_blocks,node) {
        if(dev>=block->devices && 
           dev<(block->devices+array_sizeof(block->devices))) {
            ret = dev;
            break;
        }
    }
    uv_mutex_unlock(&session->mem_mutex);
    return ret;
}

#define WORK_EXIT \
    do{\
        work_done(work);\
        return PT_EXITED;\
    }while(0)

#define WORK_ABORT(_ret) \
    do{\
        err("work abort");\
        work->ret = _ret;\
        work->cb(work,wa_abort);\
        WORK_EXIT;\
    }while(0)

#define WORK_YIELD(pt) \
    do{\
        if(ret < 0){\
            err("async op failed %d",ret);\
            WORK_ABORT(-1); \
        }else{\
            work_dbg("yield");\
            PT_YIELD(pt);\
            work_dbg("resume");\
        }\
        if(work->status < 0){ \
            uv_err_t err = uv_last_error(session->loop);\
            err("work error %d, %d,%s,%s",work->status,\
                    err.code,uv_err_name(err),uv_strerror(err));\
            WORK_ABORT(-2);\
        }\
    }while(0)

#define READ_PREPARE_(_buf,_size) \
    do{\
        work->nread = 0;\
        work->buf = uv_buf_init((char*)(_buf),_size);\
        work_dbg("read prepare %d",_size);\
    }while(0)

#define READ_PREPARE(_obj) READ_PREPARE_(&(_obj),sizeof(_obj))

#define WORK_BEGIN(pt) \
    PT_BEGIN(pt);\
    if(work->state != ws_busy) {\
        work->ret = -1;\
        work_dbg("abort on entry");\
        WORK_EXIT;\
    }\
    work_dbg("begin");

#define WORK_END(pt) PT_END(pt)

static inline int device_find_urb(device_t *dev) {
    urb_t *urb;
    TAILQ_FOREACH(urb,&dev->urb_list,node) {
        if(urb->work==NULL || 
           urb->seqnum != dev->recv.header.base.seqnum)
            continue;
        dev->recv.urb = urb;
        return 1;
    }
    err("failed to find urb seq num %u",
            dev->recv.header.base.seqnum);
    return 0;
}

static int device_loop(work_t *work, int action) {
    int ret = 0;
    session_t *session = work->session;
    device_t *dev = (device_t*)work->data;

    if(action == wa_abort) {
        dbg("abort device loop");
        device_close(dev);
        return 0;
    }
    if(action == wa_destroy) {
        device_close(dev);
        device_unref(dev);
        return 0;
    }
    if(action != wa_process)
        return -1;

    WORK_BEGIN(&dev->pt);

    dev->socket.data = work;
    ret =  uv_read_start((uv_stream_t*)&dev->socket,alloc_cb,read_cb);

    while(1) {
        READ_PREPARE(dev->recv.header.base);
        WORK_YIELD(&dev->pt);
        unpack_usbip_header_basic(&dev->recv.header.base);
        if(dev->recv.header.base.command == USBIP_RET_SUBMIT) {
            READ_PREPARE(dev->recv.header.u.ret_submit);
            WORK_YIELD(&dev->pt);
            unpack_usbip_header_ret_submit(&dev->recv.header.u.ret_submit);
            if(!device_find_urb(dev)) {
                if(dev->recv.header.u.ret_submit.status == 0 && 
                    dev->recv.header.base.direction == USBIP_DIR_IN) {
                    dbg("drop read %u",dev->recv.header.u.ret_submit.actual_length);
                    while(dev->recv.header.u.ret_submit.actual_length) {
                        if(dev->recv.header.u.ret_submit.actual_length<sizeof(dev->recv.buf))
                            READ_PREPARE_(dev->recv.buf, 
                                    dev->recv.header.u.ret_submit.actual_length);
                        else
                            READ_PREPARE_(dev->recv.buf, sizeof(dev->recv.buf));
                        WORK_YIELD(&dev->pt);
                        dev->recv.header.u.ret_submit.actual_length -= 
                            work->nread;
                    }
                }
                continue;
            }
            if(dev->recv.header.u.ret_submit.status == 0 && 
                dev->recv.header.base.direction == USBIP_DIR_IN) 
            {
                READ_PREPARE_(dev->recv.urb->buf.base,
                        dev->recv.header.u.ret_submit.actual_length);
                WORK_YIELD(&dev->pt);
            }
            dev->recv.urb->header.u.ret_submit = 
                dev->recv.header.u.ret_submit;
            TAILQ_REMOVE(&dev->urb_list,dev->recv.urb,node);
            *dev->recv.urb->ret = dev->recv.header.u.ret_submit.actual_length;
            work_done(dev->recv.urb->work);
            dev->recv.urb = NULL;

        /* no unlink support yet. Just drop connection on any error
        }else if(dev->recv.header.base.command == USBIP_RET_UNLINK) {
        */

        }else{
            err("invalid rx command %u",dev->recv.header.base.command);
            WORK_ABORT(-4);
        }
    }
    WORK_END(&dev->pt);
}

#define CHECK_OP_REPLY(_op,_code) \
    do{\
        if(_op.version!=USBIP_VERSION_NUM) {\
            err("usbip version mismatch %x,%x",USBIP_VERSION_NUM,_op.version);\
            WORK_EXIT;\
        }else if(_op.code!=_code) {\
            err("usbip op code mismatch %d,%d",_code,_op.code);\
            WORK_EXIT;\
        }else if(_op.status!=ST_OK) {\
            err("usip op error %d",_op.status);\
            WORK_EXIT;\
        }\
    }while(0)

static int device_open_cb(work_t *work, int action) {
    char addr[64];
    session_t *session = work->session;
    uv_buf_t buf[2];
    int ret = 0;
    device_t *dev = (device_t*)work->data;

    if(action == wa_abort) {
        dbg("abort device open");
        device_close(dev);
        return 0;
    }
    if(action == wa_destroy) {
        device_close(dev);
        device_unref(dev);
        return 0;
    }
    if(action != wa_process)
        return -1;

    WORK_BEGIN(&dev->pt);

    ret = uv_tcp_init(session->loop,&dev->socket);
    if(ret < 0){
        err("tcp init failed");
        WORK_EXIT;
    }
    dev->socket.data = work;
    dev->state = devs_opening;
    dev->connect.req_connect.data = work;
    dbg("connecting to %s",(uv_ip4_name(&dev->addr,addr,sizeof(addr)),addr));
    ret = uv_tcp_connect(&dev->connect.req_connect,
            &dev->socket,dev->addr,connect_cb);
    WORK_YIELD(&dev->pt);

    dev->connect.req_write.data = work;
    dev->connect.op_header.version = USBIP_VERSION_NUM;
    dev->connect.op_header.code = OP_REQ_IMPORT;
    dev->connect.op_header.status = 0;
	pack_op_common(&dev->connect.op_header);
    buf[0] = uv_buf_init((char*)&dev->connect.op_header,
            sizeof(dev->connect.op_header));

    snprintf(dev->connect.req_import.busid,
            sizeof(dev->connect.req_import.busid),
            "%u-%u",dev->devid>>16,dev->devid&0xffff);
    buf[1] = uv_buf_init((char*)&dev->connect.req_import,
            sizeof(dev->connect.req_import));

    dev->connect.req_write.data = work;
    ret = uv_write(&dev->connect.req_write, 
            (uv_stream_t*)&dev->socket, buf, 2, write_cb);
    WORK_YIELD(&dev->pt);

    READ_PREPARE(dev->connect.op_header);
    ret =  uv_read_start((uv_stream_t*)&dev->socket,alloc_cb,read_cb);
    WORK_YIELD(&dev->pt);
    CHECK_OP_REPLY(dev->connect.op_header,OP_REQ_IMPORT);

    READ_PREPARE(dev->connect.rpl_import);
    WORK_YIELD(&dev->pt);

	unpack_op_import_reply(&dev->connect.rpl_import);
	if(strncmp(dev->connect.rpl_import.udev.busid, 
                dev->connect.req_import.busid, 
                sizeof(dev->connect.req_import.busid))) 
    {
        err("recv different bus id");
        WORK_ABORT(-3);
    }

    /* pause the read first, and resume it in device_loop */
    dev->socket.data = 0;
    uv_read_stop((uv_stream_t*)&dev->socket);
    device_ref(dev);
    work_new(work->session,dev,device_loop,0,0);

    work->ret = 0;
    WORK_EXIT;

    WORK_END(&dev->pt);
}

int libusbip_device_open(session_t *session, device_t **pdev,
        const char *addr, int port, const char *busid,
        int timeout, libusbip_job_t *job)
{
    device_t *dev;
    unsigned busnum,devnum;
    if(sscanf(busid,"%u-%u",&busnum,&devnum)!=2){
        err("invalid busid %s",busid);
        return -1;
    }

    uv_mutex_lock(&session->mem_mutex);
    if(SLIST_EMPTY(&session->free_device_list)) {
        int i;
        device_block_t *block = (device_block_t*)malloc(
                sizeof(device_block_t));
        for(i=0;i<array_sizeof(block->devices);++i)
            SLIST_INSERT_HEAD(&session->free_device_list,
                    &block->devices[i],node);
    }
    dev = SLIST_FIRST(&session->free_device_list);
    SLIST_REMOVE_HEAD(&session->free_device_list,node);
    uv_mutex_unlock(&session->mem_mutex);

    memset(dev,0,sizeof(device_t));
    device_ref(dev);
    dev->devid = (busnum<<16|devnum);
    PT_INIT(&dev->pt);
    if(port == 0) port = USBIP_PORT;
    device_dbg("open %s,%d",addr,port);
    dev->addr = uv_ip4_addr(addr,port);
    *pdev = dev;

    device_ref(dev);
    return work_submit(session,dev,device_open_cb,timeout,job);
}

static int device_close_cb(work_t *work, int action) {
    device_t *dev = (device_t*)work->data;
    if(action == wa_process) {
        work_trace;
        device_close(dev);
        device_unref(dev);
        work_done(work);
    }
}

void libusbip_device_close(device_t *dev) {
    device_ref(dev);
    work_submit(dev->session,dev,device_close_cb,0,0);
}

static urb_t *urb_new(device_t *dev) {
    session_t *session = dev->session;
    urb_t *ret = NULL;
    uv_mutex_lock(&session->mem_mutex);
    if(TAILQ_EMPTY(&session->free_urb_list))
        ret = (urb_t *)malloc(sizeof(urb_t));
    else {
        ret = TAILQ_FIRST(&session->free_urb_list);
        TAILQ_REMOVE(&session->free_urb_list,ret,node);
    }
    uv_mutex_unlock(&session->mem_mutex);

    memset(ret,0,sizeof(urb_t));
    PT_INIT(&ret->pt);
    return ret;
}

static int device_urb_transfer(work_t *work, int action) {
    int ret;
    uv_buf_t buf;
    urb_t *urb = (urb_t*)work->data;
    device_t *dev = urb->dev;
    session_t *session = work->session;

    if(action == wa_abort) {
        dbg("abort urb transfer");
        device_close(dev);
        return 0;
    }
    if(action == wa_destroy) {
        urb_del(urb);
        device_unref(dev);
        return 0;
    }
    if(action != wa_process)
        return -1;

    WORK_BEGIN(&urb->pt);

    urb->seqnum = dev->seqnum++;
    urb->direction = urb->header.base.direction;
    urb->work = work;
    urb->header.base.seqnum = urb->seqnum;
    urb->header.base.devid = dev->devid;
    if(urb->header.base.command == USBIP_CMD_SUBMIT) {
        urb->header.u.cmd_submit.transfer_buffer_length = urb->buf.len;
        pack_usbip_header_cmd_submit(&urb->header.u.cmd_submit);
        buf = uv_buf_init((char*)&urb->header,sizeof(urb->header.base)+
                sizeof(urb->header.u.cmd_submit));
    }else{
        err("invalid urb command %u",urb->header.base.command);
        WORK_ABORT(-1);
    }
    pack_usbip_header_basic(&urb->header.base);

    urb->req_write.data = work;
    ret = uv_write(&urb->req_write, (uv_stream_t*)&dev->socket, &buf, 1, write_cb);
    WORK_YIELD(&dev->pt);
    if(urb->buf.len) {
        if(urb->direction == USBIP_DIR_OUT) 
            ret = uv_write(&urb->req_write,
                    (uv_stream_t*)&dev->socket,&urb->buf,1,write_cb);
        else
            READ_PREPARE_(urb->buf.base,urb->buf.len);
        WORK_YIELD(&dev->pt);
    }

    /* work not done yet, put in the device urb list and wait for device_loop 
     * to call work_done on urb */
    TAILQ_INSERT_TAIL(&dev->urb_list,urb,node);
    return PT_EXITED;

    WORK_END(&dev->pt);
}

int libusbip_control_msg(device_t *dev, int requesttype, int request,
	int value, int index, char *bytes, int size, int *ret, int timeout, job_t *job)
{
    usb_ctrlrequest_t *req;
    urb_t *urb = urb_new(dev);
    urb->header.base.command = USBIP_CMD_SUBMIT;
    urb->header.base.direction = 
        requesttype&0x80?USBIP_DIR_IN:USBIP_DIR_OUT;
    urb->header.base.ep = 0;
    urb->buf = uv_buf_init(bytes,size);
    urb->ret = ret;
    *ret = -1;
    req = (usb_ctrlrequest_t*)urb->header.u.cmd_submit.setup;
    req->bRequestType = requesttype;
    req->bRequest = request;
    req->wValue = htole16(value);
    req->wIndex = htole16(index);
    req->wLength = htole16(size);

    device_ref(dev);
    return work_submit(dev->session,urb,device_urb_transfer, timeout,job);
}

int libusbip_urb_transfer(device_t *dev, int ep, int urbtype,
	void *data, int size, int *ret, int timeout, job_t *job)
{
    urb_t *urb = urb_new(dev);
	urb->header.base.command   = USBIP_CMD_SUBMIT;
	urb->header.base.direction = ep&0x80?USBIP_DIR_IN:USBIP_DIR_OUT;
	urb->header.base.ep	= ep&0xff;
    urb->buf = uv_buf_init((char*)data,size);
    urb->ret = ret;
    *ret = -1;
    device_ref(dev);
    return work_submit(dev->session,urb,device_urb_transfer,timeout,job);
}

void libusbip_add_hosts(session_t *session, host_t *hosts, unsigned count)
{
    size_t i;
    uv_mutex_lock(&session->mem_mutex);
    session->host_count += count;
    for(i=0;i<count;++i) {
        host_node_t *h = (host_node_t*)malloc(sizeof(host_node_t));
        h->host = hosts[i];
        dbg("adding static host %s %d",h->host.addr,h->host.port);
        LIST_INSERT_HEAD(&session->host_list,h,node);
    }
    uv_mutex_unlock(&session->mem_mutex);
}

int libusbip_remove_hosts(session_t *session, host_t *hosts, unsigned count)
{
    int ret = -1;
    size_t i;
    uv_mutex_lock(&session->mem_mutex);
    for(i=0;i<count;++i) {
        host_node_t *h;
        LIST_FOREACH(h,&session->host_list,node) {
            if(hosts[i].port!=h->host.port || strcmp(hosts[i].addr,h->host.addr))
                continue;
            LIST_REMOVE(h,node);
            --session->host_count;
            ret = 0;
            break;
        }
    }
    uv_mutex_unlock(&session->mem_mutex);
    return ret;
}

int libusbip_find_hosts(session_t *session, host_t **phosts, 
        int timeout, libusbip_job_t *job)
{
    int i = 0;
    host_t *hosts;
    host_node_t *h;
    if(job) job->work = 0;
    uv_mutex_lock(&session->mem_mutex);
    hosts = (host_t*)malloc(sizeof(host_t)*session->host_count);
    LIST_FOREACH(h,&session->host_list,node)
        hosts[i++] = h->host;
    uv_mutex_unlock(&session->mem_mutex);
    *phosts = hosts;
    return i;
}

void libusbip_free_hosts(session_t *session, host_t *hosts)
{
    free(hosts);
}

typedef struct device_info_req_s {
    struct pt pt;
    struct sockaddr_in addr;
    struct usbip_usb_device udev;
    struct usbip_usb_interface intf;
    int ref_count;
    int active;
    uv_tcp_t socket;
    uv_connect_t connect;
    uv_write_t write;
    struct op_common header;
	struct op_devlist_reply reply;
    device_info_t *info;
    unsigned count;
    device_info_t **pinfo;
    unsigned *pcount;
}device_info_req_t;

#define device_info_t libusbip_device_info_t
#define device_interface_t libusbip_device_interface_t

void libusbip_free_devices_info(session_t *session, 
        device_info_t *info, unsigned count)
{
    size_t i;
    for(i=0;i<count;++i)
        free(info->interfaces);
    free(info);
}
#define device_info_req_ref(_req) obj_ref(device_info_req,_req)
#define device_info_req_unref(_req) obj_unref(device_info_req,_req)
#define device_info_req_del(req) free(req)
#define device_info_req_dbg dbg

static void device_info_req_close_cb(uv_handle_t *handle) {
    device_info_req_t *req = container_of(handle,device_info_req_t,socket);
    device_info_req_unref(req);
}

static inline void device_info_req_close(device_info_req_t *req) {
    if(!req->active) return;
    req->active = 0;
    device_info_req_ref(req);
    dbg("close device info request");
    uv_close((uv_handle_t*)&req->socket,device_info_req_close_cb);
}

static int device_info_req_cb(work_t *work, int action) {
    char addr[64];
    uv_buf_t buf;
    int ret = 0;
    session_t *session = work->session;
    device_info_req_t *req = (device_info_req_t*)work->data;

    if(action == wa_abort) {
        device_info_req_close(req);
        return 0;
    }
    if(action == wa_destroy) {
        if(req->count == 0)
            free(req->info);
        device_info_req_close(req);
        device_info_req_unref(req);
        return 0;
    }
    if(action != wa_process) return -1;

    WORK_BEGIN(&req->pt);
    ret = uv_tcp_init(session->loop,&req->socket);
    if(ret < 0){
        dbg("tcp init failed");
        WORK_EXIT;
    }
    req->active = 1;
    req->socket.data = work;
    req->connect.data = work;
    dbg("connecting to %s",(uv_ip4_name(&req->addr,addr,sizeof(addr)),addr));
    ret = uv_tcp_connect(&req->connect,&req->socket,req->addr,connect_cb);
    WORK_YIELD(&req->pt);

    req->write.data = work;
    req->header.version = USBIP_VERSION_NUM;
    req->header.code = OP_REQ_DEVLIST;
    req->header.status = 0;
    pack_op_common(&req->header);
    buf = uv_buf_init((char*)&req->header, sizeof(req->header));

    req->write.data = work;
    ret = uv_write(&req->write, (uv_stream_t*)&req->socket, &buf, 1, write_cb);
    WORK_YIELD(&req->pt);

    READ_PREPARE(req->header);
    ret =  uv_read_start((uv_stream_t*)&req->socket,alloc_cb,read_cb);
    WORK_YIELD(&req->pt);
    CHECK_OP_REPLY(req->header,OP_REQ_DEVLIST);

    READ_PREPARE(req->reply);
    WORK_YIELD(&req->pt);
    unpack_op_devlist_reply(&req->reply);

    dbg("list device %u",req->reply.ndev);

    *req->pinfo = req->info = (device_info_t *)
        calloc(req->reply.ndev,sizeof(device_info_t));
    for(;req->count<req->reply.ndev;++req->count,*req->pcount=req->count) {
        device_info_t *info;
        READ_PREPARE(req->udev);
        WORK_YIELD(&req->pt);
        unpack_usbip_usb_device(&req->udev);

        dbg("device %u,%u,%s",req->udev.busnum,req->udev.devnum,req->udev.busid);

        info = &req->info[req->count];
        strncpy(info->busid,req->udev.busid,sizeof(info->busid));
        info->busnum=req->udev.busnum;
        info->devnum=req->udev.devnum;
        info->speed=req->udev.speed;
        info->idVendor=req->udev.idVendor;
        info->idProduct=req->udev.idProduct;
        info->bcdDevice=req->udev.bcdDevice;
        info->bDeviceClass=req->udev.bDeviceClass;
        info->bDeviceSubClass=req->udev.bDeviceSubClass;
        info->bDeviceProtocol=req->udev.bDeviceProtocol;
        info->bConfigurationValue=req->udev.bConfigurationValue;
        info->bNumConfigurations=req->udev.bNumConfigurations;
        info->interfaces = (device_interface_t *)calloc(req->udev.bNumInterfaces,
                sizeof(device_interface_t));
        for(;info->bNumInterfaces<req->udev.bNumInterfaces; ++info->bNumInterfaces) {
            device_interface_t *intf;
            READ_PREPARE(req->intf);
            WORK_YIELD(&req->pt);
            unpack_usbip_usb_interface(&req->intf);
            info = &req->info[req->count];
            intf = &info->interfaces[info->bNumInterfaces];
            intf->bInterfaceClass=req->intf.bInterfaceClass;
            intf->bInterfaceSubClass=req->intf.bInterfaceSubClass;
            intf->bInterfaceProtocol=req->intf.bInterfaceProtocol;
        }
    }
    device_info_req_close(req);
    work_done(work);
    return PT_EXITED;

    WORK_END(&req->pt);
}

int libusbip_get_devices_info(session_t *session, host_t *host, 
        device_info_t **info, unsigned *count, 
        int timeout, libusbip_job_t *job)
{
    int ret;
    int port = host->port;
    device_info_req_t *req;
    if(info == 0 || count == 0)
        return -1;
    req = calloc(1,sizeof(device_info_req_t));
    if(port == 0) port = USBIP_PORT;
    req->addr = uv_ip4_addr(host->addr,port);
    PT_INIT(&req->pt);
    req->pinfo = info;
    req->pcount = count;
    device_info_req_ref(req);
    return work_submit(session,req,device_info_req_cb,timeout,job);
}

