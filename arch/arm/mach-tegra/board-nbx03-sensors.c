/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * arch/arm/mach-tegra/board-nbx03-sensors.c
 *
 * Copyright (c) 2010, NVIDIA, All Rights Reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/i2c.h>
#include <linux/nct1008.h>

#include <mach/gpio.h>

#include <generated/mach-types.h>

#include "gpio-names.h"
#include "board.h"
#include "board-nbx03.h"

#include <media/ius006.h>
#include <media/cmv59dx.h>
#include <linux/delay.h>
#include <linux/time.h>

#define NCT1008_THERM2_GPIO	TEGRA_GPIO_PN5

#define GPIO_PIN_5M_28V         TEGRA_GPIO_PL0
#define GPIO_PIN_5M_18V         TEGRA_GPIO_PL1
#define GPIO_PIN_5M_12V         TEGRA_GPIO_PL2
#define GPIO_PIN_5M_RST         TEGRA_GPIO_PL3
#define GPIO_PIN_5M_PWRDWN      TEGRA_GPIO_PL4
#define GPIO_PIN_5M_28V_VCM     TEGRA_GPIO_PL5
#define GPIO_PIN_VGA_EN         TEGRA_GPIO_PL6
#define GPIO_PIN_VGA_RST        TEGRA_GPIO_PL7

extern void tegra_throttling_enable(bool enable);

static int nbx03_camera_init(void)
{
	tegra_gpio_enable(GPIO_PIN_5M_28V);
	tegra_gpio_enable(GPIO_PIN_5M_18V);
	tegra_gpio_enable(GPIO_PIN_5M_12V);
	tegra_gpio_enable(GPIO_PIN_5M_RST);
	tegra_gpio_enable(GPIO_PIN_5M_PWRDWN);
	tegra_gpio_enable(GPIO_PIN_5M_28V_VCM);
	tegra_gpio_enable(GPIO_PIN_VGA_EN);
	tegra_gpio_enable(GPIO_PIN_VGA_RST);

    gpio_request(GPIO_PIN_5M_28V, "GPIO_PIN_5M_28V");
    gpio_request(GPIO_PIN_5M_18V, "GPIO_PIN_5M_18V");
    gpio_request(GPIO_PIN_5M_12V, "GPIO_PIN_5M_12V");
    gpio_request(GPIO_PIN_5M_RST, "GPIO_PIN_5M_RST");
    gpio_request(GPIO_PIN_5M_PWRDWN, "GPIO_PIN_5M_PWRDWN");
    gpio_request(GPIO_PIN_5M_28V_VCM, "GPIO_PIN_5M_28V_VCM");
    gpio_request(GPIO_PIN_VGA_EN, "GPIO_PIN_VGA_EN");
    gpio_request(GPIO_PIN_VGA_RST, "GPIO_PIN_VGA_RST");

    gpio_direction_output(GPIO_PIN_5M_28V, 0);
    gpio_direction_output(GPIO_PIN_5M_18V, 0);
    gpio_direction_output(GPIO_PIN_5M_12V, 0);
    gpio_direction_output(GPIO_PIN_5M_RST, 0);
    gpio_direction_output(GPIO_PIN_5M_PWRDWN, 0);
    gpio_direction_output(GPIO_PIN_5M_28V_VCM, 0);
    gpio_direction_output(GPIO_PIN_VGA_EN, 0);
    gpio_direction_output(GPIO_PIN_VGA_RST, 0);

	return 0;
}

static void nbx03_nct1008_init(void)
{
	tegra_gpio_enable(NCT1008_THERM2_GPIO);
	gpio_request(NCT1008_THERM2_GPIO, "temp_alert");
	gpio_direction_input(NCT1008_THERM2_GPIO);
}

static struct nct1008_platform_data nbx03_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = false,
	.conv_rate = 0x08,
	.offset = 0,
	.hysteresis = 0,
	.shutdown_ext_limit = 115,
	.shutdown_local_limit = 80,
	.throttling_ext_limit = 75,
	.throttling_local_limit = 65,
	.alarm_fn = tegra_throttling_enable,
};

static struct i2c_board_info nbx03_i2c4_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.platform_data = &nbx03_nct1008_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(NCT1008_THERM2_GPIO),
	},
};

static struct i2c_board_info nbx03_i2c_sensors_info[] = {
#ifdef CONFIG_INPUT_NBX_ACCELEROMETER
	{
		I2C_BOARD_INFO("nbx_accelerometer", 0x18), /* LIS331DLH */
	},
#endif
#ifdef CONFIG_INPUT_NBX_MAGNETOMETER
	{
		I2C_BOARD_INFO("nbx_magnetometer", 0x0f), /* AMI304 */
	},
#endif
#ifdef CONFIG_INPUT_NBX_GYROSCOPE
	{
		I2C_BOARD_INFO("nbx_gyroscope", 0x68), /* L3G4200DH */
	},
#endif
};

static struct i2c_board_info nbx03_camera_board_info[] = {
	{
		I2C_BOARD_INFO("ius006", 0x3D),
	},
	{
		I2C_BOARD_INFO("cmv59dx", 0x3C),
	}
};

int __init nbx03_sensors_init(void)
{
	struct board_info BoardInfo;

	nbx03_camera_init();
	nbx03_nct1008_init();

	tegra_get_board_info(&BoardInfo);

	i2c_register_board_info(3, nbx03_i2c4_board_info,
		ARRAY_SIZE(nbx03_i2c4_board_info));

#if defined(CONFIG_INPUT_NBX_ACCELEROMETER) || defined(CONFIG_INPUT_NBX_MAGNETOMETER) || defined(CONFIG_INPUT_NBX_GYROSCOPE)
	i2c_register_board_info(0, nbx03_i2c_sensors_info,
		ARRAY_SIZE(nbx03_i2c_sensors_info));
#endif

	i2c_register_board_info(2, nbx03_camera_board_info,
			ARRAY_SIZE(nbx03_camera_board_info));

	return 0;
}