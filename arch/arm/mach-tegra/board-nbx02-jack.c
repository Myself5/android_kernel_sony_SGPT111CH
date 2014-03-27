/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * arch/arm/mach-tegra/board-nbx02-jack.c
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/platform_device.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <mach/gpio.h>
#include <mach/audio.h>
#include <mach/a1026.h>

#include "gpio-names.h"
#include "board-nbx02.h"

static struct tegra_wired_jack_conf nbx02_wr_jack_conf = {
	.hp_det_n = TEGRA_GPIO_PW2,
        .cdc_irq = TEGRA_GPIO_PK6,
        .cdc_shrt = TEGRA_GPIO_PU5,
        .en_spkr = WM8903_GP3,
	.echo_canceler_sleep = a1026_suspend_command,
	.echo_canceler_wake = a1026_resume_command,
};

static struct platform_device nbx02_hs_jack_device = {
	.name = "tegra_wired_jack",
	.id = -1,
	.dev = {
		.platform_data = &nbx02_wr_jack_conf,
	},
};

int __init nbx02_wired_jack_init(void)
{
	int ret;

	tegra_gpio_enable(nbx02_wr_jack_conf.hp_det_n);
        tegra_gpio_enable(nbx02_wr_jack_conf.cdc_irq);
        tegra_gpio_enable(nbx02_wr_jack_conf.cdc_shrt);

	ret = platform_device_register(&nbx02_hs_jack_device);
	return ret;
}
