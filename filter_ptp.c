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

/* define this to support unknown usb devices by parsing ptp GetDeviceInfo request */
/* #define PTP_CONFIG_PARSE_DEVICE_INFO */

#include <asm/byteorder.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/moduleparam.h>
#include <linux/string.h>

#include "usbip_common.h"
#include "stub.h"
#include "gphoto2/camlibs/ptp2/ptp.h"
#include "gphoto2/camlibs/ptp2/ptp-bugs.h"

#define DRIVER_VERSION "1.0.0 (usdip " USBIP_VERSION ")"
#define DRIVER_AUTHOR "Zheng, Lei <realthunder.dev@gmail.com>"
#define DRIVER_DESC "USB/IP Filter for PTP"

/* #define TRACE_STREAM */

/* Desired frame rate for preview */
unsigned int usbip_ptp_fps = 10;
EXPORT_SYMBOL_GPL(usbip_ptp_fps);
module_param(usbip_ptp_fps, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(usbip_ptp_fps, "ptp preview streaming frames per second");

/* Maximum number of read ahead frames */
unsigned int usbip_ptp_buffer_count = 3;
EXPORT_SYMBOL_GPL(usbip_ptp_buffer_count);
module_param(usbip_ptp_buffer_count, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(usbip_ptp_buffer_count, "ptp preview maximum buffered frame count");

unsigned int frame_jiffies;

enum ptp_filter_state {

    /* seeking ptp vendor */
    ptpfs_init = 0,

    /* unsupported ptp device */
    ptpfs_bypassed = 1,

    /* vendor found, no interception */
    ptpfs_idle = 2,

    /* other command is sent, waiting for response */
    ptpfs_command = 3,

    /* filtering active, monitoring preview command */
    ptpfs_active = 4,

    /* self generated preview command sent, waiting for reponse */
    ptpfs_busy = 5,

    /* other client command is sent, waiting for response */
    ptpfs_wait = 6,

    /* indicate the current receiving frame should be dropped */
    ptpfs_drop = 7,

    /* sleep cause by buffer full, wait for client read request to clear
     * up some space */
    ptpfs_sleep = 8,

    /* other client command is sent while we are sleeping */
    ptpfs_sleep_wait = 9,
};

enum ptp_parser_state {
    /* response phase just finished, ready to accept new command */
    ptpps_none = 0,

    /* in the middle of data phase */
    ptpps_data = 1,

    /* in the middle of response phase */
    ptpps_response = 2,

    /* in the middle of command phase */
    ptpps_command = 3,

    /* in the middle of an unknown phase */
    ptpps_unkonwn = 4,

    /* not in the middle of any phase, and waiting for the response phase */
    ptpps_wait_response = 6,
};

struct ptp_state_machine {
    const char *type;
    int state;
    u32 size;
    PTPUSBBulkHeader ptpdu;
};

struct ptp_filter {
    struct stub_device *sdev;
    struct timer_list timer;
    int state;

    PTPDeviceInfo info;

    struct {
        struct usbip_header pdu;
        PTPUSBBulkContainer *ptpdu;
        unsigned long jiffies;
        unsigned long sequm;
    }trigger;

    int frame_count;
    int frame_serving;

    int in_pipe;
    int out_pipe;
    u32 out_pipe_size;
#define PTP_TRANSFER_LENGTH(_length) \
            (_length/filter->out_pipe_size*filter->out_pipe_size)
    u8 *frame_buffer;
    /* head pointer of the frame buffer, always point to the begining of the next future frame */
    int frame_head;
    /* tail pointer of the frame buffer */
    int frame_tail;
    /* remaining frame ptpdu length of the current serving frame */
    u32 frame_length;
    /* whether the current serving pdu ends the frame request */
    int frame_pdu_end;
    /* remaining frame ptpdu length of the current receiving frame */
    u32 frame_rx_length;
    /* whether the current reciving pdu ends the frame request */
    int frame_rx_pdu_end;
    /* the effective buffer size, due to bulk transfer size bounduary, 
     * we may wrap the buffer early */
    int frame_buffer_size;

    /* this tail marks the start of the receiving frame. 
     * This mark is to detect whether any consumer is consuming the current
     * receiving frame. If none, then when buffer is full, we shall drop the 
     * the current receiving frame. If else, we just pause and wait for the 
     * consumer to clearing for more space
     */
    int frame_rx_tail;
    /* this tail marks the start of the last complete frame. */
    int frame_rx_tail2;

#define ptp_framebuf_align_shift 6
#define ptp_framebuf_align_mask ((1<<ptp_framebuf_align_shift)-1)
#define ptp_framebuf_align(_pt) \
    do{\
        if(_pt&ptp_framebuf_align_mask) \
            _pt=((_pt>>ptp_framebuf_align_shift)+1)<<ptp_framebuf_align_shift;\
    }while(0)
#define ptp_framebuf_full_size (1024*1024)
#define ptp_framebuf_size filter->frame_buffer_size
#define ptp_framebuf_to_end_count \
    (head>=tail?head-tail:ptp_framebuf_size-tail)
#define ptp_framebuf_count \
    (head>=tail?head-tail:ptp_framebuf_size-tail+head)
#define ptp_framebuf_to_end_space \
    (head>=tail?ptp_framebuf_size-head:tail-head)
#define ptp_framebuf_space \
    (head>=tail?ptp_framebuf_size-head+tail:tail-head)
#define ptp_framebuf_nospace \
    (ptp_framebuf_space<=filter->out_pipe_size)
#define ptp_framebuf_producer_fini \
    do {\
        if(ptp_frame_to_end_space < filter->out_pipe_size) {\
            if(tail>=head) \
                ptp_bypass4("invalid head tail %d,%d,%d",tail,head,ptp_framebuf_size);\
            ptp_framebuf_size = head;\
            head = 0;\
        }\
        filter->frame_head = head;\
        ptp_consume(filter,&flags,NULL);\
    }while(0)

    spinlock_t lock;

    /* For holding client ptp frame request data phase urb from client */
    struct list_head client_queue;

    /* For holding pending ptp cammnd or data urb due to ptp busy or buffer full*/
    struct list_head request_queue;

    /* For holding free urb for ptp frame request */
    struct list_head free_list;
#define PTP_FREE_URB_COUNT 2

    /* For holding submitted urb that is originated from us */
    struct list_head busy_queue;

    u32 trans_id;
    u32 rx_trans_id;
    struct urb *frame_serving_urb;

    struct ptp_state_machine tx;
    struct ptp_state_machine rx;
    struct ptp_state_machine self;
    struct ptp_state_machine send;
};
    
/* Shortcut device info, so that we don't need to intercept
 * the GetDeviceInfo response
 */
static struct {
    const char *model;
    unsigned short usb_vendor;
    unsigned short usb_product;
    unsigned long device_flags;

	uint16_t StandardVersion;
	uint32_t VendorExtensionID;
	uint16_t VendorExtensionVersion;
} models[] = {
    {"Canon:EOS 650D",0x04a9, 0x323b, PTP_CAP|PTP_CAP_PREVIEW,0,PTP_VENDOR_CANON,0},
};

/* Supported vendor extension
 */
static unsigned vendors[] = {
    PTP_VENDOR_CANON,
};

#define ptp_bypass_(_ret,_flags,_reason,...) \
    do{\
        pr_debug(_reason "\n",##__VA_ARGS__);\
        ptp_change_state(filter,0,ptpfs_bypassed);\
        spin_unlock_irqrestore(&filter->lock,_flags);\
        _ret;\
    }while(0)

#define ptp_bypass(_reason,...) ptp_bypass_(return(0),flags,_reason,##__VA_ARGS__)
#define ptp_bypass1_(_ret,_reason,...) \
    do{\
        unsigned long _flags;\
        spin_lock_irqsave(&filter->lock,_flags);\
        ptp_bypass_(_ret,_flags,_reason,##__VA_ARGS__);\
    }while(0)
#define ptp_bypass1(_reason,...) ptp_bypass1_(return(0),_reason,##__VA_ARGS__)
#define ptp_bypass2(_reason,...) ptp_bypass_(return,flags,_reason,##__VA_ARGS__)
#define ptp_bypass3(_reason,...) ptp_bypass_(return(0),*flags,_reason,##__VA_ARGS__)

static void ptp_data_complete(struct urb *urb);
static void ptp_cmd_complete(struct urb *urb);
static int ptp_parse(struct ptp_filter *filter, 
        struct ptp_state_machine *m, struct urb *urb);

static inline void ptp_change_state_(struct ptp_filter *filter, int lock, int state, int line)
{
    unsigned long flags;

    if(lock) spin_lock_irqsave(&filter->lock,flags);

    if(state < ptpfs_active && filter->state>=ptpfs_active)
        del_timer(&filter->timer);
    else if(state == ptpfs_sleep) {
        struct stub_priv *priv,*tmp;
        list_for_each_entry_safe(priv,tmp,&filter->request_queue,list) {
            if(priv->urb->complete == ptp_cmd_complete)
                list_move_tail(&priv->list,&filter->free_list);
        }
        del_timer(&filter->timer);
    }
    if(state != filter->state) {
        pr_debug("state change(%d) from %d to %d\n",line,filter->state,state);
        filter->state = state;
    }
    if(lock) spin_unlock_irqrestore(&filter->lock,flags);
}
#define ptp_change_state(f,l,s) ptp_change_state_(f,l,s,__LINE__)

#define ptp_produce(_filter,_flags,_priv) \
    (pr_debug("produce\n"),\
     ptp_produce_(_filter,_flags,_priv))
static int ptp_produce_(struct ptp_filter *filter,
        unsigned long *flags,
        struct stub_priv *priv);
#define ptp_consume(_filter,_flags,_priv) \
    (pr_debug("consume\n"),\
     ptp_consume_(_filter,_flags,_priv))
static int ptp_consume_(struct ptp_filter *filter,
        unsigned long *flags,
        struct stub_priv *priv);

static void ptp_timer(unsigned long ctx) {
    unsigned long flags;
    struct ptp_filter *filter = (struct ptp_filter *)ctx;
    struct stub_priv *priv;
    struct urb *urb;

    spin_lock_irqsave(&filter->lock,flags);

    if(filter->state < ptpfs_active ||
        filter->state == ptpfs_drop ||
        filter->state == ptpfs_sleep ||
        filter->state == ptpfs_sleep_wait) 
    {
        spin_unlock_irqrestore(&filter->lock,flags);
        pr_debug("timer abort in state %d\n",filter->state);
        return;
    }

    priv = list_first_entry_or_null(&filter->free_list,struct stub_priv,list);

    if(priv == NULL) 
        pr_debug("no free urb\n");
    else {
        pr_debug("timer %lu,%lu\n",jiffies,filter->trigger.jiffies);
        urb = priv->urb;
        urb->complete = ptp_cmd_complete;
        urb->transfer_buffer = filter->trigger.ptpdu;
        urb->transfer_buffer_length = le32_to_cpu(filter->trigger.ptpdu->length);
        urb->pipe = filter->out_pipe;

        filter->trigger.jiffies += frame_jiffies;

        list_move_tail(&priv->list,&filter->busy_queue);
        ptp_produce(filter,&flags,priv);
    }
    spin_unlock_irqrestore(&filter->lock,flags);
}

#define array_for_each_begin(_array,_pos) \
    do{\
        int _pos;\
        for(_pos=0;_pos<sizeof(_array)/sizeof(_array[0]);++_pos)
#define array_for_each_end }while(0)

/* intercepts GetDeviceInfo data phase to look for vendor extension id */
static int ptp_get_device_info(struct ptp_filter *filter,
        struct urb *urb)
{
    PTPUSBBulkContainer *pdu;
    if(urb->actual_length < 20) {
        /* minimum size includes the ptp header (12) + some DeviceInfo
         * dataset fields
         */
        return 0;
    }
    pdu = (PTPUSBBulkContainer *)urb->transfer_buffer;
    if(le16_to_cpu(pdu->type) != PTP_USB_CONTAINER_DATA ||
            le16_to_cpu(pdu->code) != PTP_OC_GetDeviceInfo) 
        return 0;

/* DeviceInfo pack/unpack */
#define PTP_di_StandardVersion       0
#define PTP_di_VendorExtensionID     2
#define PTP_di_VendorExtensionVersion    6
#define PTP_di_VendorExtensionDesc   8
#define PTP_di_FunctionalMode        8
#define PTP_di_OperationsSupported  10
    filter->info.StandardVersion =
        le16_to_cpu(pdu->payload.data[PTP_di_StandardVersion]);
    filter->info.VendorExtensionID = 
        le32_to_cpu(pdu->payload.data[PTP_di_VendorExtensionID]);
    filter->info.VendorExtensionVersion = 
        le16_to_cpu(pdu->payload.data[PTP_di_VendorExtensionVersion]);
    /* WARNING! If want more DeviceInfo field, please change the 
     * buffer length check above
     */
    array_for_each_begin(vendors,i) {
        if(vendors[i] == filter->info.VendorExtensionID) {
            pr_debug("ptp init for vendor %u\n",vendors[i]);
            ptp_change_state(filter,1,ptpfs_idle);
            return 0;
        }
    } array_for_each_end;
    pr_debug("ptp bypass for vendor %d\n",filter->info.VendorExtensionID);
    ptp_change_state(filter,1,ptpfs_bypassed);
    return 0;
}

#define ptp_list_move(_item,_list) \
    do{\
        unsigned long _flags;\
        spin_lock_irqsave(&filter->sdev->priv_lock,_flags);\
        list_move_tail(_item,_list);\
        spin_unlock_irqrestore(&filter->sdev->priv_lock,_flags);\
    }while(0)

static int ptp_check_preview(struct ptp_filter *filter, struct urb *urb);

/* caller must hold filter->lock before calling */
static int ptp_produce_(struct ptp_filter *filter,
        unsigned long *flags,
        struct stub_priv *priv) 
{
    int next = ptpfs_idle;
    int head = filter->frame_head, tail=filter->frame_tail;
    struct urb *urb;
    int pending;

    if(priv == NULL) {
        pending = 1;
        priv = list_first_entry_or_null(&filter->request_queue,struct stub_priv,list);
        if(!priv) return 0;
    }else
        pending = 0;

    urb = priv->urb;
    pr_debug("produce urb %p\n",urb);

    if(usb_pipeout(urb->pipe)) {
        if(ptp_check_preview(filter,urb)) {
            pr_debug("preview\n");
            if(filter->state == ptpfs_sleep)
                ptp_change_state(filter,0,ptpfs_active);
            else if(filter->state!=ptpfs_active || ptp_framebuf_nospace) {
                pr_debug("preview pending in state %d\n",filter->state);
                if(!pending) ptp_list_move(&priv->list,&filter->request_queue);
                return 0;
            }
            if(filter->frame_rx_tail > 0)
                filter->frame_rx_tail2 = filter->frame_rx_tail;
            filter->frame_rx_tail = head;
#define ptp_submit_urb \
            do{\
                if(pending) {\
                    if(urb->complete != ptp_cmd_complete && urb->complete != ptp_data_complete)\
                        ptp_list_move(&priv->list,&filter->sdev->priv_init);\
                    else\
                        list_move_tail(&priv->list,&filter->busy_queue);\
                }\
                urb->actual_length = 0;\
                urb->status = 0;\
                spin_unlock_irqrestore(&filter->lock,*flags);\
                pr_debug("submit urb %c %p,%u,%u\n",usb_pipeout(urb->pipe)?'>':'<',\
                        urb,((u8*)urb->transfer_buffer)-filter->frame_buffer,urb->transfer_buffer_length);\
                if(usb_pipeout(urb->pipe)) ptp_parse(filter,&filter->send,urb);\
                stub_submit_urb(filter->sdev,NULL,urb);\
                spin_lock_irqsave(&filter->lock,*flags);\
                return 1;\
            }while(0)
            next = ptpfs_busy;
        }else{
            switch(filter->state) {
            case ptpfs_idle:
                next = ptpfs_command;
                break;
            case ptpfs_sleep:
                next = ptpfs_sleep_wait;
                break;
            case ptpfs_active:
                next = ptpfs_wait;
                break;
            default:
                pr_debug("client request pending in state %d\n",filter->state);
                if(!pending) ptp_list_move(&priv->list,&filter->request_queue);
                return 1;
            }
        }
        ptp_change_state(filter,0,next);
        ptp_submit_urb;
        return 1;
    }
    
#define ptp_bypass5(_reason,...) \
    do{\
        pr_debug(_reason "\n", ##__VA_ARGS__);\
        ptp_change_state(filter,0,ptpfs_bypassed);\
        return 0;\
    }while(0)

    if(urb->complete != ptp_data_complete) {
        if(filter->state != ptpfs_busy && filter->state != ptpfs_drop) {
            pr_debug("client request pass through in state %d\n",filter->state);
            if(!pending) return 0;
            ptp_submit_urb;
        }
        pr_debug("client data request pending in state %d\n",filter->state);
        if(!pending) ptp_list_move(&priv->list,&filter->request_queue);
        return 1;
    }

    urb->transfer_buffer_length = ptp_framebuf_to_end_space;
    if(urb->transfer_buffer_length<filter->out_pipe_size) {
        if(tail<head) {
            if(ptp_framebuf_full_size-head >= filter->out_pipe_size) {
                ptp_framebuf_size = ptp_framebuf_full_size;
                pr_debug("frame buffer expand %d,%d",ptp_framebuf_size,head);
                urb->transfer_buffer_length = ptp_framebuf_size-head;
            }else if(tail) {
                pr_debug("head roll over %d,%d\n",ptp_framebuf_size,head);
                ptp_framebuf_size = head;
                head = 0;
                urb->transfer_buffer_length = tail;
            }
        }
        if(urb->transfer_buffer_length<filter->out_pipe_size) {
            if(filter->frame_rx_tail < 0) {
                pr_debug("buffer full, wait for consumer\n");
                if(!pending) ptp_list_move(&priv->list,&filter->request_queue);
                return 0;
            }
            pr_debug("buffer full, drop frame\n");
            ptp_change_state(filter,0,ptpfs_drop);
        }
    }
    if(filter->state == ptpfs_drop) {
        /* If we dropping the current frame, we have to read the complete
         * frame before discard, or else no further transaction can happen.
         * We are free to use all the buffer space left. The buffer space may
         * be seperated in half due to buffer pointer roll over. To make the
         * transfer more efficient, we shall choose the larger half. And because
         * the consumer may still be running, the buffer space may be increasing.
         * Hence, we re-check the buffer space every time.
         */
        while(1) {
            if(tail >= filter->frame_rx_tail)
                head = filter->frame_head = filter->frame_rx_tail;
            else if(ptp_framebuf_full_size-filter->frame_rx_tail > tail) {
                head = filter->frame_head = filter->frame_rx_tail;
                ptp_framebuf_size = ptp_framebuf_full_size;
            }else
                head = filter->frame_head = 0;
            urb->transfer_buffer_length = ptp_framebuf_to_end_space;
            /* If the left space is too small, we drop previous frame too */
            if(filter->frame_rx_tail2 >= 0 && filter->frame_count>1 && 
                urb->transfer_buffer_length < (filter->out_pipe_size<<5))
            {
                filter->frame_rx_tail = filter->frame_rx_tail2;
                filter->frame_rx_tail2 = -1;
                pr_debug("drop previous frame\n");
                continue;
            }
            break;
        }
    }

    urb->transfer_buffer = filter->frame_buffer + head;
    urb->transfer_buffer_length = PTP_TRANSFER_LENGTH(urb->transfer_buffer_length);
    if(!urb->transfer_buffer_length)
        ptp_bypass5("bug: transfer_length");

    ptp_submit_urb;
}

static inline int ptp_framebuf_next(struct ptp_filter *filter,
        int pt, int length)
{
    pt = (pt+length)%ptp_framebuf_size;
    if(pt & ptp_framebuf_align_mask)
        pt = ((pt>>ptp_framebuf_align_shift)+1)<<
            ptp_framebuf_align_shift;
    return pt;
}

/* caller must hold filter->lock before calling */
static int ptp_consume_(struct ptp_filter *filter,
        unsigned long *flags,
        struct stub_priv *priv)
{
    int ret = 0;
    int pending = 0;

    while(1) {
        int head = filter->frame_head, tail=filter->frame_tail;
        int length,copy_length;
        struct urb *urb;

        if(filter->state < ptpfs_active) {
            filter->frame_serving = 0;
            pr_debug("drop consume request\n");
            return ret;
        }
        if(priv == NULL) {
            priv = list_first_entry_or_null(&filter->client_queue,struct stub_priv,list);
            if(!priv) break;
            pending = 1;
        }

        ret = 1;

        urb = priv->urb;
        length = ptp_framebuf_to_end_count;
        pr_debug("consume urb %p, %d\n",urb,length);
        if(filter->frame_length == 0) {
            PTPUSBBulkHeader *ptpdu = (PTPUSBBulkHeader*)(filter->frame_buffer+tail);
            if(tail == filter->frame_rx_tail2)
                filter->frame_rx_tail2 = -1;
            else if(tail == filter->frame_rx_tail){
                if(filter->state==ptpfs_drop) break;
                filter->frame_rx_tail = -1;
            }
            if(length < sizeof(PTPUSBBulkHeader))
                break;
            filter->frame_pdu_end =
                le16_to_cpu(ptpdu->type)==PTP_USB_CONTAINER_RESPONSE;
            if(urb->actual_length) 
                ptp_bypass3("unexpected urb length %u",urb->actual_length);
            filter->frame_length = le32_to_cpu(ptpdu->length);
            if(filter->frame_length < sizeof(*ptpdu))
                ptp_bypass3("invalid ptpdu length %d",filter->frame_length);
            pr_debug("frame %u, %d\n",filter->frame_length,filter->frame_pdu_end);
        }
        copy_length = urb->transfer_buffer_length - urb->actual_length;
        if(copy_length > length)
            copy_length = length;
        if(copy_length > filter->frame_length)
            copy_length = filter->frame_length;
        if(copy_length == 0)
            break;
        pr_debug("frame copy %d\n",copy_length);
        memcpy(urb->transfer_buffer+urb->actual_length,
                filter->frame_buffer+tail,copy_length);
        memset(filter->frame_buffer+tail,0xff,copy_length);
        filter->frame_length -= copy_length;
        urb->actual_length += copy_length;

        tail = (tail+copy_length)%ptp_framebuf_size;
        if(tail < filter->frame_tail) {
            pr_debug("tail roll over\n");
            ptp_framebuf_size = ptp_framebuf_full_size;
        }
        if(filter->frame_length == 0) {
            ptp_framebuf_align(tail);
            if(filter->frame_pdu_end) {
                filter->frame_serving = 0;
                --filter->frame_count;
                pr_debug("done frame serving %d\n",filter->frame_count);
            }
        }
        filter->frame_tail = tail;

        if(filter->frame_length==0 || 
                urb->actual_length==urb->transfer_buffer_length) {
            if(pending) ptp_list_move(&priv->list,&filter->sdev->priv_init);
            spin_unlock_irqrestore(&filter->lock,*flags);
#define ptp_complete_urb(_u) \
            do{\
                pr_debug("urb complete %p\n",_u);\
                filter->frame_serving_urb = _u;\
                _u->status = 0;\
                _u->complete(_u);\
                filter->frame_serving_urb = 0;\
            }while(0)
            ptp_complete_urb(urb);
            spin_lock_irqsave(&filter->lock,*flags);
            priv = NULL;
        }

        ptp_produce(filter,flags,NULL);
    }

    if(!pending && priv){
        pr_debug("pending frame serving %p\n",priv->urb);
        ptp_list_move(&priv->list,&filter->client_queue);
    }
    return ret;
}

static void ptp_data_complete(struct urb *urb) {
    PTPUSBBulkHeader *ptpdu = (PTPUSBBulkHeader*)urb->transfer_buffer;
	struct stub_priv *priv = (struct stub_priv *) urb->context;
	struct ptp_filter *filter = (struct ptp_filter *)priv->priv; 
	unsigned long flags;
    int head,tail;

    if(urb->status) {
        unsigned long flags;
        spin_lock_irqsave(&filter->lock,flags);
        list_move_tail(&priv->list,&filter->free_list);
        ptp_bypass2("urb failed %d",urb->status);
    }

    ptp_parse(filter,&filter->self,urb);

    spin_lock_irqsave(&filter->lock,flags);

    head = filter->frame_head; 
    tail=filter->frame_tail;
    pr_debug("data complete %p,%u, %u, %d,%d\n",
            urb,((u8*)urb->transfer_buffer)-filter->frame_buffer,urb->actual_length,head,tail);

    if(filter->state!=ptpfs_busy && filter->state!=ptpfs_drop) {
#define ptp_bypass4(_msg,...) \
        do{\
            list_move_tail(&priv->list,&filter->free_list);\
            ptp_bypass2(_msg,##__VA_ARGS__);\
        }while(0)
        ptp_bypass4("invalid state %d,%d", urb->status,filter->state);
    }

    if(filter->frame_rx_length == 0) {
        if(urb->actual_length < sizeof(PTPUSBBulkHeader))
            ptp_bypass4("invalid urb rx size %u", urb->actual_length);
        filter->frame_rx_length = le32_to_cpu(ptpdu->length);
        if(filter->frame_rx_length < sizeof(PTPUSBBulkHeader))
            ptp_bypass4("invalid pdu length %u", filter->frame_rx_length);
        filter->frame_rx_pdu_end =
            le16_to_cpu(ptpdu->type)==PTP_USB_CONTAINER_RESPONSE;
    }
    filter->frame_rx_length -= urb->actual_length;
    if(filter->state != ptpfs_drop)
        head = (head+urb->actual_length)%ptp_framebuf_size;

    if(filter->frame_rx_length == 0) {
        ptp_framebuf_align(head);

        if(filter->frame_rx_pdu_end) {
            list_move_tail(&priv->list,&filter->free_list);
            priv = NULL;
            if(filter->state == ptpfs_drop) {
                head = filter->frame_rx_tail;
                if(head >= tail) 
                    ptp_framebuf_size = ptp_framebuf_full_size;
                ptp_change_state(filter,0,ptpfs_sleep);
            }else if(++filter->frame_count >= usbip_ptp_buffer_count){
                pr_debug("max buffer count reached %d, sleep\n",filter->frame_count);
                ptp_change_state(filter,0,ptpfs_sleep);
            } else {
                if(filter->self.ptpdu.code != PTP_RC_OK){
                    pr_debug("frame done with error %04x, %d\n",
                            filter->self.ptpdu.code,filter->frame_count);
                    ptp_change_state(filter,0,ptpfs_sleep);
                }else{
                    pr_debug("frame done %d\n",filter->frame_count);
                    ptp_change_state(filter,0,ptpfs_active);
                    if(filter->trigger.jiffies < jiffies) 
                        filter->trigger.jiffies = jiffies + 1;
                    mod_timer(&filter->timer, filter->trigger.jiffies);
                }
            }
        }
    }
    filter->frame_head = head;
    ptp_produce(filter,&flags,priv);
    ptp_consume(filter,&flags,NULL);
    spin_unlock_irqrestore(&filter->lock,flags);
}

static void ptp_cmd_complete(struct urb *urb) {
	struct stub_priv *priv = (struct stub_priv *) urb->context;
	struct ptp_filter *filter = (struct ptp_filter *)priv->priv; 
	unsigned long flags;
    int head,tail;

    pr_debug("ptp cmd complete %p,%u,%u\n",
            urb,((u8*)urb->transfer_buffer)-filter->frame_buffer,urb->actual_length);

    spin_lock_irqsave(&filter->lock,flags);
    head = filter->frame_head; 
    tail=filter->frame_tail;
    if(urb->status || filter->state < ptpfs_active) 
        ptp_bypass4("urb complete with error %d,%d", urb->status,filter->state);

    filter->frame_rx_length = 0;
    urb->transfer_buffer = filter->frame_buffer + head;
    urb->complete = ptp_data_complete;
    urb->actual_length = 0;
    urb->status = 0;
    urb->pipe = filter->in_pipe;
    urb->transfer_buffer_length = PTP_TRANSFER_LENGTH(ptp_framebuf_to_end_space);
    if(!urb->transfer_buffer_length)
        ptp_bypass2("bug: invalid transfer length");
    spin_unlock_irqrestore(&filter->lock,flags);
    pr_debug("submit urb %p,%u, %u\n",
            urb,((u8*)urb->transfer_buffer)-filter->frame_buffer,urb->transfer_buffer_length);
    stub_submit_urb(filter->sdev,NULL,urb);
    return;
}

static int ptp_check_preview(struct ptp_filter *filter, struct urb *urb) {
    PTPUSBBulkContainer *ptpdu = (PTPUSBBulkContainer*)urb->transfer_buffer;
    switch(filter->info.VendorExtensionID) {
    case PTP_VENDOR_CANON:
        /* TODO add support for PowerShot operation PTP_OC_CANON_GetViewfinderImage */
        switch(le16_to_cpu(ptpdu->code)) {
        case PTP_OC_CANON_EOS_GetViewFinderData:
            return 1;
        }
    }
    return 0;
}

static inline void ptp_check_new_session(struct ptp_filter *filter,
        struct ptp_state_machine *m)
{
    unsigned long flags;
    if(m!=&filter->rx) return;
    if(m->ptpdu.type==PTP_USB_CONTAINER_COMMAND) {
        if(m->ptpdu.code==PTP_OC_OpenSession || m->ptpdu.code==PTP_OC_CloseSession) {
            spin_lock_irqsave(&filter->lock,flags);
            if(m->ptpdu.code == PTP_OC_OpenSession) filter->trans_id = 0;
            pr_debug("session change %u\n",m->ptpdu.code);
            ptp_change_state(filter,0,ptpfs_idle);
        }
    }
}

static int ptp_parse(struct ptp_filter *filter, 
        struct ptp_state_machine *m, struct urb *urb)
{
    u8 *data = (u8*)urb->transfer_buffer;
    unsigned length = usb_pipein(urb->pipe)?urb->actual_length
        :urb->transfer_buffer_length;
    int done=0;

    /* the following loop is to deal with ptp transaction state tracking and 
     * transaction id correction for both tx and rx
     */
    while(length) {
        PTPUSBBulkHeader *ptpdu = (PTPUSBBulkHeader *)data;
        switch(m->state) {
        case ptpps_wait_response:
        case ptpps_none:
            if(length < sizeof(PTPUSBBulkHeader)) {
                /* We cannot handle cases where the header is broken half
                 * by the bulk transfer. We need to add another state
                 * to handle this. It is complicated since we need to modify
                 * the trans_id. It seems to be illeagle to contain more
                 * than one ptp pdu inside one bulk transfer. So we may be
                 * fine here.
                 */
                ptp_bypass1("%s %p invalid size%u",m->type,urb,length);
            }
            pr_debug(" %s header: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                    m->type, data[0], data[1], data[2], data[3], data[4], data[5], data[6],
                    data[7], data[8], data[9], data[10], data[11]);
            m->ptpdu.length = le32_to_cpu(ptpdu->length);
            m->ptpdu.type = le16_to_cpu(ptpdu->type);
            m->ptpdu.code = le16_to_cpu(ptpdu->code);
            m->ptpdu.trans_id = le32_to_cpu(ptpdu->trans_id);
            ptp_check_new_session(filter,m);
            if(m==&filter->rx) {
                /* backup the original trans id here */
                filter->rx_trans_id = ptpdu->trans_id;
                /* rewrite with the current trans id in case we'll
                 * send this urb immediately */
                ptpdu->trans_id = cpu_to_le32(filter->trans_id);
            }else if(m==&filter->tx) {
                /* restore the trans id before returning back to
                 * the client */
                ptpdu->trans_id = filter->rx_trans_id;
            }else if(m==&filter->send){
                /* we may delay the urb because we are in the middle of an
                 * transaction (remember, we are secretly sending preview
                 * request). So rewrite with the current trans id before
                 * actually submiting the urb */
                ptpdu->trans_id = cpu_to_le32(filter->trans_id);
            }
            length -= sizeof(PTPUSBBulkHeader);
            data += sizeof(PTPUSBBulkHeader);
            m->size = sizeof(PTPUSBBulkHeader);
            pr_debug("%s %p phase %u, code %04x, length %u, tid %u,%u\n",
                    m->type,
                    urb,
                    m->ptpdu.type,
                    m->ptpdu.code,
                    m->ptpdu.length,
                    m->ptpdu.trans_id,
                    le32_to_cpu(ptpdu->trans_id));
            switch(m->ptpdu.type) {
            case PTP_USB_CONTAINER_COMMAND:
                m->state = ptpps_command;
                break;
            case PTP_USB_CONTAINER_RESPONSE:
                m->state = ptpps_response;
                break;
            case PTP_USB_CONTAINER_DATA:
                m->state = ptpps_data;
                break;
            default:
                m->state = ptpps_unkonwn;
                ptp_bypass1("%s %p unknown code %u",m->type,urb,m->ptpdu.code);
            }
            /* fall through */
        default:
            if(length + m->size < m->ptpdu.length) {
                m->size += length;
                /* TODO check ptp pdu code and buffer for get_device_info ?  */
                length = 0;
                pr_debug("%s phase pending, need %u\n",m->type,m->ptpdu.length-m->size);
            }else {
                unsigned offset = m->ptpdu.length - m->size;
                length -= offset;
                data += offset;
                if(m->state == ptpps_response) {
                    m->state = ptpps_none;
                    done = 1;
                    if(m!=&filter->rx && urb!=filter->frame_serving_urb){
                        ++filter->trans_id;
                        pr_debug("%s increase tid %u",m->type,filter->trans_id);
                    }
                }else
                    m->state = ptpps_wait_response;
            }
        }
    }
    return done;
}

static int ptp_on_tx(struct usbip_filter *ufilter, 
        struct urb *urb) 
{
    struct ptp_filter *filter = 
        (struct ptp_filter*)ufilter->priv;
    int done;

    if(filter->state == ptpfs_bypassed ||
       !usb_pipebulk(urb->pipe))
        return 0;

    if(urb->status)
        ptp_bypass1("urb failed %d",urb->status);
       
    done = ptp_parse(filter,&filter->tx,urb);

#ifdef TRACE_STREAM
    return 0;
#endif

    if(usb_pipeout(urb->pipe)) 
        return 0;
    
    if(filter->state == ptpfs_init) {
        /* get_device_info is broken at the moment, need to buffer the  */
        /* data phase in case it is broken in half by a bulk transfer */

        /* ret = ptp_get_device_info(filter,urb); */
    }else if(done){
        unsigned long flags;
        switch(filter->state) {
        case ptpfs_wait:
            ptp_change_state(filter,1,ptpfs_active);
            break;
        case ptpfs_sleep_wait:
            ptp_change_state(filter,1,ptpfs_sleep);
            break;
        case ptpfs_command:
            ptp_change_state(filter,1,ptpfs_idle);
            break;
        default:
            pr_debug("parser state change to none in state %d\n",filter->state);
        }
        
        spin_lock_irqsave(&filter->lock,flags);
        ptp_produce(filter,&flags,NULL);
        spin_unlock_irqrestore(&filter->lock,flags);
    }
    return 0;
}

static int ptp_on_rx(struct usbip_filter *ufilter, 
        struct usbip_header *pdu, struct urb *urb) 
{
#ifdef PTP_TRACE_COMMAND
    int n,i;
    u32 *params;
#endif
    unsigned long flags;
    PTPUSBBulkContainer *ptpdu;
    int ret = 0;
    struct ptp_filter *filter = 
        (struct ptp_filter*)ufilter->priv;
    struct stub_priv *priv = (struct stub_priv *)urb->context;

    if(filter->state==ptpfs_bypassed ||
        !usb_pipebulk(urb->pipe)) 
        return 0;

    if(!filter->in_pipe && usb_pipein(urb->pipe)){
        filter->in_pipe = urb->pipe;
        pr_debug("in pipe %x\n",filter->in_pipe);
    }
    if(!filter->out_pipe && usb_pipeout(urb->pipe)) {
        struct usb_host_endpoint *ep = 
            priv->sdev->udev->ep_out[pdu->base.ep & 0x7f];
        filter->out_pipe_size = ep->desc.wMaxPacketSize;
        filter->out_pipe = urb->pipe;
        pr_debug("out pipe %x, %u\n",filter->out_pipe,filter->out_pipe_size);
    }

    if(usb_pipein(urb->pipe)) {
        spin_lock_irqsave(&filter->lock,flags);
        if(filter->frame_serving) {
            pr_debug("frame serving %p, %u\n",urb, urb->transfer_buffer_length);
            ret = ptp_consume(filter,&flags,priv);
        }else if(filter->state >= ptpfs_active){
            pr_debug("client data request %p in state %d, %u\n",
                    urb,filter->state,urb->transfer_buffer_length);
            ret = ptp_produce(filter,&flags,priv);
        }
        spin_unlock_irqrestore(&filter->lock,flags);
        return ret;
    }

    ptp_parse(filter,&filter->rx,urb);
#ifdef TRACE_STREAM
    return 0;
#endif

    if(filter->rx.state != ptpps_wait_response ||
        filter->rx.ptpdu.type != PTP_USB_CONTAINER_COMMAND)
        return 0;

    spin_lock_irqsave(&filter->lock,flags);

    ptpdu = (PTPUSBBulkContainer *)urb->transfer_buffer;

    if(ptp_check_preview(filter,urb)) {
        switch(filter->state) {
        case ptpfs_idle:
            /* stream on */
            pr_debug("usbip filter stream on\n");
            filter->frame_count = 0;
            filter->frame_rx_tail = -1;
            filter->frame_rx_tail2 = -1;
            filter->frame_head =
            filter->frame_tail = 0;
            filter->frame_length = 0;
            filter->trigger.pdu = *pdu;
            /* fall through */
        case ptpfs_sleep:
            filter->frame_serving = 1;
            ptp_change_state(filter,0,ptpfs_active);

            urb->actual_length = urb->transfer_buffer_length;

            if(filter->frame_count && filter->trigger.jiffies>jiffies) {
                mod_timer(&filter->timer, filter->trigger.jiffies);
                spin_unlock_irqrestore(&filter->lock,flags);
                ptp_complete_urb(urb);
                return 1;
            }

            filter->trigger.jiffies = jiffies;
            spin_unlock_irqrestore(&filter->lock,flags);

            ptp_complete_urb(urb);

            if(!filter->frame_buffer) {
                int i;
                filter->frame_buffer = kmalloc(ptp_framebuf_full_size,GFP_KERNEL);
                if(filter->frame_buffer == NULL) 
                    ptp_bypass1("kmalloc error");
                memset(filter->frame_buffer,0xfd,ptp_framebuf_full_size);
                filter->frame_buffer_size = ptp_framebuf_full_size;

                if(urb->transfer_buffer_length != le32_to_cpu(ptpdu->length)) 
                    ptp_bypass1("invalid ptpdu size");

                filter->trigger.ptpdu = kmemdup(ptpdu,urb->transfer_buffer_length,GFP_KERNEL);
                if(filter->trigger.ptpdu == NULL)
                    ptp_bypass1("no memory");

                for(i=0;i<PTP_FREE_URB_COUNT;++i) {
                    struct stub_priv *priv;
                    struct urb *urb = stub_build_urb(filter->sdev,&filter->trigger.pdu,
                            filter->trigger.ptpdu);
                    if(!urb) ptp_bypass1("urb alloc failed");
                    priv = (struct stub_priv *)urb->context;
                    priv->priv = filter;
                    ptp_list_move(&priv->list,&filter->free_list);
                }
            }
            ptp_timer((unsigned long)filter);
            return 1;
        default:
            if(filter->state >= ptpfs_active){ 
                if(filter->frame_serving)
                    ptp_bypass("multiple frame serving");
                pr_debug("frame serving in state %d\n",filter->state);
                filter->frame_serving = 1;
                spin_unlock_irqrestore(&filter->lock,flags);
                urb->actual_length = urb->transfer_buffer_length;
                ptp_complete_urb(urb);
                return 1;
            } 
            ptp_bypass("preview command in invalid state %d",filter->state);
        }
    }else{
        switch(filter->state) {
        case ptpfs_active:
            ptp_change_state(filter,0,ptpfs_wait);
            break;
        case ptpfs_sleep:
            ptp_change_state(filter,0,ptpfs_sleep_wait);
            break;
        case ptpfs_idle:
            ptp_change_state(filter,0,ptpfs_command);
            break;
        case ptpfs_drop:
        case ptpfs_busy:
            pr_debug("client request pending\n");
            ptp_list_move(&priv->list,&filter->request_queue);
            ret = 1;
            break;
        default:
            ptp_bypass("command received in invalid state %d",filter->state);
        }
    }

    spin_unlock_irqrestore(&filter->lock,flags);
    return ret;
}

static void *ptp_probe(struct usbip_device *ud,
        struct usb_interface *interface)
{
	struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_device_descriptor *udept = &udev->descriptor;
    int index = -1;
    struct ptp_filter *filter;

    if(ud->side != USBIP_STUB)
        return NULL;

    array_for_each_begin(models,i) {
        if(models[i].usb_vendor == udept->idVendor &&
           models[i].usb_product == udept->idProduct) {
            index = i;
            break;
        }
    }array_for_each_end;

    if(index < 0) {
#ifndef PTP_CONFIG_PARSE_DEVICE_INFO
        pr_debug("usbip filter ptp bypass %04x:%04x\n",
                udept->idVendor,udept->idProduct);
        return NULL;
#endif
        if(udept->bDeviceClass != USB_CLASS_PTP ||
            udept->bDeviceSubClass != 1 ||
            udept->bDeviceProtocol != 1) 
        {
            pr_debug("usbip filter ptp bypass%04x:%04x with %d-%d-%d\n",
                udept->idVendor,udept->idProduct,
                udept->bDeviceClass,
                udept->bDeviceSubClass,
                udept->bDeviceProtocol);
            return NULL;
        }
    }

    filter = kzalloc(sizeof(struct ptp_filter),GFP_KERNEL);
    spin_lock_init(&filter->lock);
    INIT_LIST_HEAD(&filter->client_queue);
    INIT_LIST_HEAD(&filter->request_queue);
    INIT_LIST_HEAD(&filter->free_list);
    INIT_LIST_HEAD(&filter->busy_queue);
    filter->rx.type = "rx";
    filter->tx.type = "tx";
    filter->self.type = "self";
    filter->send.type = "send";
    filter->sdev = container_of(ud, struct stub_device, ud);
    setup_timer(&filter->timer,ptp_timer,(unsigned long)filter);

    if(index>=0) {
        filter->info.StandardVersion = models[index].StandardVersion;
        filter->info.VendorExtensionID = models[index].VendorExtensionID;
        filter->info.VendorExtensionVersion = models[index].VendorExtensionVersion;
        ptp_change_state(filter,0,ptpfs_idle);
    }

    pr_debug("usbip filter ptp attached to %04x:%04x\n",
            udept->idVendor,udept->idProduct);
    return filter;
}

static void ptp_remove(struct usbip_filter *ufilter) {
    struct stub_priv *priv,*tmp;
    struct ptp_filter *filter = (struct ptp_filter*)ufilter->priv;

    ptp_change_state(filter,1,ptpfs_idle);

    if(filter->frame_buffer) kfree(filter->frame_buffer);
    if(filter->trigger.ptpdu) kfree(filter->trigger.ptpdu);

    list_for_each_entry_safe(priv,tmp,&filter->free_list,list)
        stub_free_priv_and_urb(priv);

    kfree(filter);
}

static struct usbip_filter_driver ptp_driver = {
    .list = LIST_HEAD_INIT(ptp_driver.list),
    .name = DRIVER_DESC,
    .probe = ptp_probe,
    .remove = ptp_remove,
    .on_rx = ptp_on_rx,
    .on_tx = ptp_on_tx,
};

static int __init ptp_init(void)
{
	pr_info(DRIVER_DESC " v" DRIVER_VERSION "\n");
    usbip_filter_register(&ptp_driver);
    if(!usbip_ptp_fps) usbip_ptp_fps = 10;
    frame_jiffies = HZ/usbip_ptp_fps;
    if(!frame_jiffies) frame_jiffies = 1;
    pr_debug("frame jiffies %u\n",frame_jiffies);
	return 0;
}

static void __exit ptp_exit(void)
{
    usbip_filter_unregister(&ptp_driver);
	return;
}

module_init(ptp_init);
module_exit(ptp_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
