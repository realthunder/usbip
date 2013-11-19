/*
 * Copyright (C) 2005-2007 Takahiro Hirofuchi
 */

#ifndef __USBIP_NETWORK_H
#define __USBIP_NETWORK_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <sys/types.h>
#include <sysfs/libsysfs.h>

#include <stdint.h>

#define PACK_OP_COMMON(pack, op_common)  do {\
	usbip_net_pack_uint16_t(pack, &(op_common)->version);\
	usbip_net_pack_uint16_t(pack, &(op_common)->code);\
	usbip_net_pack_uint32_t(pack, &(op_common)->status);\
} while (0)

#define PACK_OP_IMPORT_REQUEST(pack, request)  do {\
} while (0)

#define PACK_OP_IMPORT_REPLY(pack, reply)  do {\
	usbip_net_pack_usb_device(pack, &(reply)->udev);\
} while (0)


#define PACK_OP_EXPORT_REQUEST(pack, request)  do {\
	usbip_net_pack_usb_device(pack, &(request)->udev);\
} while (0)

#define PACK_OP_EXPORT_REPLY(pack, reply)  do {\
} while (0)


#define PACK_OP_UNEXPORT_REQUEST(pack, request)  do {\
	usbip_net_pack_usb_device(pack, &(request)->udev);\
} while (0)

#define PACK_OP_UNEXPORT_REPLY(pack, reply)  do {\
} while (0)


#define PACK_OP_DEVLIST_REQUEST(pack, request)  do {\
} while (0)

#define PACK_OP_DEVLIST_REPLY(pack, reply)  do {\
	usbip_net_pack_uint32_t(pack, &(reply)->ndev);\
} while (0)

void usbip_net_pack_uint32_t(int pack, uint32_t *num);
void usbip_net_pack_uint16_t(int pack, uint16_t *num);
void usbip_net_pack_usb_device(int pack, struct usbip_usb_device *udev);
void usbip_net_pack_usb_interface(int pack, struct usbip_usb_interface *uinf);

ssize_t usbip_net_recv(int sockfd, void *buff, size_t bufflen);
ssize_t usbip_net_send(int sockfd, void *buff, size_t bufflen);
int usbip_net_send_op_common(int sockfd, uint32_t code, uint32_t status);
int usbip_net_recv_op_common(int sockfd, uint16_t *code);
int usbip_net_set_reuseaddr(int sockfd);
int usbip_net_set_nodelay(int sockfd);
int usbip_net_set_keepalive(int sockfd);
int usbip_net_tcp_connect(char *hostname, char *port);

#endif /* __USBIP_NETWORK_H */
