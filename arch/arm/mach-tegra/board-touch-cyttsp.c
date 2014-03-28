/* 2012-07-20: File added and changed by Sony Corporation */
/*
 * arch/arm/mach-tegra/board-touch-cyttsp.c
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

#define VERBOSE_DEBUG	1	/* When set, emit verbose debug messages.	*/
#define MULTI_SKU	1	/* When set, multi-driver detection is enabled.	*/
				/* When clear, DEFAULT_SKU is used.		*/

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include "board.h"
#if defined(CONFIG_MACH_VENTANA)
#include "board-ventana.h"
#elif defined(CONFIG_MACH_TXS03)
#include "board-txs03.h"
#elif defined(CONFIG_MACH_TEGRA_ENTERPRISE)
#include "board-enterprise.h"
#endif
#include "gpio-names.h"
//#include "touch.h"

#define SKU_MASK	0xFF00	/* Mask BoardInfo before testing sku.	*/
#define UNKNOWN_SKU	0xFF00	/* When BoardInfo not programmed...	*/

#include <linux/input/cyttsp.h>


/* default bootloader keys */
u8 dflt_bl_keys[] = {
	0, 1, 2, 3, 4, 5, 6, 7
};
enum cyttsp_gest {
	CY_GEST_GRP_NONE = 0x0F,
	CY_GEST_GRP1 =	0x10,
	CY_GEST_GRP2 = 0x20,
	CY_GEST_GRP3 = 0x40,
	CY_GEST_GRP4 = 0x80,
};

static int cyttsp_wakeup(void)
{
	int ret;
#if 0
	ret = gpio_direction_output(TEGRA_GPIO_PN0, 1);
	if (ret) {
		printk(KERN_ERR "%s: Failed to request gpio_direction_output\n",
		__func__);
                return ret;
	}
	gpio_set_value(TEGRA_GPIO_PN0, 1);
#endif
	atmel_gpio_power_control(1);
	msleep(300);
	return 0;
}

static int cyttsp_poweroff(void)
{
	int ret;
	atmel_gpio_power_control(0);
#if 0
	ret = gpio_direction_output(TEGRA_GPIO_PN0, 0);
	if (ret) {
		printk(KERN_ERR "%s: Failed to request gpio_direction_output\n",
		__func__);
                return ret;
	}        
	gpio_set_value(TEGRA_GPIO_PN0, 0);
#endif
	/*msleep(3); */
	return 0;
}

static int cyttsp_init(int on)
{
	int ret;

	if (on) {
#if 1                
		ret = gpio_request(TEGRA_GPIO_PK2, "CYTTSP IRQ GPIO");
		if (ret) {
			printk(KERN_ERR "%s: Failed to request GPIO %d\n",
			       __func__, TEGRA_GPIO_PK2);
			return ret;
		}
#endif
		tegra_gpio_enable(TEGRA_GPIO_PK2);
		gpio_direction_input(TEGRA_GPIO_PK2);

#if 0
        	ret = gpio_request(TEGRA_GPIO_PN0, "CYTTSP POWER GPIO");
        	if (ret) {
        		printk(KERN_ERR "%s: Failed to request GPIO %d\n",
        		       __func__, TEGRA_GPIO_PN0);
        		return ret;
        	}
		tegra_gpio_enable(TEGRA_GPIO_PN0);
		gpio_direction_output(TEGRA_GPIO_PN0, 1);
#endif

	} else {
		gpio_free(TEGRA_GPIO_PK2);
		gpio_free(TEGRA_GPIO_PN0);
	}
	return 0;
}


static struct cyttsp_platform_data cypress_i2c_touch_data = {
	.wakeup = cyttsp_wakeup,
	.poweroff = cyttsp_poweroff,
	.init = cyttsp_init,
//	.mt_sync = input_mt_sync,
	/* TODO: maxx and maxy values should be retrieved from the firmware */
	.maxx = 1279,
	.maxy = 799,
	.flags = 0x04,
//	.gen = CY_GEN4,
//	.use_st = 0,
//	.use_mt = 1,
//	.use_trk_id = 0,
	.use_hndshk = 0,
//	.use_timer = 0,
	.use_sleep = 1,
//	.use_gestures = 0,
//	.use_load_file = 0,
//	.use_force_fw_update = 0,
//	.use_virtual_keys = 0,
	/* activate up to 4 groups
	 * and set active distance
	 */
//	.gest_set = CY_GEST_GRP_NONE | CY_ACT_DIST,
	/* change act_intrvl to customize the Active power state
	 * scanning/processing refresh interval for Operating mode
	 */
	/* activate up to 4 groups
	 * and set active distance
	 */
	.act_dist = CY_GEST_GRP_NONE & CY_ACT_DIST_DFLT,
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
	.name = CY_I2C_NAME,
	.irq_gpio = TEGRA_GPIO_PK2,
	.bl_keys = dflt_bl_keys,
};

static const struct i2c_board_info nbx03_i2c_bus1_touch_info[] = {
	{
		I2C_BOARD_INFO(CY_I2C_NAME, 0x67),
		.platform_data = &cypress_i2c_touch_data,
	},
};

int __init cyttsp_touch_init(void)
{
	i2c_register_board_info(1, nbx03_i2c_bus1_touch_info, ARRAY_SIZE(nbx03_i2c_bus1_touch_info));

	return 0;
}
