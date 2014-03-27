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
#include <linux/kernel.h>
#include <linux/init.h>

#include "board-nbx-common.h"

static u32 s_odmdata;

static int __init nbx_odmdata_arg(char *options)
{
	char *p = options;

	s_odmdata = (u32)simple_strtoul(p, NULL, 0);

	pr_info("odmdata=%08x\n", s_odmdata);

	return 0;
}
early_param("odmdata", nbx_odmdata_arg);

u32 nbx_get_odmdata()
{
	return s_odmdata;
}
