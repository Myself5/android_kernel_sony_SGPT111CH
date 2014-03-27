/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * Copyright (C) 2010 NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/device.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/kbc.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#define NBX03_ROW_COUNT		3
#define NBX03_COL_COUNT		6

static int plain_kbd_keycode[] = {
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_VOLUMEUP, KEY_VOLUMEDOWN,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED,
	KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED,
		KEY_RESERVED, KEY_RESERVED,
};

static struct tegra_kbc_wake_key nbx03_wake_cfg[] = {
    /* none */
};


static struct tegra_kbc_platform_data nbx03_kbc_platform_data = {
	.debounce_cnt = 20,
	.repeat_cnt = 50 * 32,
	.scan_timeout_cnt = 3000 * 32,
	.plain_keycode = plain_kbd_keycode,
	.fn_keycode = NULL,
	.is_filter_keys = false,
	.is_wake_on_any_key = false,
	.wake_key_cnt = ARRAY_SIZE(nbx03_wake_cfg),
	.wake_cfg = &nbx03_wake_cfg[0],
};


static struct resource nbx03_kbc_resources[] = {
	[0] = {
		.start = TEGRA_KBC_BASE,
		.end   = TEGRA_KBC_BASE + TEGRA_KBC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_KBC,
		.end   = INT_KBC,
		.flags = IORESOURCE_IRQ,
	},
};


struct platform_device nbx03_kbc_device = {
	.name = "tegra-kbc",
	.id = -1,
	.dev = {
		.platform_data = &nbx03_kbc_platform_data,
	},
	.resource = nbx03_kbc_resources,
	.num_resources = ARRAY_SIZE(nbx03_kbc_resources),
};

int __init nbx03_kbc_init(void)
{
	struct tegra_kbc_platform_data *data = &nbx03_kbc_platform_data;
	int i;

	pr_info("KBC: nbx03_kbc_init\n");

	/*
	 * Setup the pin configuration information.
	 */
	for (i = 0; i < KBC_MAX_GPIO; i++) {
		data->pin_cfg[i].num = 0;
		data->pin_cfg[i].pin_type = kbc_pin_unused;
	}
	for (i = 0; i < NBX03_ROW_COUNT; i++) {
		data->pin_cfg[i].num = i;
		data->pin_cfg[i].pin_type = kbc_pin_row;
	}
	for (i = 0; i < NBX03_COL_COUNT; i++) {
		data->pin_cfg[i + NBX03_ROW_COUNT].num = i;
		data->pin_cfg[i + NBX03_ROW_COUNT].pin_type = kbc_pin_col;
	}

	platform_device_register(&nbx03_kbc_device);
	return 0;
}



