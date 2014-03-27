/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * Copyright (C) 2011 Sony Corporation
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __NBX_EC_IPC_H_INCLUDED__
#define __NBX_EC_IPC_H_INCLUDED__

#include <linux/types.h>

/* send request packet and receive response packet, without timeout version */
#define ec_ipc_send_request(pid, cid, buf, size, response_buf, response_buf_size) \
	ec_ipc_send_request_timeout((pid), (cid), (buf), (size), (response_buf), (response_buf_size), 0)

/*
 *  send request packet and receive response packet.
 *
 *  arg : "pid",               request packet PID.
 *        "cid",               request packet CID.
 *        "buf",               request packet PAYLOAD data.
 *        "size",              request packet PAYLOAD size.
 *        "response_buf",      response packet PAYLOAD data store area.
 *        "response_buf_size", response packet PAYLOAD data store area size limit.
 *        "timeout_ms",        time limit of receive response packet, specifies ms.
 *
 *  ret : received request packet PAYLOAD data size.
 *        if less than 0, error occur.
 */
ssize_t ec_ipc_send_request_timeout(uint8_t pid, uint8_t cid, const uint8_t* buf, int size,
				uint8_t* response_buf, int response_buf_size, int timeout_ms);

/*
 *  register/unregister event packet received callback.
 *
 *  arg : "cid",        callback when same COMID received.
 *        "event_func", address of callback function.
 *          arg : "buf",  event packet PAYLOAD data.
 *                "size", event packet PAYLOAD size.
 *
 *  ret : if non 0, error occur.
 */
int ec_ipc_register_recv_event(uint8_t cid, void (*event_func)(const uint8_t* buf, int size));
int ec_ipc_unregister_recv_event(uint8_t cid);

/*
 *  send request packet async
 *
 *  arg : "pid",              request packet PID.
 *        "cid",              request packet CID.
 *        "buf",              request packet PAYLOAD data.
 *        "size",             request packet PAYLOAD size.
 *        "res_func",         address of callback when response packet received.
 *          arg : "buf",          response packet PAYLOAD data.
 *                "size",         response packet PAYLOAD size.
 *                "private_data", same data as send request.
 *        "private_data", same data in callback argument.
 *
 *        if "res_func" is NULL, send to unnecessary response version.
 *
 *  ret : allocated frame number. use this when cancel.
 *        if less than 0, error occur.
 */
int ec_ipc_send_request_async(uint8_t pid, uint8_t cid, const uint8_t* buf, int size,
			void (*res_func)(const uint8_t* buf, int size, void* private_data),
			void* private_data);

/*
 *  cancel request packet
 *
 *  arg : "pid",        requested packet PID.
 *        "cid",        requested packet CID.
 *        "frame_num",  requested packet FRAME_NUM.
 *                        returned by ec_ipc_send_request_async().
 */
void ec_ipc_cancel_request(uint8_t pid, uint8_t cid, uint8_t frame_num);

/* oneway version */
#define ec_ipc_send_request_oneway(pid, cid, buf, size)			\
	ec_ipc_send_request_async((pid), (cid), (buf), (size), NULL, NULL)


#define EC_IPC_PID_LED          1
#define EC_IPC_PID_LIGHTSENSOR  2
#define EC_IPC_PID_IR           3
#define EC_IPC_PID_LID          4
#define EC_IPC_PID_BATTERY      5
#define EC_IPC_PID_SYSFS        6
#define EC_IPC_PID_POWERSTATE   7
#define EC_IPC_PID_TOPCOVER     8
#define EC_IPC_PID_POWERKEY     9

#endif /* __NBX_EC_IPC_H_INCLUDED__ */
