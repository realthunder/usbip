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

#define TRACE_STREAM

/* Desired frame rate for preview */
unsigned int usbip_ptp_fps = 10;
EXPORT_SYMBOL_GPL(usbip_ptp_fps);
module_param(usbip_ptp_fps, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(usbip_ptp_fps, "ptp preview streaming frames per second");

/* Auto shutdown of preview stream when dropped this number of frames */
unsigned int usbip_ptp_drop = 20;
EXPORT_SYMBOL_GPL(usbip_ptp_drop);
module_param(usbip_ptp_drop, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(usbip_ptp_drop, "ptp preview streaming auto shutdown on frame drop count");

unsigned int frame_jiffies;

enum ptp_state {

    /* seeking ptp vendor */
    ptps_init = 0,

    /* unsupported ptp device */
    ptps_bypassed = 1,

    /* vendor found, no interception */
    ptps_idle = 2,

    /* filtering active, can insert new command */
    ptps_active = 3,

    /* command phase, must wait for data or response phase before inserting new command */
    ptps_command = 4,
};

enum ptp_tx_state {
    /* response phase just finished, ready to accept new command */
    ptpts_none = 0,

    /* in the middle of data phase */
    ptpts_data = 1,

    /* in the middle of response phase */
    ptpts_response = 2,

    /* in the middle of an unknown phase */
    ptpts_unkonwn = 3,

    /* in the middle of receiving the ptp pdu header */
    ptpts_header = 4,

    /* not in the middle of any phase, and waiting for the response phase */
    ptpts_wait_response = 5,
};

struct ptp_filter {
    struct stub_device *sdev;
    struct timer_list timer;
    int state;
    int tx_state;

    PTPDeviceInfo info;

    struct {
        struct usbip_header pdu;
        PTPUSBBulkContainer *ptpdu;
        unsigned long jiffies;
        unsigned long sequm;
    }trigger;

    int drop_frames;

    spinlock_t lock;

    /* If none frames are available when a request is received,
     * we hold the urb, and try to serve it later when any
     * frame is ready
     */
    struct list_head priv_init;

    /* priv_tx is for holding ready streamed frames */
    struct list_head priv_tx;

    /* Because PTP must follow serialized command->(data)->response phases,
     * we may have to delay submitting intercepted command, by queuing the
     * request in priv_rx, in case we are in the middle of streaming command phase. 
     */
    struct list_head priv_rx;

    unsigned long trans_id;
    unsigned long trans_id_offset;

    /* For buffering incomplete ptp pdu, because multiple bulk read may break
     * the pdu in the middle. */
    PTPUSBBulkContainer ptpdu;

    /* Current buffered ptp pdu size */
    int ptpdu_size;
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

static void ptp_resume_rx(struct ptp_filter *filter) {
    unsigned long flags, flags2;
    struct stub_priv *priv, *tmp;

    spin_lock_irqsave(&filter->lock,flags);
    
    if(filter->state == ptps_active) {
        list_for_each_entry_safe(priv, tmp, &filter->priv_rx, list) {
            spin_lock_irqsave(&filter->sdev->priv_lock,flags2);
            list_move_tail(&priv->list,&filter->sdev->priv_init);
            spin_unlock_irqrestore(&filter->sdev->priv_lock,flags2);
            spin_unlock_irqrestore(&filter->lock,flags);
            stub_submit_urb(filter->sdev,&filter->trigger.pdu,priv->urb);
            return;
        }
    }
    spin_unlock_irqrestore(&filter->lock,flags);
}

static void ptp_change_state_(struct ptp_filter *filter, int lock, int state, int line)
{
    struct stub_priv *priv, *tmp;
    unsigned long flags;

    if(lock) spin_lock_irqsave(&filter->lock,flags);

    if(state != filter->state) {
        pr_debug("state change(%d) from %d to %d\n",line,filter->state,state);
        filter->state = state;

        switch(state) {
        case ptps_idle:
            del_timer(&filter->timer);
            if(filter->trigger.ptpdu) {
                kfree(filter->trigger.ptpdu);
                filter->trigger.ptpdu = NULL;
            }
            list_for_each_entry_safe(priv, tmp, &filter->priv_tx, list) {
                stub_free_priv_and_urb(priv);
            }
            /* fall through */
        case ptps_active:
            if(lock) spin_unlock_irqrestore(&filter->lock,flags);
            ptp_resume_rx(filter);
            return;
        }
    }

    if(lock) spin_unlock_irqrestore(&filter->lock,flags);
}
#define ptp_change_state(f,l,s) ptp_change_state_(f,l,s,__LINE__)

static void ptp_complete(struct urb *urb);

static void ptp_timer(unsigned long ctx) {
    unsigned long flags;
    struct ptp_filter *filter = (struct ptp_filter *)ctx;
    struct stub_priv *priv;
    struct urb *urb;

    filter->trigger.jiffies += frame_jiffies;
    if(filter->trigger.jiffies < jiffies) 
        filter->trigger.jiffies = jiffies + 1;
    mod_timer(&filter->timer, filter->trigger.jiffies);

    spin_lock_irqsave(&filter->lock,flags);
    if(filter->state != ptps_active) {
        /* pr_debug("ptp timer wake up in state %i\n",filter->state); */
    } else {
        filter->trigger.ptpdu->trans_id = cpu_to_le32(++filter->trans_id);
        ++filter->trans_id_offset;

        urb = stub_build_urb(filter->sdev,&filter->trigger.pdu,
                filter->trigger.ptpdu);
        if(urb) {
            priv = (struct stub_priv *)urb->context;
            priv->priv = filter;
            urb->complete = ptp_complete;
            ptp_change_state(filter,0,ptps_command);
            pr_debug("insert trans id %d\n",le32_to_cpu(filter->trans_id-1));
            spin_unlock_irqrestore(&filter->lock,flags);
            stub_submit_urb(filter->sdev,&filter->trigger.pdu,urb);
            return;
        }
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
            ptp_change_state(filter,1,ptps_idle);
            return 0;
        }
    } array_for_each_end;
    pr_debug("ptp bypass for vendor %d\n",filter->info.VendorExtensionID);
    ptp_change_state(filter,1,ptps_bypassed);
    return 0;
}

static int ptp_stream_serve(struct ptp_filter *filter,
        struct stub_priv *priv) 
{
    int ret = 0;
    unsigned long flags,flags2;
    struct stub_priv *tx;
    
    spin_lock_irqsave(&filter->lock,flags);

    if(filter->state == ptps_active) {
        if(priv) {
            spin_lock_irqsave(&filter->sdev->priv_lock,flags2);
            list_move_tail(&priv->list,&filter->priv_init);
            spin_unlock_irqrestore(&filter->sdev->priv_lock,flags2);
        }

        /* Here are the things we are going to do,
         *   grab a pending preview request urb, 
         *   find a pending preview result urb,
         *   exchange the urb buffers,
         *   free the result urb
         *   move request urb back to stub device
         *   call stub_complete to notify the completion
         */
        list_for_each_entry(priv,&filter->priv_init,list) {
            struct urb *urb = priv->urb;
            PTPUSBBulkContainer *ptpdu = (PTPUSBBulkContainer*)urb->transfer_buffer;

            list_for_each_entry(tx,&filter->priv_tx,list) {
                PTPUSBBulkContainer *tx_ptpdu = (PTPUSBBulkContainer*)tx->urb->transfer_buffer;
                u32 tmp_length = urb->transfer_buffer_length;
                pr_debug("stream serve trans id %u\n",le32_to_cpu(ptpdu->trans_id));
                tx_ptpdu->trans_id = ptpdu->trans_id;
                urb->transfer_buffer = tx_ptpdu;
                urb->transfer_buffer_length = tx->urb->transfer_buffer_length;
                urb->actual_length = tx->urb->actual_length;
                tx->urb->transfer_buffer = ptpdu;
                tx->urb->transfer_buffer_length = tmp_length;
                tx->urb->status = urb->status;
                filter->drop_frames = 0;

                stub_free_priv_and_urb(tx);

                spin_lock_irqsave(&filter->sdev->priv_lock,flags2);
                list_move_tail(&priv->list,&filter->sdev->priv_init);
                spin_unlock_irqrestore(&filter->sdev->priv_lock,flags2);

                spin_unlock_irqrestore(&filter->lock,flags);

                stub_complete(priv->urb);
                return 1;
            }
        }
        ret = 1;
    }
    spin_unlock_irqrestore(&filter->lock,flags);
    return ret;
}

static void ptp_complete(struct urb *urb) {
    struct stub_priv *p,*tmp;
	struct stub_priv *priv = (struct stub_priv *) urb->context;
	struct ptp_filter *filter = (struct ptp_filter *)priv->priv; 
	unsigned long flags;

    spin_lock_irqsave(&filter->lock,flags);
    if(urb->status) {
		pr_debug("usbip_ptp urb completion with non-zero status "
			 "%d\n", urb->status);
        stub_free_priv_and_urb(priv);
        spin_unlock_irqrestore(&filter->lock,flags);
        return;
    }
    if(filter->state < ptps_active) {
        pr_debug("ptp_complete in state %d\n",filter->state);
        stub_free_priv_and_urb(priv);
        spin_unlock_irqrestore(&filter->lock,flags);
        return;
    } else {
        list_for_each_entry_safe(p,tmp,&filter->priv_tx,list) {
            ++filter->drop_frames;
            stub_free_priv_and_urb(p);
        }
        if(filter->drop_frames >= usbip_ptp_drop) {
            pr_debug("frame drop limit reached\n");
            spin_unlock_irqrestore(&filter->lock,flags);
            /* stream off */
            ptp_change_state(filter,1,ptps_idle);
            return;
        }else
            list_move_tail(&priv->list,&filter->priv_tx);
    }
    spin_unlock_irqrestore(&filter->lock,flags);

    ptp_change_state(filter,1,ptps_active);
    ptp_stream_serve(filter,NULL);
}

static int ptp_check_preview(struct ptp_filter *filter,
        struct usbip_header *pdu, PTPUSBBulkContainer *ptpdu)
{
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

static inline u32 ptp_rx_correct_trans_id(
        struct ptp_filter *filter, 
        u32 trans_id)
{
    return le32_to_cpu(trans_id)+filter->trans_id_offset;
}

static inline u32 ptp_tx_correct_trans_id(
        struct ptp_filter *filter,
        u32 trans_id)
{
    return le32_to_cpu(trans_id)-filter->trans_id_offset;
}

static inline void ptp_tx_read_header(
        struct ptp_filter *filter, void *data) 
{
    PTPUSBBulkContainer *ptpdu = (PTPUSBBulkContainer *)data;
    filter->ptpdu.length = le32_to_cpu(ptpdu->length);
    filter->ptpdu.type = le16_to_cpu(ptpdu->type);
    filter->ptpdu.code = le16_to_cpu(ptpdu->code);
    filter->ptpdu.trans_id = 
            ptp_tx_correct_trans_id(filter,ptpdu->trans_id);
}

static int ptp_on_tx(struct usbip_filter *ufilter, 
        struct urb *urb) 
{
    PTPUSBBulkContainer *ptpdu;
    int ret = 0;
    u8 *data = (u8*)urb->transfer_buffer;
    unsigned length = urb->actual_length;
    struct ptp_filter *filter = 
        (struct ptp_filter*)ufilter->priv;

    if(filter->state == ptps_bypassed ||
       !usb_pipein(urb->pipe) || 
       !usb_pipebulk(urb->pipe))
        return 0;
       
    if(urb->status) {
        pr_debug("usbip filter tx urb failed %d\n",urb->status);
        return 0;
    }

    pr_debug("tx %u\n",urb->actual_length);

    /* the following loop is to deal with ptp transaction state tracking and 
     * transaction id correction
     */
    while(length) {
        switch(filter->tx_state) {
        case ptpts_wait_response:
        case ptpts_none:
            /* see below for the reason of this init trans_id */
            filter->ptpdu.trans_id = 0xffffffff;
            filter->tx_state = ptpts_header;
            if(length >= 12) {
                ptpdu = (PTPUSBBulkContainer *)data;
                ptp_tx_read_header(filter,data);
                ptpdu->trans_id = cpu_to_le32(filter->ptpdu.trans_id);
                length -= 12;
                data += 12;
                filter->ptpdu_size = 12;
            }else
                filter->ptpdu_size = 0;
            /* fall through */
        case ptpts_header:
            if(filter->ptpdu_size < 12) {
                union {
                    u32 id;
                    u8 bytes[4];
                } trans;
                u8 *dst = (u8*)&filter->ptpdu;
                unsigned copy_length = filter->ptpdu_size+length>12?
                    12-filter->ptpdu_size:length;
                memcpy(dst+filter->ptpdu_size,data,copy_length);
                if(filter->ptpdu_size + copy_length > 8) {
                    /* Now we have a tricky situation. The trans_id field may be broken in
                     * the middle by bulk transfer. Thankfully, ptp standard uses little
                     * endian, we don't have to wait for all the bytes before correcting
                     * the trans_id. And because sending back the trans_id requires substracting
                     * the offset, we initialize the trans_id as 0xffffffff in previous state,
                     */
                    unsigned offset = filter->ptpdu_size<=8?8-filter->ptpdu_size:0;
                    trans.id = cpu_to_le32(ptp_tx_correct_trans_id(filter,filter->ptpdu.trans_id));
                    pr_debug("ptp trans_id fix %u,%u,%u",filter->ptpdu_size,copy_length,offset);
                    memcpy(data+offset,trans.bytes+filter->ptpdu_size+offset-8,
                            copy_length-offset);
                }
                filter->ptpdu_size += copy_length;
                length -= copy_length;
                data += copy_length;

                if(filter->ptpdu_size < 12) break;
                ptp_tx_read_header(filter,&filter->ptpdu);
            }
            pr_debug("ptp phase %u, tid %u\n",
                    filter->ptpdu.type,filter->ptpdu.trans_id);
            switch(filter->ptpdu.type) {
            case PTP_USB_CONTAINER_RESPONSE:
                filter->tx_state = ptpts_response;
                break;
            case PTP_USB_CONTAINER_DATA:
                filter->tx_state = ptpts_data;
                break;
            default:
                filter->tx_state = ptpts_unkonwn;
            }
            /* fall through */
        default:
            if(length + filter->ptpdu_size < filter->ptpdu.length) {
                filter->ptpdu_size += length;
                /* TODO check ptp pdu code and buffer for get_device_info ?  */
                length = 0;
                pr_debug("ptp phase pending, need %u\n",filter->ptpdu.length-filter->ptpdu_size);
            }else {
                unsigned offset = filter->ptpdu.length - filter->ptpdu_size;
                length -= offset;
                data += offset;
                pr_debug("ptp phase done\n");
                if(filter->tx_state == ptpts_response) 
                    filter->tx_state = ptpts_none;
                else
                    filter->tx_state = ptpts_wait_response;
            }
        }
    }

    if(filter->state == ptps_init) {
        /* get_device_info is broken at the moment, need to buffer the  */
        /* data phase in case it is broken in half by a bulk transfer */

        /* ret = ptp_get_device_info(filter,urb); */
    } else if(filter->state == ptps_command &&
            filter->tx_state == ptpts_none) {
        ptp_change_state(filter,1,ptps_active);
    }
    return ret;
}

static int ptp_on_rx(struct usbip_filter *ufilter, 
        struct usbip_header *pdu, struct urb *urb) 
{
    unsigned long flags;
    PTPUSBBulkContainer *ptpdu;
    int ret = 0;
    struct ptp_filter *filter = 
        (struct ptp_filter*)ufilter->priv;
	struct stub_priv *priv = (struct stub_priv *) urb->context;

    if(filter->state == ptps_bypassed || 
       !usb_pipeout(urb->pipe) || 
       !usb_pipebulk(urb->pipe) ||
       urb->transfer_buffer_length < sizeof(PTPUSBBulkHeader))
        return 0;

    ptpdu = (PTPUSBBulkContainer *)urb->transfer_buffer;
    
    spin_lock_irqsave(&filter->lock,flags);

    pr_debug("ptp rx type %u, code%x, tid %lu+%lu\n",
            le32_to_cpu(ptpdu->type),
            le32_to_cpu(ptpdu->code),
            le32_to_cpu(ptpdu->trans_id),
            filter->trans_id_offset);
    if(le16_to_cpu(ptpdu->type) != PTP_USB_CONTAINER_COMMAND)
        ptpdu->trans_id = cpu_to_le32(ptp_rx_correct_trans_id(filter,ptpdu->trans_id));
    else{
        filter->trans_id = ptp_rx_correct_trans_id(filter,ptpdu->trans_id);
        ptpdu->trans_id = cpu_to_le32(filter->trans_id);

        if(filter->state == ptps_command) {
            unsigned long flags;
            spin_lock_irqsave(&filter->sdev->priv_lock,flags);
            list_move_tail(&priv->list,&filter->priv_rx);
            spin_unlock_irqrestore(&filter->sdev->priv_lock,flags);
            ret = 1;
        }else if(ptp_check_preview(filter,pdu,ptpdu)) {
            if(filter->state == ptps_idle) {
                /* stream on */
                pr_debug("usbip filter stream on\n");
#ifdef TRACE_STREAM
                return 0;
#endif
                filter->drop_frames = 0;
                filter->trigger.pdu = *pdu;
                filter->trigger.ptpdu = kmemdup(ptpdu,urb->actual_length,GFP_KERNEL);
                filter->trigger.jiffies = jiffies;
                if(filter->trigger.ptpdu == NULL){
                    dev_err(&filter->sdev->interface->dev, "ptp_filter allocate trigger\n");
                    usbip_event_add(&filter->sdev->ud, SDEV_EVENT_ERROR_MALLOC);
                    ptp_change_state(filter,0,ptps_bypassed);
                    spin_unlock_irqrestore(&filter->lock,flags);
                    return 0;
                }
                filter->trigger.jiffies += frame_jiffies;
                mod_timer(&filter->timer,filter->trigger.jiffies);
                ptp_change_state(filter,0,ptps_command);
                spin_unlock_irqrestore(&filter->lock,flags);
                return 0;
            } else if(filter->state == ptps_active){ 
                spin_unlock_irqrestore(&filter->lock,flags);
                return ptp_stream_serve(filter,priv);
            }
        }else if(filter->state == ptps_active) {
            spin_unlock_irqrestore(&filter->lock,flags);
            /* stream off */
            ptp_change_state(filter,1,ptps_idle);
            return 0;
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
    INIT_LIST_HEAD(&filter->priv_init);
    INIT_LIST_HEAD(&filter->priv_rx);
    INIT_LIST_HEAD(&filter->priv_tx);
    filter->sdev = container_of(ud, struct stub_device, ud);
    setup_timer(&filter->timer,ptp_timer,(unsigned long)filter);

    if(index>=0) {
        filter->info.StandardVersion = models[index].StandardVersion;
        filter->info.VendorExtensionID = models[index].VendorExtensionID;
        filter->info.VendorExtensionVersion = models[index].VendorExtensionVersion;
        ptp_change_state(filter,0,ptps_idle);
    }

    pr_debug("usbip filter ptp attached to %04x:%04x\n",
            udept->idVendor,udept->idProduct);
    return filter;
}

static void ptp_remove(struct usbip_filter *ufilter) {
    struct ptp_filter *filter = (struct ptp_filter*)ufilter->priv;
    ptp_change_state(filter,1,ptps_idle);
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
