#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <asm/byteorder.h>

#include <linux/types.h>
#include <linux/usb/gadgetfs.h>
#include <linux/usb/ch9.h>

#include "../libusbip-client.h"
#include "../deps/bsd/queue.h"

#undef err
#undef log
#define err(msg,...) fprintf(stderr,"err(%d): " msg "\n",__LINE__,##__VA_ARGS__)
#define log(msg,...) fprintf(stderr,"log(%d): " msg "\n",__LINE__,##__VA_ARGS__)

typedef struct context_s context_t;
typedef struct ep_s ep_t;

#define BUF_SIZE (7 * 1024)

typedef struct urb_s {
    TAILQ_ENTRY(urb_s) node;
    ep_t *ep;
    int stopping;
    char buf[BUF_SIZE];
    int length;
    int actual_length;
    libusbip_work_t *work;
}urb_t;

typedef TAILQ_HEAD(urb_list_s, urb_s) urb_list_t;

struct ep_s{
    int init;
    int fd;
    pthread_t thread;
    struct usb_interface_descriptor *interface;
    struct usb_endpoint_descriptor *desc;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    urb_list_t urb_ready_list;
    int stopping;
};

struct context_s {
    int verbose;
    int speed;
    libusbip_session_t *session;
    pthread_t thread;
    libusbip_device_info_t info;
    libusbip_host_t host;
    libusbip_device_t *dev;
    char *desc;
    struct usb_config_descriptor *config;
    int fd;

    ep_t eps[32];

    pthread_mutex_t mutex;
    urb_list_t urb_free_list;

    int stopping;
};

static inline const char *ep_tag(char *tag, struct usb_endpoint_descriptor *desc) {
    static const char *type[] = {"ctrl","iso","bulk","intr"};
    sprintf(tag,"ep%d%s-%s",usb_endpoint_num(desc),usb_endpoint_dir_in(desc)?"in":"out",
            type[usb_endpoint_type(desc)]);
    return tag;
}

static inline int ep_index(struct usb_endpoint_descriptor *desc) {
    return usb_endpoint_num(desc) + usb_endpoint_dir_in(desc)*16;
}

static inline context_t *ep_to_ctx(ep_t *ep) {
    context_t *tmp = 0;
    return (context_t*)(((char*)ep)-((char*)&tmp->eps[ep_index(ep->desc)]));
}

static inline void urb_free(context_t *ctx, urb_t *urb) {
    pthread_mutex_lock(&ctx->mutex);
    TAILQ_INSERT_HEAD(&ctx->urb_free_list,urb,node);
    pthread_mutex_unlock(&ctx->mutex);
}

static void stop_io(context_t *ctx) {
    int i;
    pthread_mutex_lock(&ctx->mutex);
    if(ctx->stopping) {
        pthread_mutex_unlock(&ctx->mutex);
        return;
    }
    ctx->stopping = 1;
    pthread_mutex_unlock(&ctx->mutex);

    for(i=0;i<sizeof(ctx->eps)/sizeof(ctx->eps[0]);++i) {
        ep_t *ep = &ctx->eps[i];
        if(ep->fd <= 0) continue;

        pthread_mutex_lock(&ep->mutex);
        ep->stopping = 1;
        pthread_mutex_unlock(&ep->mutex);
        close(ep->fd);
        ep->fd = -1;
        pthread_join(ep->thread,0);

        while(!TAILQ_EMPTY(&ep->urb_ready_list)) {
            urb_t *urb = TAILQ_FIRST(&ep->urb_ready_list);
            TAILQ_REMOVE(&ep->urb_ready_list,urb,node);
            urb_free(ctx,urb);
        }
    }

    libusbip_device_close(ctx->dev);
    ctx->dev = 0;
    
    pthread_mutex_lock(&ctx->mutex);
    ctx->stopping = 0;
    pthread_mutex_unlock(&ctx->mutex);
}

static inline int device_open(context_t *ctx) {
    return libusbip_device_open(ctx->session,&ctx->dev,
            ctx->host.addr,ctx->host.port,ctx->info.busid,
            ctx->info.busnum,ctx->info.devnum,3,5000,0);
}

static urb_t *urb_new(context_t *ctx, ep_t *ep) {
    urb_t *urb;
    pthread_mutex_lock(&ctx->mutex);
    if(TAILQ_EMPTY(&ctx->urb_free_list)) {
        urb = (urb_t*)calloc(1,sizeof(urb_t));
    }else{
        urb = TAILQ_FIRST(&ctx->urb_free_list);
        TAILQ_REMOVE(&ctx->urb_free_list,urb,node);
    }
    pthread_mutex_unlock(&ctx->mutex);
    urb->ep = ep;
    urb->actual_length = 0;
    urb->length = 0;
    urb->work = 0;
    return urb;
}

static void urb_done(void *_ctx, int ret) {
    urb_t *urb = (urb_t*)_ctx;
    ep_t *ep = urb->ep;
    context_t *ctx = ep_to_ctx(ep);
    if(ret) {
        err("urb failed %d",ret);
        stop_io(ctx);
    }
    libusbip_work_close(urb->work);

    if(!usb_endpoint_dir_in(ep->desc) && urb->length!=urb->actual_length) {
        err("urb write incomplete %d, %d",urb->length,urb->actual_length);
        stop_io(ctx);
    }else{
        pthread_mutex_lock(&ep->mutex);
        if(!ep->stopping) { 
            TAILQ_INSERT_TAIL(&ep->urb_ready_list,urb,node);
            pthread_cond_signal(&ep->cond);
            urb = 0;
        }
        pthread_mutex_unlock(&ep->mutex);
    }
    if(urb) urb_free(ctx,urb);
}

static void usbip_read(context_t *ctx, ep_t *ep, urb_t *urb) {
    libusbip_job_t job;
    if(ep->stopping) return;
    if(!urb) urb = urb_new(ctx,ep);
    job.data = urb;
    job.cb = urb_done;
    job.work = 0;
    if(libusbip_urb_transfer(ctx->dev,ep->desc->bEndpointAddress, 
                usb_endpoint_type(ep->desc),urb->buf,sizeof(urb->buf),
                &urb->actual_length,0,&job)) {
        err("urb transfer failed");
        stop_io(ctx);
    }else
        urb->work = job.work;
}

static inline void usbip_write(context_t *ctx, urb_t *urb) {
    libusbip_job_t job;
    ep_t *ep = urb->ep;
    if(ep->stopping) return;
    job.data = urb;
    job.cb = urb_done;
    job.work = 0;
    if(libusbip_urb_transfer(ctx->dev,ep->desc->bEndpointAddress, 
                usb_endpoint_type(ep->desc),urb->buf,urb->length,
                &urb->actual_length,0,&job)) {
        err("urb transfer failed");
        stop_io(ctx);
    }else
        urb->work = job.work;
}

static void *ep_in_thread (void *param)
{
    urb_t *free_urb = 0;
    urb_t *urb;
    ep_t *ep = (ep_t*)param;
    char tag[32];
    context_t *ctx = ep_to_ctx(ep);
    int ret;
    char *buf,*end;

    ep_tag(tag,ep->desc);

    pthread_mutex_lock(&ep->mutex);
    usbip_read(ctx,ep,0);

    while(!ep->stopping) {
        if(TAILQ_EMPTY(&ep->urb_ready_list)) {
            pthread_cond_wait(&ep->cond,&ep->mutex);
            continue;
        }
        urb = TAILQ_FIRST(&ep->urb_ready_list);
        TAILQ_REMOVE(&ep->urb_ready_list,urb,node);

        log("%s usbip read %d",urb->actual_length);

        usbip_read(ctx,ep,free_urb);
        free_urb = 0;

        pthread_mutex_unlock(&ep->mutex);

        buf = urb->buf;
        end = buf + urb->actual_length;
        while(buf!=end) {
            ret = write(ep->fd,buf,end-buf);
            if(ret < 0) {
                err("%s write railed %d",tag,errno);
                stop_io(ctx);
                urb_free(ctx,urb);
                return 0;
            }else
                log("%s written %d",tag,ret);
            buf += ret;
        }
        free_urb = urb;
        pthread_mutex_lock(&ep->mutex);
    }
    pthread_mutex_unlock(&ep->mutex);

    if(free_urb) urb_free(ctx,free_urb);

    return 0;
}

static void *ep_out_thread (void *param) {
    urb_t *free_urb = 0;
    urb_t *urb;
    ep_t *ep = (ep_t*)param;
    context_t *ctx = ep_to_ctx(ep);
    char tag[32];

    ep_tag(tag,ep->desc);

    urb = urb_new(ctx,ep);
    TAILQ_INSERT_HEAD(&ep->urb_ready_list,urb,node);
    /* insert more if need multiple buffering, but it 
     * probably have adverse effect */

    pthread_mutex_lock(&ep->mutex);
    while(!ep->stopping) {
        if(TAILQ_EMPTY(&ep->urb_ready_list)){
            pthread_cond_wait(&ep->cond,&ep->mutex);
            continue;
        }
        urb = TAILQ_FIRST(&ep->urb_ready_list);
        TAILQ_REMOVE(&ep->urb_ready_list,urb,node);
        pthread_mutex_unlock(&ep->mutex);

        urb->length = read(ep->fd,urb->buf,sizeof(urb->buf));
        if(urb->length < 0) {
            err("%s read failed, %d",tag,errno);
            stop_io(ctx);
            urb_free(ctx,urb);
            return 0;
        }else
            log("%s read %d",tag,urb->length);
        pthread_mutex_lock(&ep->mutex);
        usbip_write(ctx,urb);
    }
    pthread_mutex_unlock(&ep->mutex);
    return 0;
}

static int start_io(context_t *ctx) {
    char name[64];
    int i;
    if(!ctx->dev) {
        if(!device_open(ctx)) {
            err("device open failed");
            return 1;
        }
    }
    ctx->stopping = 0;

    for(i=0;i<sizeof(ctx->eps)/sizeof(ctx->eps[0]);++i) {
        char buf[256*2+4];
        char *config = buf;
        ep_t *ep = &ctx->eps[i];

        if(!ep->desc) continue;
        snprintf(name,sizeof(name),"ep%d%s", usb_endpoint_num(ep->desc),
                usb_endpoint_dir_in(ep->desc)?"in":"out");
        ep->fd = open(name,O_RDWR);
        if(ep->fd<0) {
            err("failed to open %s, %d",name,errno);
            stop_io(ctx);
            return 1;
        }
        log("starting %s",name);

        *(__u32 *)config = 1;  /* tag for this format */
        config += 4;
        memcpy(config, ep->desc, ep->desc->bLength);
        config += ep->desc->bLength;
        if(ctx->speed == USB_SPEED_HIGH){
            /* for high speed device, we can't get the full speed
             * descriptor directly. We should have manually downgrade
             * the endpoint spec. We simple duplicate the descriptor
             * for now. */
            memcpy(config, ep->desc, ep->desc->bLength);
            config += ep->desc->bLength;
        }

        if(write(ep->fd,buf,config-buf)<0) {
            err("%s config failed %d",name,errno);
            stop_io(ctx);
            return 1;
        }

        ep->stopping = 0;
        pthread_create(&ep->thread,0,usb_endpoint_dir_in(ep->desc)?
                ep_in_thread:ep_out_thread,ep);
    }
    return 0;
}

static int get_descriptor(context_t *ctx, unsigned char type,
	unsigned char index, void *buf, int size)
{
    int ret = -1;
    memset(buf, 0, size);
    libusbip_control_msg(ctx->dev, USB_DIR_IN, USB_REQ_GET_DESCRIPTOR,
            (type << 8) + index, 0, buf, size, &ret, 1000, 0);
    return ret;
}

/* config the endpoints using the given interface number and
 * altsetting. If altsetting<0, iterate through all interfaces
 * and use the first altsetting */
static int ep_config(context_t *ctx, int interface, int altsetting)
{
    union {
        struct usb_interface_descriptor *intf;
        struct usb_endpoint_descriptor *ep;
    } descriptor;
    int i;
    int ret = -1;
    char *desc = (char *)ctx->config;
    int length = __le16_to_cpu(ctx->config->wTotalLength);
    struct usb_interface_descriptor *current;
    int state;
    enum {
        STATE_SEEK, /* seek for the next different interface descriptor */
        STATE_CONFIG, /* config the endpoints with the current interface altsetting */
    };
    state = STATE_SEEK;
    for(i=sizeof(*ctx->config);i<length;i+=desc[i]) {
        descriptor.ep = (struct usb_endpoint_descriptor*)&desc[i];
        switch(state) {
        case STATE_CONFIG: {
            if(descriptor.ep->bDescriptorType == USB_DT_ENDPOINT) {
                ep_t *ep = &ctx->eps[ep_index(descriptor.ep)];
                ep->interface = current;
                ep->desc = descriptor.ep;
                char tag[32];
                log("config %s on interface %d-%d",ep_tag(tag,descriptor.ep),
                        current->bInterfaceNumber, current->bAlternateSetting);
                if(!ep->init) {
                    log("ep init");
                    pthread_mutex_init(&ep->mutex,0);
                    pthread_cond_init(&ep->cond,0);
                    TAILQ_INIT(&ep->urb_ready_list);
                    ep->init = 1;
                }else
                    log("ep already init");
                break;
            }else if(descriptor.ep->bDescriptorType != USB_DT_INTERFACE) {
                log("skip descriptor %02x",descriptor.ep->bDescriptorType);
                break;
            }
            if(altsetting>=0) {
                log("config end with another interface %d-%d",
                        descriptor.intf->bInterfaceNumber, descriptor.intf->bAlternateSetting);
                return 0;
            }
            /* if altsetting<0, fall through, and seek for the next interface */
        } case STATE_SEEK: {
            if(descriptor.intf->bDescriptorType == USB_DT_INTERFACE && 
               descriptor.intf->bInterfaceNumber == interface)
            {
                if(altsetting<0 || descriptor.intf->bAlternateSetting==altsetting) {
                    ret = 0;
                    current = descriptor.intf;
                    state = STATE_CONFIG;
                    log("config interface %d-%d",
                        descriptor.intf->bInterfaceNumber, descriptor.intf->bAlternateSetting);
                    continue;
                }
            }
            log("skip interface %d-%d",
                    descriptor.intf->bInterfaceNumber, descriptor.intf->bAlternateSetting);
            break;
        }}
    }
    return ret;
}

static int init(context_t *ctx, const char *name, 
        libusbip_host_t *host, libusbip_device_info_t *info,
        int idvendor, int idproduct)
{
    struct usb_config_descriptor *config;
    struct usb_interface_descriptor *interface;
    struct usb_endpoint_descriptor *ep;
    struct usb_device_descriptor *ddesc;
    unsigned length;
    char tmp[8];
    char    *desc;
    int     fd;
    libusbip_host_t *hosts = NULL;
    libusbip_device_info_t *infos = NULL;
    int host_count,device_count,i,j;
    int ret;
    int state;

    pthread_mutex_init(&ctx->mutex,0);
    TAILQ_INIT(&ctx->urb_free_list);

    if(libusbip_init(&ctx->session,LIBUSBIP_LOG_ERROR)<0)
        return -1;

    if(!host) {
        if(libusbip_find_hosts(ctx->session,&hosts,&host_count,5000,0)<0)
            return -1;
    }else{
        hosts = host;
        host_count = 1;
    }
    for(i=0;i<host_count;++i){
        if(libusbip_get_devices_info(ctx->session,&hosts[i],&infos,&device_count,5000,0)<0)
            break;

        for(j=0;j<device_count;++j) {
            if(info){
               if(infos[j].busnum == info->busnum &&
                    infos[j].devnum == info->devnum &&
                    strcmp(infos[j].busid,info->busid)==0)
               {
                   log("found device");
                    ctx->info = infos[j];
                    ctx->host = hosts[i];
                    goto NEXT;
               }
            }else if(infos[j].idVendor == idvendor &&
                infos[j].idProduct == idproduct) {

                log("found device");
                ctx->info = infos[j];
                ctx->host = hosts[i];
                goto NEXT;
            }
        }

        libusbip_free_devices_info(ctx->session,infos,device_count);
        infos = NULL;
    }
NEXT:
    if(hosts!=host && hosts) {
        libusbip_free_hosts(ctx->session,hosts);
        hosts = NULL;
    }
    if(infos) {
        libusbip_free_devices_info(ctx->session,infos,device_count);
        infos = NULL;
    }else{
        err("no device found");
        return 1;
    }

    log("open device");
    device_open(ctx);
    if(!ctx->dev) return -1;

    /* gadgetfs only support single configuration, so we choose the first one as default */

    log("get descriptor first");

    /* get first 8 bytes to deteremine the total length */
    ret = get_descriptor(ctx, USB_DT_CONFIG, 0, tmp, 8);
    if (ret < 8) {
        err("failed to get init descriptor");
        return -1;
    }

    config = (struct usb_config_descriptor*)tmp;
    length = __le16_to_cpu(config->wTotalLength)+4+sizeof(struct usb_device_descriptor);
    if(ctx->info.speed == USB_SPEED_HIGH) {
        log("construct high speed descriptor");
        length += sizeof(struct usb_config_descriptor);
    }
    if(ctx->desc) free(ctx->desc);
    desc = ctx->desc = (char *)malloc(length);
    if(!desc) {
        err("failed to alloc descriptor buffer");
        return -1;
    }

    /* gadgetfs demand a tag = 0 for device config */
    *(__u32 *)desc = 0;
    desc += 4;

    if(ctx->info.speed == USB_SPEED_HIGH) {
        /* we can't get the full speed descriptor easily if the remote 
         * device is connected as high speed, so just fake a config descriptor
         * with no endpoints */
        struct usb_config_descriptor fs_config = {
                .bLength =      sizeof fs_config,
                .bDescriptorType =  USB_DT_CONFIG,
                .bNumInterfaces =   0,
                .bConfigurationValue =  3,
                .iConfiguration =   0,
                .bmAttributes =     USB_CONFIG_ATT_ONE
                    | USB_CONFIG_ATT_SELFPOWER,
                .bMaxPower =        1,
            };
        fs_config.wTotalLength = __constant_cpu_to_le16(sizeof fs_config);
        memcpy(desc,&fs_config,sizeof(fs_config));
        desc += sizeof(fs_config);
    }

    length = __le16_to_cpu(config->wTotalLength);
    log("get config descriptor %d",length);

    /* gadgetfs does not support extended descriptors. 
     * we just leave them there for now, because we probably can't 
     * tap into those devices any way
     */
    ret = get_descriptor(ctx, USB_DT_CONFIG, 0, desc, length);
    if (ret != length) {
        err("failed to get config descriptor %d",length);
        return -1;
    }
    ctx->config = (struct usb_config_descriptor*)desc;

    log("ep config");

    /* go through all interfaces and config the endpoints using the first altsetting */
    ep_config(ctx,0,-1);

    desc += length;

    log("get device config");

    /* now get the device descriptor. */
    ddesc = (struct usb_device_descriptor*)desc;
    ret = get_descriptor(ctx, USB_DT_DEVICE, 0, desc, sizeof(*ddesc));
    if(ret < sizeof(*ddesc)) {
        err("failed to get device descriptor");
        return -1;
    }
    /* again, gadgetfs only support one configuration */
    if(ddesc->bNumConfigurations>1) {
        log("Warning! Device has multiple configurations");
        ddesc->bNumConfigurations = 1;
    }

    desc += sizeof(*ddesc);

    ctx->fd = open(name,O_RDWR);
    if(fd < 0) {
        err("failed to open gadget device %s",name);
        return -1;
    }
    log("write descriptor");
    length = desc-ctx->desc;
    ret = write (ctx->fd, ctx->desc, length);
    if (ret!=length) {
        err("failed to write descriptors %d, %d", length, errno);
        return -1;
    }

    log("init done");

    return 0;
}

static void handle_control (context_t *ctx, struct usb_ctrlrequest *setup)
{
    int start = 0;
    int ret = 0;
    char    *buf;
    __u16       value, index, length;
    int to_host = setup->bRequestType & USB_DIR_IN;
    int fd = ctx->fd;

    value = __le16_to_cpu(setup->wValue);
    index = __le16_to_cpu(setup->wIndex);
    length = __le16_to_cpu(setup->wLength);

    log("SETUP %02x.%02x "
            "v%04x i%04x %d",
        setup->bRequestType, setup->bRequest,
        value, index, length);

    buf = (char*)malloc(length);
    if(length && !to_host) {
        log("read from host");
        ret = read(fd,buf,length);
        if(ret!=length){
            err("control read failed %d,%d",ret,errno);
            goto STALL;
        }
        log("read %d from host",ret);
    }
    switch (setup->bRequest) {  /* usb 2.0 spec ch9 requests */
    case USB_REQ_SET_CONFIGURATION:
        if (setup->bRequestType != USB_DIR_OUT)
            goto STALL;
        log("CONFIG #%d", value);

        if(value == ctx->config->bConfigurationValue) {
            start = 1;
        }else if(value == 0)
            stop_io (ctx);
        else{
            /* kernel bug -- "can't happen" */
            err("? illegal config");
            goto STALL;
        }
        break;
    case USB_REQ_SET_INTERFACE:
        if (setup->bRequestType != USB_RECIP_INTERFACE)
            goto STALL;

        stop_io(ctx);
        if(ep_config(ctx,index,value))
            goto STALL;
        start = 1;
        break;
    }

    libusbip_control_msg(ctx->dev, setup->bRequestType, setup->bRequest,
            value, index, buf, length, &ret, 1000, 0);
    if(ret<0) {
        err("control message failed %d",ret);
        goto STALL;
    }else{
        log("control return %d",ret);
    }
    if(to_host) {
        length = ret;
        log("control write to host %d",length);
        ret = write(fd,buf,length);
        if(ret!=length) {
            err("control write failed %d,%d",ret,errno);
            goto STALL;
        }
        log("control wrote %d",ret);
    }else if(!length) {
        log("control ack read null");
        ret = write(fd,&ret,0);
        if(ret<0) {
            err("control read null failed %d",errno);
            goto STALL;
        }else
            log("control ack read null done");
    }
    if(start && start_io(ctx)) {
        log("start io");
        goto STALL;
    }
    log("control done");
    return;

STALL:
    err("... protocol stall %02x.%02x",
            setup->bRequestType, setup->bRequest);
    if (!to_host)
        ret = read (fd, &ret, 0);
    else
        ret = write (fd, &ret, 0);
    if (ret != -1)
        err("can't stall ep0 for %02x.%02x",
            setup->bRequestType, setup->bRequest);
    else if (errno != EL2HLT)
        perror ("ep0 stall");
    return;
}

static const char *speed (enum usb_device_speed s)
{
    switch (s) {
    case USB_SPEED_LOW: return "low speed";
    case USB_SPEED_FULL:    return "full speed";
    case USB_SPEED_HIGH:    return "high speed";
    default:        return "UNKNOWN speed";
    }
}

/*-------------------------------------------------------------------------*/

/* control thread, handles main event loop  */

#define NEVENT      5
#define LOGDELAY    (15 * 60)   /* seconds before stdout timestamp */

static void *ep0_thread (void *param)
{
    context_t *ctx = (context_t *)param;
    time_t          now, last;
    struct pollfd       ep0_poll;
    int fd = ctx->fd;

    ep0_poll.fd = fd;
    ep0_poll.events = POLLIN | POLLOUT | POLLHUP;

    /* event loop */
    last = 0;
    for (;;) {
        int             tmp;
        struct usb_gadgetfs_event   event [NEVENT];
        int             connected = 0;
        int             i, nevent;

        /* Use poll() to test that mechanism, to generate
         * activity timestamps, and to make it easier to
         * tweak this code to work without pthreads.  When
         * AIO is needed without pthreads, ep0 can be driven
         * instead using SIGIO.
         */
        tmp = poll(&ep0_poll, 1, -1);
        if (tmp < 0) {
            /* exit path includes EINTR exits */
            err("poll error %d",errno);
            break;
        }

        tmp = read (fd, &event, sizeof event);
        if (tmp < 0) {
            if (errno == EAGAIN) {
                sleep (1);
                continue;
            }
            err("ep0 read after poll, %d",errno);
            goto done;
        }
        nevent = tmp / sizeof event [0];
        if (nevent != 1)
            log("read %d ep0 events", nevent);

        for (i = 0; i < nevent; i++) {
            switch (event [i].type) {
            case GADGETFS_NOP:
                log("NOP");
                break;
            case GADGETFS_CONNECT:
                connected = 1;
                ctx->speed = event [i].u.speed;
                log("CONNECT %s", speed (event [i].u.speed));
                break;
            case GADGETFS_SETUP:
                connected = 1;
                handle_control (ctx, &event [i].u.setup);
                break;
            case GADGETFS_DISCONNECT:
                connected = 0;
                ctx->speed = USB_SPEED_UNKNOWN;
                log("DISCONNECT");
                stop_io(ctx);
                break;
            case GADGETFS_SUSPEND:
                // connected = 1;
                log( "SUSPEND");
                break;
            default:
                err("* unhandled event %d",
                    event [i].type);
            }
        }
        continue;
done:
        if (connected)
            stop_io (ctx);
        break;
    }
    log("done");

    return 0;
}

/*-------------------------------------------------------------------------*/

int
main (int argc, char **argv)
{
    int c;
    int idvendor=0,idproduct=0;
    context_t ctx;
    libusbip_host_t host,*phost=0;
    libusbip_device_info_t info,*pinfo=0;
    const char *devname = "dwc_otg_pcd";

    memset(&ctx,0,sizeof(ctx));
    memset(&host,0,sizeof(host));
    memset(&info,0,sizeof(info));

    while ((c = getopt (argc, argv, "i:p:b:n:V:v")) != EOF) {
        switch (c) {
        case 'n':
            devname = optarg;
            continue;
        case 'V':
            if(sscanf(optarg,"%x:%x",&idvendor,&idproduct)==2)
                continue;
            break;
        case 'i':
            strncpy(host.addr,optarg,sizeof(host.addr)-1);
            phost = &host;
            continue;
        case 'p':
            host.port = atoi(optarg);
            continue;
        case 'b':
            if(sscanf(optarg,"%d-%d:%s",&info.busnum,&info.devnum,info.busid)!=3)
                break;
            pinfo = &info;
            continue;
        case 'v':       /* verbose */
            ctx.verbose++;
            continue;
        case 'h':
            break;
        }
        fprintf (stderr, "usage:  %s "
                "[-i IP] [-p port] [-b busnum-devnum:busid] "
                "[-V hex_vendor_id:hex_product_id] [-n device name] [-v]\n",
                argv [0]);
        return 1;
    }
    if (chdir ("/dev/gadget") < 0) {
        err("can't chdir /dev/gadget");
        return 1;
    }

    if(init(&ctx,devname,phost,pinfo,idvendor,idproduct))
        return 1;
    (void) ep0_thread (&ctx);
    return 0;
}

