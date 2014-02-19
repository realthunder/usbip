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

#undef log
#undef err

static const char *log_level[] = {
    "none",
    "error",
    "debug",
    "trace",
};

#ifdef LIBUSBIP_NO_DEBUG
#   define log(_l,msg,...) do{}while(0)
#else
#   define log(_l,msg,...) do{\
        if(LIBUSBIP_LOG_##_l>=session->log_level) break;\
        if(session->log_level < LIBUSBIP_LOG_DEBUG)\
            fprintf(stderr,msg "\n",##__VA_ARGS__);\
        else {\
            char _tmp[128];\
            session_time(session,_tmp,sizeof(_tmp));\
            fprintf(stderr,"%s:%d:1 %s %s - %s:" msg "\n", \
                    __FILE__,__LINE__,_tmp,__func__,log_level[LIBUSBIP_LOG_##_l],##__VA_ARGS__);\
        }\
    }while(0)
#endif

#define err(msg,...) log(ERROR,msg,##__VA_ARGS__)
#define err_uv(_ret,msg,...) do{\
    uv_err_t err = uv_last_error(session->loop);\
    err("uv error: %d, %d,%s,%s - " msg, _ret,\
            err.code,uv_err_name(err),uv_strerror(err), ##__VA_ARGS__);\
}while(0)
#define work_log(_log,_work,msg,...) log(_log,"%s(%p): " msg, (_work)->cb_name,(_work)->data,##__VA_ARGS__)
#define work_dbg(msg,...) work_log(DEBUG,work,msg,##__VA_ARGS__)
#define work_trace(msg,...) work_log(TRACE,work,msg,##__VA_ARGS__)
#define device_log(_log,_dev,msg,...) log(_log,"device(%p): " msg, _dev,##__VA_ARGS__)
#define device_dbg(msg,...) device_log(DEBUG,dev,msg,##__VA_ARGS__)
#define device_trace(msg,...) device_log(TRACE,dev,msg,##__VA_ARGS__)
#define device_info_req_log(_log,_req,msg,...) log(_log,msg,##__VA_ARGS__)

enum device_state_t {
    devs_closed = 0,
    devs_closing = 1,
    devs_restarting = 2,
    devs_opening = 3,
    devs_running = 4,
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
        _name##_log(TRACE,_obj,#_name " ref %d",(_obj)->ref_count);\
    }while(0)

#define obj_unref(_name,_obj) \
    do {\
        --(_obj)->ref_count;\
        _name##_log(TRACE,_obj,#_name " unref %d",(_obj)->ref_count);\
        assert((_obj)->ref_count>=0);\
        if((_obj)->ref_count == 0)\
            _name##_del(_obj);\
    }while(0)

struct device_s { 
    SLIST_ENTRY(device_s) node;
    struct device_s **pdev;
    session_t *session;
    uv_tcp_t socket;
    struct sockaddr_in addr;
    uint32_t devid;
    uint32_t seqnum;
    struct pt pt;
    int ref_count;
    int state;
    int max_retry;
    uv_timer_t timer;
    union {
        struct {
            uv_write_t req_write;
            uv_connect_t req_connect;
            struct op_common op_header;
            struct op_import_request req_import;
            struct op_import_reply rpl_import;
            int retry;
        }connect;
        struct {
            struct usbip_header header;
            urb_t *urb;
            char buf[64*1024];
        }recv;
    };

    TAILQ_HEAD(urb_list_s, urb_s) urb_list;
    TAILQ_HEAD(read_list_s, work_s) read_list;
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
    int reading;

    callback_t user_cb;
    void *user_data;
};

typedef struct host_node_s {
    LIST_ENTRY(host_node_s) node;
    host_t host;
}host_node_t;

typedef struct session_s {
    uv_thread_t thread;
    uv_loop_t *loop;
    uv_async_t notify;
    int active;
    int log_level;
    uint64_t start_time;

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

static void session_time(session_t *session, char *buf, size_t len) {
    buf[len-1] = 0;
    if(session->loop) {
        unsigned _t = uv_now(session->loop) - session->start_time;
        snprintf(buf,len-1,"%02u:%02u.%02u",_t/1000/60,_t/1000%60,_t%1000);
    }else
        buf[0] = 0;
}

static void close_cb(uv_handle_t *handle) {
    work_t *work = (work_t*)handle->data;
    work->cb(work,wa_process);
}

static void timer_cb(uv_timer_t *timer, int status) {
    work_t *work = (work_t*)timer->data;
    if(!status) work->cb(work,wa_process);
}

static uv_buf_t alloc_cb(uv_handle_t *handle, size_t suggested_size) {
    work_t *work = (work_t*)handle->data;
    session_t *session = work->session;
    work_dbg("alloc cb %d,%u",work->nread,work->buf.len);
    return uv_buf_init(work->buf.base+work->nread,
            work->buf.len-work->nread);
}

static void read_cb(uv_stream_t *handle, ssize_t nread, uv_buf_t buf) {
    work_t *work = (work_t*)handle->data;
    session_t *session = work->session;
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

#define work_ref(_work) obj_ref(work,_work)
#define work_unref(_work) obj_unref(work,_work)

#define WORK_EXIT do{\
    PT_INIT(__pt);\
    work_done(work);\
    return PT_EXITED;\
}while(0)

#define WORK_ABORT(_ret) do{\
    work_dbg("work abort %d",_ret);\
    work->ret = _ret;\
    work->cb(work,wa_abort);\
    WORK_EXIT;\
}while(0)

#define WORK_ASYNC(_func,_arg) do{\
    if((ret=_func _arg)) {\
        err_uv(ret,#_func " failed");\
        WORK_ABORT(uv_last_error(session->loop).code);\
    }\
}while(0)

#define READ_DONE(_obj) do{\
    if(work->reading) {\
        work->reading = 0;\
        TAILQ_REMOVE(&(_obj)->read_list,work,node);\
    }\
}while(0)

#define WORK_YIELD(_obj) do{\
    work_dbg("yield");\
    PT_YIELD(__pt);\
    work_dbg("resume");\
    READ_DONE(_obj);\
    if(work->ret) {\
        work_dbg("error %d",work->ret);\
        WORK_EXIT;\
    }else if(work->status < 0){ \
        err_uv(work->status,"");\
        WORK_ABORT(uv_last_error(session->loop).code);\
    }\
}while(0)

#define WORK_RESTART do{\
    work_dbg("restart");\
    PT_RESTART(__pt);\
}while(0)

#define READ_PREPARE_(_obj, _buf,_size) do{\
    if(!work->reading) {\
        TAILQ_INSERT_HEAD(&(_obj)->read_list,work,node);\
        work->reading = 1;\
    }\
    work->nread = 0;\
    work->buf = uv_buf_init((char*)(_buf),_size);\
    work_dbg("read prepare %d",_size);\
}while(0)

#define READ_PREPARE(_o,_obj) READ_PREPARE_(_o,&(_obj),sizeof(_obj))

#define WORK_SET_ERR_(_work,_err) do{\
    if(!_work->ret) _work->ret = -_err;\
}while(0)

#define WORK_SET_ERR(_err) WORK_SET_ERR_(work,_err)

#define WORK_BEGIN(_pt) \
{\
    struct pt *__pt = _pt;\
    PT_BEGIN(_pt);\
    if(work->state != ws_busy || work->ret) {\
        WORK_SET_ERR(UV_ECANCELED);\
        work_dbg("abort on entry");\
        WORK_EXIT;\
    }\
    work_dbg("begin");

#define WORK_END PT_END(__pt) }

static void work_del(work_t *work);

static void timer_close_cb(uv_handle_t *handle) {
    work_t *work = container_of(handle,work_t,timer);
    session_t *session = work->session;
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
    work_dbg("work del");
    work->cb(work,wa_destroy);
    uv_mutex_lock(&session->mem_mutex);
    if(work->sync) 
        SLIST_INSERT_HEAD(&session->free_sync_list,work->sync,node);
    TAILQ_INSERT_HEAD(&session->free_work_list,work,node);
    uv_mutex_unlock(&session->mem_mutex);
}

#define work_done(_work) \
    do{\
        work_log(DEBUG,_work,"work done");\
        work_done_(_work);\
    }while(0)

static inline void work_done_(work_t *work) {
    session_t *session = work->session;
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

static void timedout_cb(uv_timer_t *timer, int status) {
    work_t *work = (work_t*)timer->data;
    session_t *session = work->session;
    if(!status) {
        work_dbg("work timeout %d",work->timeout);
        WORK_SET_ERR(UV_ETIMEDOUT);
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
    if(!job && timeout>=0) {
        work->sync = SLIST_FIRST(&session->free_sync_list);
        if(work->sync == NULL){
            work->sync = (sync_t *)malloc(sizeof(sync_t));
            uv_mutex_init(&work->sync->mutex);
            uv_cond_init(&work->sync->cond);
        }else
            SLIST_REMOVE(&session->free_sync_list,work->sync,sync_s,node);
    }
    uv_mutex_unlock(&session->mem_mutex);

    work->session = session;
    work->data = data;
    work->cb = cb;
    work->cb_name = cb_name;
    work->state = ws_pending;
    if(job) {
        work->user_cb = job->cb;
        work->user_data = job->data;
        job->work = work;
        work_ref(work); /* unref by manually calling libusbip_work_close */
    }else if(work->sync)
        work_ref(work); /* unref by libusbip_work_close called in work_wait */
    work->timeout = timeout;
    work_ref(work); /* unref by work_done */
    uv_mutex_lock(&session->work_mutex);
    TAILQ_INSERT_TAIL(&session->work_queue,work,node);
    uv_mutex_unlock(&session->work_mutex);
    uv_async_send(&session->notify);
    return work;
} 

static int work_abort_cb(work_t *work, int action) {
    work_t *target = (work_t*)work->data;
    session_t *session = work->session;
    if(action == wa_process) {
        work_dbg("user abort cb");
        WORK_SET_ERR_(target,UV_ECANCELED);
        if(target->state == ws_busy)
            target->cb(target,wa_abort);
        work_done(work);
    }
    return 0;
}

int libusbip_work_abort(work_t *work) {
    session_t *session = work->session;
    work_dbg("user abort");
    work_new(work->session,work,work_abort_cb,-1,0);
    return 0;
}

static int work_unref_cb(work_t *work,int action) {
    session_t *session = work->session;
    if(action == wa_process) {
        work_unref((work_t*)work->data);
        work_done(work);
    }
    return 0;
}

void libusbip_work_close(work_t *work) {
    session_t *session = work->session;
    work_dbg("work close");
    /* since we are not in the loop thread, must unref the work asynchronously */
    work_new(work->session,work,work_unref_cb,-1,0);
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
    work_t *work,*wtmp;
    urb_t *urb,*tmp; 
    device_t *dev = container_of(handle,device_t,socket);
    session_t *session = dev->session;
    dev->state = devs_closed;
    TAILQ_FOREACH_SAFE(urb,&dev->urb_list,node,tmp) {
        TAILQ_REMOVE(&dev->urb_list,urb,node);
        WORK_SET_ERR_(urb->work,UV_ECANCELED);
        work_done(urb->work);
    }
    TAILQ_FOREACH_SAFE(work,&dev->read_list,node,wtmp){
        assert(work->reading);
        WORK_SET_ERR(UV_ECANCELED);
        work->status = -1;
        work->cb(work,wa_process);
    }
    device_unref(dev);
}

#define DEVICE_CLOSE(_dev) \
    do{\
        if(_dev->state>=devs_opening) {\
            _dev->state = devs_closing;\
            device_ref(_dev);\
            device_dbg("close socket");\
            uv_close((uv_handle_t*)&_dev->socket,socket_close_cb);\
        }\
    }while(0)

static int work_wait(work_t *work) {
    session_t *session = work->session;
    int ret;
    uv_mutex_lock(&work->sync->mutex);
    do{
        uv_cond_wait(&work->sync->cond,&work->sync->mutex);
    }while(!work_is_done(work));
    uv_mutex_unlock(&work->sync->mutex);
    ret = work->ret;
    work_dbg("work finish %d",ret);
    libusbip_work_close(work);
    return ret;
}

#define work_submit(_s,_d,_cb,_t,_j) \
    work_submit_(_s,_d,_cb,#_cb,_t,_j)
static inline int work_submit_(session_t *session, void *data, work_cb_t cb, 
        const char *cb_name, int timeout, job_t *job)
{
    int ret;
    work_t *work = work_new_(session,data,cb,cb_name,timeout,job);
    if(work == NULL) return -1;
    if(work->sync) return work_wait(work);
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
        work_trace("deque %d",work->state);
        if(work->state == ws_pending) {
            work->state = ws_busy;
            /* TODO: do we need to adjust the timeout in case
             * the work is queued for too long??
             */
            if(work->timeout>0) {
                uv_timer_init(session->loop,&work->timer);
                work->timer.data = work;
                work_dbg("start timer");
                uv_timer_start(&work->timer,timedout_cb,work->timeout,0);
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

int session_start_cb(work_t *work, int action) {
    session_t *session = work->session;
    if(action == wa_process) 
        session->start_time = uv_now(session->loop);
    return 0;
}

void libusbip_set_debug(session_t *session, int level) {
    session->log_level = level;
}

int libusbip_init(session_t **_session, int level) {
    char *env_level = getenv("LIBUSBIP_LOG_LEVEL");
    char *hosts = getenv("LIBUSBIP_HOSTS");
    int ret;
    session_t *session = (session_t*)calloc(1,sizeof(session_t));
    if(env_level)
        session->log_level = atoi(env_level);
    else
        session->log_level = level; 
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
    work_new(session,0,session_start_cb,-1,0);
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

static inline int device_find_urb(device_t *dev) {
    session_t *session = dev->session;
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
        work_dbg("abort device loop");
        DEVICE_CLOSE(dev);
        return 0;
    }
    if(action == wa_destroy) {
        DEVICE_CLOSE(dev);
        device_unref(dev);
        return 0;
    }
    if(action != wa_process)
        return -1;

    WORK_BEGIN(&dev->pt);

    dev->socket.data = work;
    WORK_ASYNC(uv_read_start,((uv_stream_t*)&dev->socket,alloc_cb,read_cb));
    while(1) {
        READ_PREPARE(dev,dev->recv.header);
        WORK_YIELD(dev);
        unpack_usbip_header_basic(&dev->recv.header.base);
        if(dev->recv.header.base.command == USBIP_RET_SUBMIT) {
            unpack_usbip_header_ret_submit(&dev->recv.header.u.ret_submit);
            if(!device_find_urb(dev)) {
                work_dbg("can't find urb %d",dev->recv.header.base.seqnum);
                if(dev->recv.header.u.ret_submit.status == 0 && 
                    dev->recv.header.base.direction == USBIP_DIR_IN) {
                    work_dbg("drop read %u",dev->recv.header.u.ret_submit.actual_length);
                    while(dev->recv.header.u.ret_submit.actual_length) {
                        if(dev->recv.header.u.ret_submit.actual_length<sizeof(dev->recv.buf))
                            READ_PREPARE_(dev,dev->recv.buf, 
                                    dev->recv.header.u.ret_submit.actual_length);
                        else
                            READ_PREPARE_(dev,dev->recv.buf, sizeof(dev->recv.buf));
                        WORK_YIELD(dev);
                        dev->recv.header.u.ret_submit.actual_length -= work->nread;
                    }
                }
                continue;
            }
            work_dbg("urb %d,%u",dev->recv.header.u.ret_submit.status,
                    dev->recv.header.u.ret_submit.actual_length);
            dev->recv.urb->header.u.ret_submit = dev->recv.header.u.ret_submit;
            TAILQ_REMOVE(&dev->urb_list,dev->recv.urb,node);
            if((work->status = dev->recv.header.u.ret_submit.status))
                WORK_SET_ERR(UV_EIO);
            else{
                if(dev->recv.urb->direction == USBIP_DIR_IN) {
                    READ_PREPARE_(dev,dev->recv.urb->buf.base,
                            dev->recv.header.u.ret_submit.actual_length);
                    WORK_YIELD(dev);
                }
                *dev->recv.urb->ret = dev->recv.header.u.ret_submit.actual_length;
            }
            work_done(dev->recv.urb->work);
            dev->recv.urb = NULL;

        /* no unlink support yet. Just drop connection on any error
        }else if(dev->recv.header.base.command == USBIP_RET_UNLINK) {
        */

        }else{
            err("invalid rx command %u",dev->recv.header.base.command);
            WORK_ABORT(UV_EINVAL);
        }
    }
    WORK_END;
}

#define CHECK_OP_REPLY(_op,_code) do{\
    unpack_op_common(_op);\
    if((_op)->version!=USBIP_VERSION_NUM) {\
        err("usbip version mismatch %x,%x",USBIP_VERSION_NUM,(_op)->version);\
        WORK_ABORT(UV_EINVAL);\
    }else if((_op)->code!=_code) {\
        err("usbip op code mismatch %d,%d",_code,(_op)->code);\
        WORK_ABORT(UV_EINVAL);\
    }\
}while(0)

static void device_timer_close_cb(uv_handle_t *handle) {
    device_t *dev = (device_t*)handle->data;
    session_t *session = dev->session;
    device_unref(dev);
}

static int device_open_cb(work_t *work, int action) {
    char addr[64];
    session_t *session = work->session;
    uv_buf_t buf[2];
    int ret = 0;
    device_t *dev = (device_t*)work->data;

    if(action == wa_abort) {
        work_dbg("abort device open");
        if(dev->state == devs_restarting){
            dev->state = devs_closed;
            uv_timer_stop(&dev->timer);
            work_done(work);
        }else
            DEVICE_CLOSE(dev);
        return 0;
    }else if(action == wa_destroy) {
        if(dev->timer.data){
            dev->timer.data = dev;
            uv_close((uv_handle_t*)&dev->timer,device_timer_close_cb);
        }else
            device_unref(dev);
        return 0;
    }else if(action != wa_process)
        return -1;

    WORK_BEGIN(&dev->pt);

    ret = uv_tcp_init(session->loop,&dev->socket);
    if(ret < 0){
        err("tcp init failed");
        WORK_SET_ERR(uv_last_error(session->loop).code);
        WORK_EXIT;
    }
    dev->socket.data = work;
    dev->state = devs_opening;
    dev->connect.req_connect.data = work;
    device_dbg("connecting to %s",(uv_ip4_name(&dev->addr,addr,sizeof(addr)),addr));
    WORK_ASYNC(uv_tcp_connect,(&dev->connect.req_connect,
            &dev->socket,dev->addr,connect_cb));
    WORK_YIELD(dev);

    dev->connect.req_write.data = work;
    dev->connect.op_header.version = USBIP_VERSION_NUM;
    dev->connect.op_header.code = OP_REQ_IMPORT;
    dev->connect.op_header.status = 0;
	pack_op_common(&dev->connect.op_header);
    buf[0] = uv_buf_init((char*)&dev->connect.op_header,
            sizeof(dev->connect.op_header));
    device_dbg("import device %s",dev->connect.req_import.busid);

    buf[1] = uv_buf_init((char*)&dev->connect.req_import,
            sizeof(dev->connect.req_import));

    dev->connect.req_write.data = work;
    WORK_ASYNC(uv_read_start,((uv_stream_t*)&dev->socket,alloc_cb,read_cb));

    WORK_ASYNC(uv_write,(&dev->connect.req_write, 
            (uv_stream_t*)&dev->socket, buf, 2, write_cb));
    WORK_YIELD(dev);

    READ_PREPARE(dev,dev->connect.op_header);
    WORK_YIELD(dev);
    CHECK_OP_REPLY(&dev->connect.op_header,OP_REP_IMPORT);
    if(dev->connect.op_header.status!=ST_OK) {
        err("usbip import error %d",dev->connect.op_header.status);
        if(++dev->connect.retry > dev->max_retry)
            WORK_ABORT(UV_EBUSY);

        assert(dev->state==devs_opening);
        dev->state = devs_closing;
        device_dbg("restart socket");
        uv_close((uv_handle_t*)&dev->socket,close_cb);
        WORK_YIELD(dev);
        dev->state = devs_restarting;
        if(dev->timer.data == 0) {
            uv_timer_init(session->loop,&dev->timer);
            dev->timer.data = work;
        }
        uv_timer_start(&dev->timer,timer_cb,500,0);
        WORK_RESTART;
    }

    READ_PREPARE(dev,dev->connect.rpl_import);
    WORK_YIELD(dev);

    unpack_op_import_reply(&dev->connect.rpl_import);
    if(strncmp(dev->connect.rpl_import.udev.busid, 
                dev->connect.req_import.busid, 
                sizeof(dev->connect.req_import.busid))) {
        err("busid mismatch %s,%s",dev->connect.req_import.busid,
                dev->connect.rpl_import.udev.busid);
        WORK_ABORT(UV_EINVAL);
    }

    /* pause the read first, and resume it in device_loop */
    dev->socket.data = 0;
    uv_read_stop((uv_stream_t*)&dev->socket);
    device_ref(dev);
    work_new(work->session,dev,device_loop,-1,0);

    work->ret = 0;
    *dev->pdev = dev;
    /* device returned to user, expect to unref in libusbip_device_close */
    device_ref(dev);
    WORK_EXIT;

    WORK_END;
}

int libusbip_device_open(session_t *session, device_t **pdev,
        const char *addr, int port, const char *busid,
        unsigned busnum, unsigned devnum,
        int retry, int timeout, libusbip_job_t *job)
{
    device_t *dev;

    uv_mutex_lock(&session->mem_mutex);
    if(SLIST_EMPTY(&session->free_device_list)) {
        int i;
        device_block_t *block = (device_block_t*)malloc(
                sizeof(device_block_t));
        SLIST_INSERT_HEAD(&session->device_blocks,block,node);
        for(i=0;i<array_sizeof(block->devices);++i)
            SLIST_INSERT_HEAD(&session->free_device_list,
                    &block->devices[i],node);
    }
    dev = SLIST_FIRST(&session->free_device_list);
    SLIST_REMOVE_HEAD(&session->free_device_list,node);
    uv_mutex_unlock(&session->mem_mutex);

    memset(dev,0,sizeof(device_t));
    TAILQ_INIT(&dev->read_list);
    TAILQ_INIT(&dev->urb_list);
    strncpy(dev->connect.req_import.busid,busid,
            sizeof(dev->connect.req_import.busid)-1);
    dev->session = session;
    dev->max_retry = retry;
    dev->devid = (busnum<<16|devnum);
    PT_INIT(&dev->pt);
    if(port == 0) port = USBIP_PORT;
    device_dbg("open %s,%d,%u-%u:%s",addr,port,busnum,devnum,
            dev->connect.req_import.busid);
    dev->addr = uv_ip4_addr(addr,port);
    dev->pdev = pdev;

    device_ref(dev);
    return work_submit(session,dev,device_open_cb,timeout,job);
}

static int device_close_cb(work_t *work, int action) {
    device_t *dev = (device_t*)work->data;
    session_t *session = work->session;
    if(action == wa_process) {
        DEVICE_CLOSE(dev);
        work_done(work);
        device_unref(dev);
    }
}

void libusbip_device_close(device_t *dev) {
    session_t *session = dev->session;
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
    ret->dev = dev;
    PT_INIT(&ret->pt);
    return ret;
}

static int device_urb_transfer(work_t *work, int action) {
    int ret = 0;
    uv_buf_t buf[2];
    int buf_count = 1;
    urb_t *urb = (urb_t*)work->data;
    device_t *dev = urb->dev;
    session_t *session = work->session;

    if(action == wa_abort) {
        work_dbg("abort urb transfer");
        DEVICE_CLOSE(dev);
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
    }else{
        err("invalid urb command %u",urb->header.base.command);
        WORK_ABORT(UV_EINVAL);
    }
    pack_usbip_header_basic(&urb->header.base);
    buf[0] = uv_buf_init((char*)&urb->header,sizeof(urb->header));
    if(urb->buf.len && urb->direction == USBIP_DIR_OUT)  {
        work_dbg("output %u",urb->buf.len);
        buf_count = 2;
        buf[1] = urb->buf;
    }else
        work_dbg("input %u",urb->buf.len);
    urb->req_write.data = work;
    WORK_ASYNC(uv_write,(&urb->req_write, (uv_stream_t*)&dev->socket, buf, buf_count, write_cb));
    WORK_YIELD(dev);

    /* work not done yet, put in the device urb list and wait for device_loop 
     * to call work_done on urb */
    work_dbg("urb wait");
    TAILQ_INSERT_TAIL(&dev->urb_list,urb,node);
    return PT_EXITED;

    WORK_END;
}

int libusbip_control_msg(device_t *dev, int requesttype, int request,
	int value, int index, char *bytes, int size, int *ret, int timeout, job_t *job)
{
    session_t *session = dev->session;
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

int libusbip_urb_transfer(device_t *dev, int ep, int ep_type,
	void *data, int size, int *ret, int timeout, job_t *job)
{
    session_t *session = dev->session;
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
        log(DEBUG,"adding static host %s %d",h->host.addr,h->host.port);
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

int libusbip_find_hosts(session_t *session, host_t **phosts, unsigned *count,
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
    *count = i;
    return 0;
}

void libusbip_free_hosts(session_t *session, host_t *hosts)
{
    free(hosts);
}

typedef struct device_info_req_s {
    session_t *session;
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

    TAILQ_HEAD(req_read_list_s,work_s) read_list;
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

static void device_info_req_close_cb(uv_handle_t *handle) {
    work_t *work,*wtmp;
    device_info_req_t *req = container_of(handle,device_info_req_t,socket);
    session_t *session = req->session;
    TAILQ_FOREACH_SAFE(work,&req->read_list,node,wtmp){
        assert(work->reading);
        WORK_SET_ERR(UV_ECANCELED);
        work->status = -1;
        work->cb(work,wa_process);
    }
    device_info_req_unref(req);
}

static inline void device_info_req_close(device_info_req_t *req) {
    session_t *session = req->session;
    if(!req->active) return;
    req->active = 0;
    device_info_req_ref(req);
    log(DEBUG,"close device info request");
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
        err("tcp init failed");
        WORK_SET_ERR(work->ret);
        WORK_EXIT;
    }
    req->active = 1;
    req->socket.data = work;
    req->connect.data = work;
    work_dbg("connecting to %s",(uv_ip4_name(&req->addr,addr,sizeof(addr)),addr));
    WORK_ASYNC(uv_tcp_connect,(&req->connect,&req->socket,req->addr,connect_cb));
    WORK_YIELD(req);

    req->write.data = work;
    req->header.version = USBIP_VERSION_NUM;
    req->header.code = OP_REQ_DEVLIST;
    req->header.status = 0;
    pack_op_common(&req->header);
    buf = uv_buf_init((char*)&req->header, sizeof(req->header));

    req->write.data = work;
    WORK_ASYNC(uv_write,(&req->write, (uv_stream_t*)&req->socket, &buf, 1, write_cb));
    WORK_YIELD(req);

    READ_PREPARE(req,req->header);
    ret =  uv_read_start((uv_stream_t*)&req->socket,alloc_cb,read_cb);
    WORK_YIELD(req);
    CHECK_OP_REPLY(&req->header,OP_REP_DEVLIST);
    if(req->header.status != ST_OK) {
        err("usbip op error %d",req->header.status);
        WORK_ABORT(UV_EINVAL);
    }

    READ_PREPARE(req,req->reply);
    WORK_YIELD(req);
    unpack_op_devlist_reply(&req->reply);

    work_dbg("list device %u",req->reply.ndev);

    *req->pinfo = req->info = (device_info_t *)
        calloc(req->reply.ndev,sizeof(device_info_t));
    for(;req->count<req->reply.ndev;++req->count,*req->pcount=req->count) {
        device_info_t *info;
        READ_PREPARE(req,req->udev);
        WORK_YIELD(req);
        unpack_usbip_usb_device(&req->udev);

        work_dbg("device %u-%u:%s",req->udev.busnum,req->udev.devnum,req->udev.busid);

        info = &req->info[req->count];
        strncpy(info->busid,req->udev.busid,sizeof(info->busid)-1);
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
            READ_PREPARE(req,req->intf);
            WORK_YIELD(req);
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

    WORK_END;
}

int libusbip_get_devices_info(session_t *session, host_t *host, 
        device_info_t **info, unsigned *count, 
        int timeout, libusbip_job_t *job)
{
    int port = host->port;
    device_info_req_t *req;
    if(info == 0 || count == 0)
        return -1;
    req = (device_info_req_t*)calloc(1,sizeof(device_info_req_t));
    req->session = session;
    TAILQ_INIT(&req->read_list);
    if(port == 0) port = USBIP_PORT;
    req->addr = uv_ip4_addr(host->addr,port);
    log(DEBUG,"host %s,%d",host->addr,port);
    PT_INIT(&req->pt);
    req->pinfo = info;
    req->pcount = count;
    device_info_req_ref(req);
    return work_submit(session,req,device_info_req_cb,timeout,job);
}

