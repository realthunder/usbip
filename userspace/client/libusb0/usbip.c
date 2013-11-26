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

#include <assert.h>
#include "../libusbip-client.h"
#include "usbi.h"

#undef err
#undef dbg
#define err(msg,...) fprintf(stderr,"err(%d): " msg "\n",__LINE__,##__VA_ARGS__)
#define dbg(msg,...) fprintf(stderr,"dbg(%d): " msg "\n",__LINE__,##__VA_ARGS__)

static libusbip_session_t *session;

static struct {
    usb_dev_handle *(*func_usb_open)(struct usb_device *dev);
    int (*func_usb_close)(usb_dev_handle *dev);
    int (*func_usb_get_string)(usb_dev_handle *dev, int index, int langid, char *buf,
        size_t buflen);
    int (*func_usb_get_string_simple)(usb_dev_handle *dev, int index, char *buf,
        size_t buflen);

    /* descriptors.c */
    int (*func_usb_get_descriptor_by_endpoint)(usb_dev_handle *udev, int ep,
        unsigned char type, unsigned char index, void *buf, int size);
    int (*func_usb_get_descriptor)(usb_dev_handle *udev, unsigned char type,
        unsigned char index, void *buf, int size);

    /* <arch>.c */
    int (*func_usb_bulk_write)(usb_dev_handle *dev, int ep, const char *bytes, int size,
        int timeout);
    int (*func_usb_bulk_read)(usb_dev_handle *dev, int ep, char *bytes, int size,
        int timeout);
    int (*func_usb_interrupt_write)(usb_dev_handle *dev, int ep, const char *bytes, int size,
            int timeout);
    int (*func_usb_interrupt_read)(usb_dev_handle *dev, int ep, char *bytes, int size,
            int timeout);
    int (*func_usb_control_msg)(usb_dev_handle *dev, int requesttype, int request,
        int value, int index, char *bytes, int size, int timeout);
    int (*func_usb_set_configuration)(usb_dev_handle *dev, int configuration);
    int (*func_usb_claim_interface)(usb_dev_handle *dev, int interface);
    int (*func_usb_release_interface)(usb_dev_handle *dev, int interface);
    int (*func_usb_set_altinterface)(usb_dev_handle *dev, int alternate);
    int (*func_usb_resetep)(usb_dev_handle *dev, unsigned int ep);
    int (*func_usb_clear_halt)(usb_dev_handle *dev, unsigned int ep);
    int (*func_usb_reset)(usb_dev_handle *dev);

    int (*func_usb_get_driver_np)(usb_dev_handle *dev, int interface, char *name,
        unsigned int namelen);
    int (*func_usb_detach_kernel_driver_np)(usb_dev_handle *dev, int interface);

    char *(*func_usb_strerror)(void);

    void (*func_usb_init)(void);
    void (*func_usb_set_debug)(int level);
    int (*func_usb_find_busses)(void);
    int (*func_usb_find_devices)(void);
    struct usb_device *(*func_usb_device)(usb_dev_handle *dev);
    struct usb_bus *(*func_usb_get_busses)(void);

    int (*func_usb_os_find_busses)(struct usb_bus **busses);
    int (*func_usb_os_find_devices)(struct usb_bus *bus, struct usb_device **devices);
    int (*func_usb_os_determine_children)(struct usb_bus *bus);
    void (*func_usb_os_init)(void);
    int (*func_usb_os_open)(usb_dev_handle *dev);
} hook;

#ifndef USBIP_NO_HOOK
#   include "uv.h"
#   define usb_os_find_busses usb_os_find_busses_
#   define usb_os_find_devices usb_os_find_devices_
#   define usb_os_determine_children usb_os_determine_children_
#   define usb_os_init usb_os_init_
#   define usb_os_open usb_os_open_
#   define usb_os_close usb_os_close_
#   define usb_set_configuration usb_set_configuration_
#   define usb_claim_interface usb_claim_interface_
#   define usb_release_interface usb_release_interface_
#   define usb_set_altinterface usb_set_altinterface_
#   define usb_control_msg usb_control_msg_
#   define usb_bulk_write usb_bulk_write_
#   define usb_bulk_read usb_bulk_read_
#   define usb_interrupt_write usb_interrupt_write_
#   define usb_interrupt_read usb_interrupt_read_
#   define usb_resetep usb_resetep_
#   define usb_clear_halt usb_clear_halt_
#   define usb_get_driver_np usb_get_driver_np_
#   define usb_detach_kernel_driver_np usb_detach_kernel_driver_np_
#   define usb_reset usb_reset_
#   if defined (__linux) || defined(linux)
#       include "linux.c"
#   elif defined(__FreeBSD__)
#       include "bsd.c"
#   elif defined(DARWIN)
#       include "darwin.c"
#   endif
#   undef usb_os_find_busses
#   undef usb_os_find_devices
#   undef usb_os_determine_children
#   undef usb_os_init
#   undef usb_os_open
#   undef usb_os_close
#   undef usb_set_configuration
#   undef usb_claim_interface
#   undef usb_release_interface
#   undef usb_set_altinterface
#   undef usb_control_msg
#   undef usb_bulk_write
#   undef usb_bulk_read
#   undef usb_interrupt_write
#   undef usb_interrupt_read
#   undef usb_resetep
#   undef usb_clear_halt
#   undef usb_get_driver_np
#   undef usb_detach_kernel_driver_np
#   undef usb_reset
static void hooklib() {
    uv_lib_t lib;
    const char *name = getenv("LIBUSBIP_HOOK");
    if(!name||!name[0])
        return;

    if(strcmp(name,"internal")) {
#   if defined(_WIN32) || defined(WIN32)
#   else
        hook.func_usb_os_find_busses = usb_os_find_busses_;
        hook.func_usb_os_find_devices = usb_os_find_devices_;
        hook.func_usb_os_determine_children = usb_os_determine_children_;
        hook.func_usb_os_init = usb_os_init_;
        hook.func_usb_os_open = usb_os_open_;
        hook.func_usb_close = usb_os_close_;
        hook.func_usb_reset = usb_reset_;
#   endif
        return;
    }
    if(uv_dlopen(name,&lib)) {
        err("failed to load libusb");
        return;
    }
#   define HOOK(_name) \
    do {\
        if(!uv_dlsym(&lib,#_name,(void**)&hook.func_##_name)) \
            err("failed to load libusb symbol " #_name);\
    }while(0)

    HOOK(usb_open);
    HOOK(usb_close);
    HOOK(usb_get_string);
    HOOK(usb_get_string_simple);
    HOOK(usb_get_descriptor_by_endpoint);
    HOOK(usb_get_descriptor);
    HOOK(usb_bulk_write);
    HOOK(usb_bulk_read);
    HOOK(usb_interrupt_write);
    HOOK(usb_interrupt_read);
    HOOK(usb_control_msg);
    HOOK(usb_set_configuration);
    HOOK(usb_claim_interface);
    HOOK(usb_release_interface);
    HOOK(usb_set_altinterface);
    HOOK(usb_resetep);
    HOOK(usb_clear_halt);
    HOOK(usb_reset);
    HOOK(usb_get_driver_np);
    HOOK(usb_detach_kernel_driver_np);
    HOOK(usb_strerror);
    HOOK(usb_init);
    HOOK(usb_set_debug);
    HOOK(usb_find_busses);
    HOOK(usb_find_devices);
    HOOK(usb_device);
    HOOK(usb_get_busses);
    return;
}
#else //LIBUSBIP_NO_HOOK
void hooklib() {}
#endif

static inline int is_usbip_bus(struct usb_bus *bus) {
    return strncmp(bus->dirname,"/usbip/",7)==0;
}

static inline int is_usbip_device(struct usb_device *dev) {
    return is_usbip_bus(dev->bus);
}

static inline const char *get_hostname(struct usb_bus *bus)
{
    assert(is_usbip_bus(bus));
    return bus->dirname+7;
}

#define CAST_DEVICE(_func,_args) \
    libusbip_device_t *dev = libusbip_device_cast(session,handle->impl_info); \
    if(!dev) {\
        if(hook.func_##_func)\
            return hook.func_##_func _args;\
        return -1;\
    }

struct usb_dev_handle *usb_os_open(struct usb_device *udev) {
    int ret;
    libusbip_device_t *dev = NULL;
    int busnum,devnum;
    usb_dev_handle *handle;

    if(hook.func_usb_open) {
        if(!is_usbip_device(udev)) 
            return hook.func_usb_open(udev);
    }

    handle = malloc(sizeof(*udev));
    if (!handle)
        return NULL;

    handle->fd = -1;
    handle->device = udev;
    handle->bus = udev->bus;
    handle->config = handle->interface = handle->altsetting = -1;

    if(!is_usbip_device(udev)) {
        if(hook.func_usb_os_open){
            if(!hook.func_usb_os_open(handle))
                return handle;
        }
        goto ON_ERROR;
    }

    ret = libusbip_device_open(session,&dev,
            get_hostname(handle->bus),handle->bus->location,
            handle->device->filename,5000,0);
    if(ret) goto ON_ERROR;
    handle->impl_info = dev;
    return handle;
ON_ERROR:
    free(handle);
    return NULL;
}

int usb_os_close(usb_dev_handle *handle) {
    CAST_DEVICE(usb_close, (handle));
    libusbip_device_close(dev);
    return 0;
}

int usb_set_configuration(usb_dev_handle *handle, int configuration)
{
    int ret = -1;
    CAST_DEVICE(usb_set_configuration, (handle,configuration));
    libusbip_control_msg(dev, USB_RECIP_DEVICE, USB_REQ_SET_CONFIGURATION,
            0, configuration, NULL, 0, &ret,1000,0);
    if(ret == 0) handle->config = configuration;
    return ret;
}

int usb_claim_interface(usb_dev_handle *handle, int interface)
{
    CAST_DEVICE(usb_claim_interface, (handle,interface));

    /* usbip binds to all active interfaces of a device, so,
     * as long as we successfully attached to that remote
     * device, no other attachment can happen, which means
     * we've owned all the interfaces, or is it?
     *
     * Or, maybe we should seperate device from interface,
     * and let usb_dev_handle represent an interface.
     */
    handle->interface = interface;
    return 0;
}

int usb_release_interface(usb_dev_handle *handle, int interface)
{
    CAST_DEVICE(usb_release_interface, (handle,interface));
    handle->interface = -1;
    return 0;
}

int usb_set_altinterface(usb_dev_handle *handle, int alternate)
{
    int ret = -1;
    CAST_DEVICE(usb_set_altinterface, (handle,alternate));

    if (handle->interface < 0)
        USB_ERROR(-EINVAL);

    libusbip_control_msg(dev, USB_RECIP_INTERFACE, USB_REQ_SET_INTERFACE,
            alternate, handle->interface, 0, 0, &ret,5000, 0);
    if(ret == 0) handle->altsetting = alternate;
    return ret;
}

int usb_control_msg(usb_dev_handle *handle, int requesttype, int request,
	int value, int index, char *bytes, int size, int timeout)
{
    int ret = -1;
    CAST_DEVICE(usb_control_msg, (handle, requesttype, 
        request, value, index, bytes, size, timeout));
    libusbip_control_msg(dev,requesttype,request,value,
            index,bytes,size,&ret,timeout,0);
    return ret;
}

int usb_bulk_write(usb_dev_handle *handle, int ep, const char *bytes, int size,
	int timeout)
{
    int ret = -1;
    CAST_DEVICE(usb_bulk_write,(handle,ep,bytes,size,timeout));
    libusbip_urb_transfer(dev,ep,USBIP_URB_TYPE_BULK,(void*)bytes,size,&ret,timeout,0);
    return ret;
}

int usb_bulk_read(usb_dev_handle *handle, int ep, char *bytes, int size,
	int timeout)
{
    int ret = -1;
    CAST_DEVICE(usb_bulk_read,(handle,ep,bytes,size,timeout));
    libusbip_urb_transfer(dev,ep,USBIP_URB_TYPE_BULK,bytes,size,&ret,timeout,0);
    return ret;
}

int usb_interrupt_write(usb_dev_handle *handle, int ep, const char *bytes, int size,
	int timeout)
{
    int ret = -1;
    CAST_DEVICE(usb_interrupt_write,(handle,ep,bytes,size,timeout));
    libusbip_urb_transfer(dev,ep,USBIP_URB_TYPE_INTERRUPT,(void*)bytes,size,&ret,timeout,0);
    return ret;
}

int usb_interrupt_read(usb_dev_handle *handle, int ep, char *bytes, int size,
	int timeout)
{
    int ret = -1;
    CAST_DEVICE(usb_interrupt_read,(handle,ep,bytes,size,timeout));
    libusbip_urb_transfer(dev,ep,USBIP_URB_TYPE_INTERRUPT,bytes,size,&ret,timeout,0);
    return ret;
}

int usb_os_find_busses(struct usb_bus **busses)
{
    libusbip_host_t *hosts = 0;
    struct usb_bus *bus = NULL;
    struct usb_bus *fbus = NULL;
    ssize_t i,ret;

    if(hook.func_usb_os_find_busses) {
        hook.func_usb_os_find_busses(&bus);
        if(bus) LIST_ADD(fbus,bus);
    } else if(hook.func_usb_find_busses &&
              hook.func_usb_find_devices &&
              hook.func_usb_get_busses)
    {
        hook.func_usb_find_busses();
        hook.func_usb_find_devices();
        bus = hook.func_usb_get_busses();
        while(bus) {
            struct usb_device *dev;
            struct usb_bus *newbus = (struct usb_bus*)calloc(1,sizeof(struct usb_bus));
            strncpy(newbus->dirname,bus->dirname,sizeof(newbus->dirname));
            newbus->location = bus->location;
            LIST_ADD(fbus,newbus);
            for(dev=bus->devices;dev;dev=dev->next) {
                /* FIXME: missing child device copy from external hook
                 */
                struct usb_device *newdev = (struct usb_device*)calloc(1,sizeof(struct usb_device));
                strncpy(newdev->filename,dev->filename,sizeof(newdev->filename));
                newdev->bus = newbus;
                newdev->dev = dev->dev;
                newdev->devnum = dev->devnum;
                newdev->descriptor = dev->descriptor;
                /* Too lazy to copy descriptor by hand. 
                 * Just let usb.c to fetch other descriptors */
                LIST_ADD(newbus->devices,newdev);
            }
            bus = bus->next;
        }
        if(hook.func_usb_find_devices)
            hook.func_usb_find_devices();
    }

    ret = libusbip_find_hosts(session,&hosts,5000,0);
    if(ret < 0) return -1;
    for(i=0;i<ret;++i) {
        bus = (struct usb_bus *)calloc(1,sizeof(struct usb_bus));
        bus->location = hosts[i].port;
        snprintf(bus->dirname,sizeof(bus->dirname),"/usbip/%s", hosts[i].addr);
        LIST_ADD(fbus,bus);
    }
    libusbip_free_hosts(session,hosts);
    *busses = fbus;
    return 0;
}

int usb_os_find_devices(struct usb_bus *bus, struct usb_device **devices)
{
    struct usb_device *fdev = NULL;
    struct usb_device *dev;
    char buf[512];
    libusbip_device_info_t *info = 0;
    libusbip_host_t host;
    ssize_t count=0,i;

    *devices = NULL;
    if(!is_usbip_bus(bus)) {
        if(hook.func_usb_os_find_devices)
            return hook.func_usb_os_find_devices(bus,devices);
        else if(hook.func_usb_find_devices){
            *devices = bus->devices;
            bus->devices = NULL;
            return 0;
        }
        return -1;
    }

    strncpy(host.addr,get_hostname(bus),sizeof(host.addr));
    host.port = bus->location;
    if(libusbip_get_devices_info(session,&host,&info,&count,5000,0))
        return -1;
    for(i=0;i<count;++i) {
        dev = (struct usb_device*)calloc(1,sizeof(struct usb_device));
        dev->bus = bus;
        strncpy(dev->filename,info[i].busid,sizeof(dev->filename));
        dev->descriptor.bDescriptorType = USB_DT_DEVICE;
        dev->descriptor.bcdUSB = info[i].bcdDevice;
        dev->descriptor.bDeviceClass = info[i].bDeviceClass;
        dev->descriptor.bDeviceSubClass = info[i].bDeviceSubClass;
        dev->descriptor.bDeviceProtocol = info[i].bDeviceProtocol;
        dev->descriptor.idVendor = info[i].idVendor;
        dev->descriptor.idProduct = info[i].idProduct;
        dev->descriptor.bNumConfigurations = info[i].bNumConfigurations;
        /* usbip can't get full configuration information when list
         * remote exports, so we don't fill the configuration descriptor,
         * and just let libusb handle that 
         */
        LIST_ADD(fdev,dev);
    }
    libusbip_free_devices_info(session,info,count);
    *devices = fdev;
    return 0;
}

int usb_os_determine_children(struct usb_bus *bus)
{
    if(!is_usbip_bus(bus)) {
        if(hook.func_usb_os_determine_children)
            return hook.func_usb_os_determine_children(bus);
        return 0;
    }

    /* usbip can't determin port info yet. 
     * In fact, usbip won't export hub/root device at all 
     */
    return 0;
}

void usb_os_init(void)
{
    hooklib();
    if(hook.func_usb_os_init) 
        hook.func_usb_os_init();
    libusbip_init(&session);
}

int usb_resetep(usb_dev_handle *handle, unsigned int ep)
{
    int ret = -1;
    CAST_DEVICE(usb_resetep,(handle,ep));
    libusbip_control_msg(dev,USB_RECIP_ENDPOINT, 
            USB_REQ_CLEAR_FEATURE, 0, ep, NULL, 0, &ret, 5000, 0);
    return ret;
}

int usb_clear_halt(usb_dev_handle *handle, unsigned int ep)
{
    int ret = -1;
    CAST_DEVICE(usb_clear_halt,(handle,ep));
    libusbip_control_msg(dev,USB_RECIP_ENDPOINT, 
            USB_REQ_CLEAR_FEATURE, 0, ep, NULL, 0, &ret, 5000, 0);
    return ret;
}

int usb_reset(usb_dev_handle *handle)
{
    int ret = -1;
    CAST_DEVICE(usb_reset,(handle));
    /* FIXME: only works on usb 2.0? */
    libusbip_control_msg(dev,USB_RECIP_OTHER|USB_TYPE_CLASS, 
            USB_REQ_SET_FEATURE, 4, 0, NULL, 0, &ret,5000, 0);
    return ret;
}

int usb_get_driver_np(usb_dev_handle *handle, int interface, char *name,
	unsigned int namelen)
{
    CAST_DEVICE(usb_get_driver_np,(handle,interface,name,namelen));
    USB_ERROR_STR(-ENOSYS, "could not get bound driver: %s", strerror(ENOSYS));
}

int usb_detach_kernel_driver_np(usb_dev_handle *handle, int interface)
{
    CAST_DEVICE(usb_detach_kernel_driver_np,(handle,interface));
    USB_ERROR_STR(-ENOSYS, "could not detach kernel driver from interface %d: %s",
        interface, strerror(ENOSYS));
}

void usb_set_debug(int level) {
    if(hook.func_usb_set_debug)
        hook.func_usb_set_debug(level);
    return usb_os_set_debug(level);
}

char *usb_strerror() {
    if(hook.func_usb_strerror)
        return hook.func_usb_strerror();
    return usb_os_strerror();
}
