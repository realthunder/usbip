#ifndef LIBUSBIP_H
#define LIBUSBIP_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
# if defined(LIBUSBIP_EXPORT)
#   define LIBUSBIP_EXTERN __declspec(dllexport)
# elif defined(LIBUSBIP_IMPORT)
#   define LIBUSBIP_EXTERN __declspec(dllimport)
# else
#   define LIBUSBIP_EXTERN
# endif
#elif __GNUC__ >= 4
# define LIBUSBIP_EXTERN __attribute__((visibility("default")))
#else
# define LIBUSBIP_EXTERN
#endif

typedef struct libusbip_session_s libusbip_session_t;
typedef struct libusbip_device_s libusbip_device_t;
typedef struct libusbip_work_s libusbip_work_t;
typedef void (*libusbip_callback_t)(void *data, int ret);

typedef struct libusbip_job_s {
    libusbip_callback_t cb;
    void *data;
    libusbip_work_t *work;
}libusbip_job_t;

typedef struct libusbip_host_s {
    char addr[64];
    int port;
}libusbip_host_t;

typedef struct libusbip_device_interface_s {
	unsigned char bInterfaceClass;
	unsigned char bInterfaceSubClass;
	unsigned char bInterfaceProtocol;
}libusbip_device_interface_t;

typedef struct libusbip_device_info_s {
    char busid[32];

	unsigned int busnum;
	unsigned int devnum;
	unsigned int speed;

	unsigned short idVendor;
	unsigned short idProduct;
	unsigned short bcdDevice;

	unsigned char bDeviceClass;
	unsigned char bDeviceSubClass;
	unsigned char bDeviceProtocol;
	unsigned char bConfigurationValue;
	unsigned char bNumConfigurations;
	unsigned char bNumInterfaces;

    libusbip_device_interface_t *interfaces;
}libusbip_device_info_t;

LIBUSBIP_EXTERN int libusbip_init(libusbip_session_t **session);
LIBUSBIP_EXTERN void libusbip_exit(libusbip_session_t *session);

LIBUSBIP_EXTERN void libusbip_add_hosts(libusbip_session_t *session, 
        libusbip_host_t *hosts, unsigned count);
LIBUSBIP_EXTERN int libusbip_remove_hosts(libusbip_session_t *session, 
        libusbip_host_t *hosts, unsigned count);
LIBUSBIP_EXTERN int libusbip_find_hosts(libusbip_session_t *session,
        libusbip_host_t **hosts, int timeout, libusbip_job_t *job);
LIBUSBIP_EXTERN void libusbip_free_hosts(libusbip_session_t *session, 
        libusbip_host_t *hosts);
LIBUSBIP_EXTERN int libusbip_get_devices_info(libusbip_session_t *session,
        libusbip_host_t *host, libusbip_device_info_t **info, unsigned *count,
        int timeout, libusbip_job_t *job);
LIBUSBIP_EXTERN void libusbip_free_devices_info(libusbip_session_t *session,
        libusbip_device_info_t *info, unsigned count);

LIBUSBIP_EXTERN int libusbip_device_open(libusbip_session_t *session,
        libusbip_device_t **dev, const char *addr, int port, 
        const char *devid, int timeout, libusbip_job_t *job);
LIBUSBIP_EXTERN libusbip_device_t *libusbip_device_cast(
        libusbip_session_t *session, void *pt);

/* work abort is async */
LIBUSBIP_EXTERN int libusbip_work_abort(libusbip_work_t *work);

/* work close is async, just decrease the reference count internally.
 * You must not free any work related resource until your callback is invoked */ 
LIBUSBIP_EXTERN void libusbip_work_close(libusbip_work_t *work);

#define USBIP_URB_TYPE_ISO	0
#define USBIP_URB_TYPE_INTERRUPT	1
#define USBIP_URB_TYPE_CONTROL	2
#define USBIP_URB_TYPE_BULK	3
LIBUSBIP_EXTERN int libusbip_urb_transfer(
        libusbip_device_t *dev, int ep, int urbtype,
	    void *data, int size, int *ret, int timeout, libusbip_job_t *job);

LIBUSBIP_EXTERN int libusbip_control_msg(
        libusbip_device_t *dev, int requesttype, int request,
	    int value, int index, char *bytes, int size, int *ret,
        int timeout, libusbip_job_t *job);

LIBUSBIP_EXTERN void libusbip_device_close(libusbip_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif
