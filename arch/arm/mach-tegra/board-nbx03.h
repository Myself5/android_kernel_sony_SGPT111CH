/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * arch/arm/mach-tegra/board-nbx03.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_BOARD_NBX03_H
#define _MACH_TEGRA_BOARD_NBX03_H

int nbx03_charge_init(void);
int nbx03_regulator_init(void);
int nbx03_sdhci_init(void);
int nbx03_pinmux_init(void);
int nbx03_panel_init(void);
int nbx03_sensors_init(void);
int nbx03_kbc_init(void);
int nbx03_emc_init(void);
int nbx03_wired_jack_init(void);

/* external gpios */

/* TPS6586X gpios */
#define TPS6586X_GPIO_BASE	TEGRA_NR_GPIOS
#define AVDD_DSI_CSI_ENB_GPIO	TPS6586X_GPIO_BASE + 1 /* gpio2 */

/* TCA6416 gpios */
#define TCA6416_GPIO_BASE	TEGRA_NR_GPIOS + 4
#define CAM2_PWR_DN_GPIO	TCA6416_GPIO_BASE + 4 /* gpio4 */
#define CAM2_RST_L_GPIO		TCA6416_GPIO_BASE + 5 /* gpio5 */
#define CAM2_AF_PWR_DN_L_GPIO	TCA6416_GPIO_BASE + 6 /* gpio6 */
#define CAM2_LDO_SHUTDN_L_GPIO	TCA6416_GPIO_BASE + 7 /* gpio7 */
#define CAM2_I2C_MUX_RST_GPIO	TCA6416_GPIO_BASE + 15 /* gpio15 */

/* WM8903 gpios */
#define WM8903_GPIO_BASE	(TEGRA_NR_GPIOS + 32)
#define WM8903_GP1		(WM8903_GPIO_BASE + 0)
#define WM8903_GP2		(WM8903_GPIO_BASE + 1)
#define WM8903_GP3		(WM8903_GPIO_BASE + 2)
#define WM8903_GP4		(WM8903_GPIO_BASE + 3)
#define WM8903_GP5		(WM8903_GPIO_BASE + 4)

/* Interrupt numbers from external peripherals */
#define TPS6586X_INT_BASE       TEGRA_NR_IRQS
#define TPS6586X_INT_END        (TPS6586X_INT_BASE + 32)

#endif
