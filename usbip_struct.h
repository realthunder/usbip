#include "usbip_struct_helper.h"

#define USBIP_VERSION_STRING "1.0.0"
#define USBIP_VERSION_NUM 0x111

/*
 * USB/IP request headers
 *
 * Each request is transferred across the network to its counterpart, which
 * facilitates the normal USB communication. The values contained in the headers
 * are basically the same as in a URB. Currently, four request types are
 * defined:
 *
 *  - USBIP_CMD_SUBMIT: a USB request block, corresponds to usb_submit_urb()
 *    (client to server)
 *
 *  - USBIP_RET_SUBMIT: the result of USBIP_CMD_SUBMIT
 *    (server to client)
 *
 *  - USBIP_CMD_UNLINK: an unlink request of a pending USBIP_CMD_SUBMIT,
 *    corresponds to usb_unlink_urb()
 *    (client to server)
 *
 *  - USBIP_RET_UNLINK: the result of USBIP_CMD_UNLINK
 *    (server to client)
 *
 */
#ifndef USBIP_CMD_SUBMIT
#   define USBIP_CMD_SUBMIT	0x0001
#   define USBIP_CMD_UNLINK	0x0002
#   define USBIP_RET_SUBMIT	0x0003
#   define USBIP_RET_UNLINK	0x0004

#   define USBIP_DIR_OUT	0x00
#   define USBIP_DIR_IN	0x01
#endif

/*
 * This is the same as usb_iso_packet_descriptor but packed for pdu.
 */
USBIP_STRUCT_BEGIN(usbip_iso_packet_descriptor)
    USBIP_STRUCT_MEMBER_U32(offset);
    USBIP_STRUCT_MEMBER_U32(length); /*expected length*/
    USBIP_STRUCT_MEMBER_U32(actual_length);
    USBIP_STRUCT_MEMBER_U32(status);
USBIP_STRUCT_END

/**
 * struct usbip_header_basic - data pertinent to every request
 * @command: the usbip request type
 * @seqnum: sequential number that identifies requests; incremented per
 *	    connection
 * @devid: specifies a remote USB device uniquely instead of busnum and devnum;
 *	   in the stub driver, this value is ((busnum << 16) | devnum)
 * @direction: direction of the transfer
 * @ep: endpoint number
 */
USBIP_STRUCT_BEGIN(usbip_header_basic)
	USBIP_STRUCT_MEMBER_U32(command);
    USBIP_STRUCT_MEMBER_U32(seqnum);
    USBIP_STRUCT_MEMBER_U32(devid);
    USBIP_STRUCT_MEMBER_U32(direction);
    USBIP_STRUCT_MEMBER_U32(ep);
USBIP_STRUCT_END

/**
 * struct usbip_header_cmd_submit - USBIP_CMD_SUBMIT packet header
 * @transfer_flags: URB flags
 * @transfer_buffer_length: the data size for (in) or (out) transfer
 * @start_frame: initial frame for isochronous or interrupt transfers
 * @number_of_packets: number of isochronous packets
 * @interval: maximum time for the request on the server-side host controller
 * @setup: setup data for a control request
 */
USBIP_STRUCT_BEGIN(usbip_header_cmd_submit)
    USBIP_STRUCT_MEMBER_U32(transfer_flags);
    USBIP_STRUCT_MEMBER_S32(transfer_buffer_length);

	/* it is difficult for usbip to sync frames (reserved only?) */
    USBIP_STRUCT_MEMBER_S32(start_frame);
    USBIP_STRUCT_MEMBER_S32(number_of_packets);
    USBIP_STRUCT_MEMBER_S32(interval);

    USBIP_STRUCT_MEMBER_BYPASS(unsigned char setup[8]);
USBIP_STRUCT_END

/**
 * struct usbip_header_ret_submit - USBIP_RET_SUBMIT packet header
 * @status: return status of a non-iso request
 * @actual_length: number of bytes transferred
 * @start_frame: initial frame for isochronous or interrupt transfers
 * @number_of_packets: number of isochronous packets
 * @error_count: number of errors for isochronous transfers
 */
USBIP_STRUCT_BEGIN(usbip_header_ret_submit)
    USBIP_STRUCT_MEMBER_S32(status);
    USBIP_STRUCT_MEMBER_S32(actual_length);
    USBIP_STRUCT_MEMBER_S32(start_frame);
    USBIP_STRUCT_MEMBER_S32(number_of_packets);
    USBIP_STRUCT_MEMBER_S32(error_count);
USBIP_STRUCT_END

/**
 * struct usbip_header_cmd_unlink - USBIP_CMD_UNLINK packet header
 * @seqnum: the URB seqnum to unlink
 */
USBIP_STRUCT_BEGIN(usbip_header_cmd_unlink)
    USBIP_STRUCT_MEMBER_U32(seqnum);
USBIP_STRUCT_END

/**
 * struct usbip_header_ret_unlink - USBIP_RET_UNLINK packet header
 * @status: return status of the request
 */
USBIP_STRUCT_BEGIN(usbip_header_ret_unlink)
    USBIP_STRUCT_MEMBER_S32(status);
USBIP_STRUCT_END

/**
 * struct usbip_header - common header for all usbip packets
 * @base: the basic header
 * @u: packet type dependent header
 */
USBIP_STRUCT_BEGIN(usbip_header)
	USBIP_STRUCT_MEMBER_STRUCT(usbip_header_basic,base);
    USBIP_STRUCT_MEMBER_BYPASS(
        union {
            struct usbip_header_cmd_submit	cmd_submit;
            struct usbip_header_ret_submit	ret_submit;
            struct usbip_header_cmd_unlink	cmd_unlink;
            struct usbip_header_ret_unlink	ret_unlink;
        } u);
USBIP_STRUCT_END
