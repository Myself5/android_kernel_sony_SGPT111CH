/* 2011-06-10: File added by Sony Corporation */
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
/*
 * nbx02 backlight driver data - see drivers/video/backlight/nbx02_bl.c
 */
#ifndef __LINUX_NBX02_BACKLIGHT_H
#define __LINUX_NBX02_BACKLIGHT_H

struct platform_nbx02_backlight_data {
	struct platform_pwm_backlight_data pwm[2];
};

#endif
