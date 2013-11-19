#include "../../usbip_struct_helper.h"
USBIP_STRUCT_BEGIN(usbip_usb_interface)
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bInterfaceClass);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bInterfaceSubClass);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bInterfaceProtocol);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t padding);
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(usbip_usb_device)
    /*FIXME: original header uses SYSFS_PATH_MAX, which is not portable.
     * Is it safe to just use its current value 256? */
	USBIP_STRUCT_MEMBER_BYPASS(char path[256]);

    /* FIXME: original size is SYSFS_BUS_ID_SIZE */
	USBIP_STRUCT_MEMBER_BYPASS(char busid[32]);

	USBIP_STRUCT_MEMBER_U32(busnum);
	USBIP_STRUCT_MEMBER_U32(devnum);
	USBIP_STRUCT_MEMBER_U32(speed);

	USBIP_STRUCT_MEMBER_U16(idVendor);
	USBIP_STRUCT_MEMBER_U16(idProduct);
	USBIP_STRUCT_MEMBER_U16(bcdDevice);

	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bDeviceClass);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bDeviceSubClass);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bDeviceProtocol);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bConfigurationValue);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bNumConfigurations);
	USBIP_STRUCT_MEMBER_BYPASS(uint8_t bNumInterfaces);
USBIP_STRUCT_END

#ifndef USBIP_PORT
#   define USBIP_PORT 3240
#   define USBIP_PORT_STRING "3240"
#endif

/* ---------------------------------------------------------------------- */
/* Common header for all the kinds of PDUs. */
USBIP_STRUCT_BEGIN(op_common)

    USBIP_STRUCT_MEMBER_U16(version);

#ifndef OP_REQUEST
#   define OP_REQUEST	(0x80 << 8)
#   define OP_REPLY	(0x00 << 8)
#endif
    USBIP_STRUCT_MEMBER_U16(code);

	/* add more error code */
#ifndef ST_OK
#   define ST_OK	0x00
#   define ST_NA	0x01
#endif
	USBIP_STRUCT_MEMBER_U32(status); /* op_code status (for reply) */
USBIP_STRUCT_END

/* ---------------------------------------------------------------------- */
/* Dummy Code */
#ifndef OP_UNSEPC
#   define OP_UNSPEC	0x00
#   define OP_REQ_UNSPEC	OP_UNSPEC
#   define OP_REP_UNSPEC	OP_UNSPEC
#endif

/* ---------------------------------------------------------------------- */
/* Retrieve USB device information. (still not used) */
#ifndef OP_DEVINFO
#   define OP_DEVINFO	0x02
#   define OP_REQ_DEVINFO	(OP_REQUEST | OP_DEVINFO)
#   define OP_REP_DEVINFO	(OP_REPLY   | OP_DEVINFO)
#endif

USBIP_STRUCT_BEGIN(op_devinfo_request)
    /* FIXME: original size is SYSFS_BUS_ID_SIZE */
    USBIP_STRUCT_MEMBER_BYPASS(char busid[32]);
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(op_devinfo_reply)
    USBIP_STRUCT_MEMBER_STRUCT(usbip_usb_device,udev);
    USBIP_STRUCT_MEMBER_BYPASS(struct usbip_usb_interface uinf[]);
USBIP_STRUCT_END


/* ---------------------------------------------------------------------- */
/* Import a remote USB device. */
#ifndef OP_IMPORT
#   define OP_IMPORT	0x03
#   define OP_REQ_IMPORT	(OP_REQUEST | OP_IMPORT)
#   define OP_REP_IMPORT   (OP_REPLY   | OP_IMPORT)
#endif

USBIP_STRUCT_BEGIN(op_import_request)
    /* FIXME: original size is SYSFS_BUS_ID_SIZE */
    USBIP_STRUCT_MEMBER_BYPASS(char busid[32]);
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(op_import_reply)
	USBIP_STRUCT_MEMBER_STRUCT(usbip_usb_device,udev);
USBIP_STRUCT_END

/* ---------------------------------------------------------------------- */
/* Export a USB device to a remote host. */
#ifndef OP_EXPORT
#   define OP_EXPORT	0x06
#   define OP_REQ_EXPORT	(OP_REQUEST | OP_EXPORT)
#   define OP_REP_EXPORT	(OP_REPLY   | OP_EXPORT)
#endif

USBIP_STRUCT_BEGIN(op_export_request)
	USBIP_STRUCT_MEMBER_STRUCT(usbip_usb_device,udev);
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(op_export_reply)
    USBIP_STRUCT_MEMBER_S32(returncode);
USBIP_STRUCT_END

/* ---------------------------------------------------------------------- */
/* un-Export a USB device from a remote host. */
#ifndef OP_UNEXPORT
#   define OP_UNEXPORT	0x07
#   define OP_REQ_UNEXPORT	(OP_REQUEST | OP_UNEXPORT)
#   define OP_REP_UNEXPORT	(OP_REPLY   | OP_UNEXPORT)
#endif

USBIP_STRUCT_BEGIN(op_unexport_request)
	USBIP_STRUCT_MEMBER_STRUCT(usbip_usb_device,udev);
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(op_unexport_reply)
    USBIP_STRUCT_MEMBER_S32(returncode);
USBIP_STRUCT_END

/* ---------------------------------------------------------------------- */
/* Negotiate IPSec encryption key. (still not used) */
#ifndef OP_CRYPKEY	
#   define OP_CRYPKEY	0x04
#   define OP_REQ_CRYPKEY	(OP_REQUEST | OP_CRYPKEY)
#   define OP_REP_CRYPKEY	(OP_REPLY   | OP_CRYPKEY)
#endif

USBIP_STRUCT_BEGIN(op_crypkey_request)
	/* 128bit key */
    USBIP_STRUCT_MEMBER_ARRAY(U32,key,4);
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(op_crypkey_reply)
    USBIP_STRUCT_MEMBER_U32(__reserved);
USBIP_STRUCT_END

/* ---------------------------------------------------------------------- */
/* Retrieve the list of exported USB devices. */
#ifndef OP_DEVLIST
#   define OP_DEVLIST	0x05
#   define OP_REQ_DEVLIST	(OP_REQUEST | OP_DEVLIST)
#   define OP_REP_DEVLIST	(OP_REPLY   | OP_DEVLIST)
#endif

USBIP_STRUCT_BEGIN(op_devlist_request)
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(op_devlist_reply)
	USBIP_STRUCT_MEMBER_U32(ndev);
	/* followed by reply_extra[] */
USBIP_STRUCT_END

USBIP_STRUCT_BEGIN(op_devlist_reply_extra)
    USBIP_STRUCT_MEMBER_STRUCT(usbip_usb_device,udev);
    USBIP_STRUCT_MEMBER_BYPASS(struct usbip_usb_interface uinf[]);
USBIP_STRUCT_END

