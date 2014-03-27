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

#ifndef __NBX_LIGHTSENSOR_H__
#define __NBX_LIGHTSENSOR_H__

#include <linux/types.h>
#include <linux/ioctl.h>
#ifdef __cplusplus
extern "C" {
#endif

#define NBX_LIGHTSENSOR_IOC_MAGIC 'l'
#define NBX_LIGHTSENSOR_IOC_GET_DELAY	_IOR(NBX_LIGHTSENSOR_IOC_MAGIC, 0, __s32)
#define NBX_LIGHTSENSOR_IOC_SET_DELAY	_IOW(NBX_LIGHTSENSOR_IOC_MAGIC, 1, __s32)
#define NBX_LIGHTSENSOR_IOC_GET_ACTIVE	_IOR(NBX_LIGHTSENSOR_IOC_MAGIC, 2, __s32)
#define NBX_LIGHTSENSOR_IOC_SET_ACTIVE	_IOW(NBX_LIGHTSENSOR_IOC_MAGIC, 3, __s32)
#define NBX_LIGHTSENSOR_IOC_MAXNR ( 4 )

#ifdef __cplusplus
}
#endif

#endif /* __NBX_LIGHTSENSOR_H__ */

