/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * arch/arm/mach-tegra/board-nbx02.c
 *
 * Copyright (c) 2010 - 2011, NVIDIA Corporation.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/usb/android_composite.h>
#include <linux/mfd/tps6586x.h>
#include <linux/memblock.h>
#include <linux/nbx_ec_battery.h>

#ifdef CONFIG_TOUCHSCREEN_PANJIT_I2C
#include <linux/i2c/panjit_ts.h>
#endif

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MT_T9
#include <linux/i2c/atmel_maxtouch.h>
#endif

#include <sound/wm8903.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/spdif.h>
#include <mach/audio.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#include <mach/tegra_das.h>
#include <mach/a1026.h>
#include <mach/tegra2_fuse.h>

#include "board.h"
#include "clock.h"
#include "board-nbx02.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "board-nbx-common.h"
#include "wireless_power_control.h"


static struct plat_serial8250_port debug_uart_platform_data[] = {
	{
		.membase	= IO_ADDRESS(TEGRA_UARTA_BASE),
		.mapbase	= TEGRA_UARTA_BASE,
		.irq		= INT_UARTA,
		.flags		= UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type		= PORT_TEGRA,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= 216000000,
	}, {
		.flags		= 0,
	}
};

static struct platform_device debug_uart = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uart_platform_data,
	},
};

static struct tegra_audio_platform_data tegra_spdif_pdata = {
	.dma_on = true,  /* use dma by default */
	.i2s_clk_rate = 5644800,
	.spdif_clk_rate = 5644800,
	.mode = SPDIF_BIT_MODE_MODE16BIT,
	.fifo_fmt = 0,
};

static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[1] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 8,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
};

static struct tegra_ulpi_config ulpi_phy_config = {
	.reset_gpio = TEGRA_GPIO_PG2,
	.clk = "clk_dev2",
	.inf_type = TEGRA_USB_LINK_ULPI,
};

#ifdef CONFIG_BCM4329_RFKILL

static struct resource nbx02_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = TEGRA_GPIO_PU0,
		.end    = TEGRA_GPIO_PU0,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device nbx02_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(nbx02_bcm4329_rfkill_resources),
	.resource       = nbx02_bcm4329_rfkill_resources,
};

static noinline void __init nbx02_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", nbx02_bcm4329_rfkill_device.name, \
				"blink", NULL);

	platform_device_register(&nbx02_bcm4329_rfkill_device);

	return;
}
#else
static inline void nbx02_bt_rfkill(void) { }
#endif

#ifdef CONFIG_BT_BLUESLEEP
static noinline void __init tegra_setup_bluesleep(void)
{
	struct platform_device *pdev = NULL;
	struct resource *res;

	pdev = platform_device_alloc("bluesleep", 0);
	if (!pdev) {
		pr_err("unable to allocate platform device for bluesleep");
		return;
	}

	res = kzalloc(sizeof(struct resource) * 2, GFP_KERNEL);
	if (!res) {
		pr_err("unable to allocate resource for bluesleep\n");
		goto err_free_dev;
	}

	res[0].name   = "gpio_host_wake";
	res[0].start  = TEGRA_GPIO_PS2;
	res[0].end    = TEGRA_GPIO_PS2;
	res[0].flags  = IORESOURCE_IO;

	res[1].name   = "host_wake";
	res[1].start  = gpio_to_irq(TEGRA_GPIO_PS2);
	res[1].end    = gpio_to_irq(TEGRA_GPIO_PS2);
	res[1].flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE;

	if (platform_device_add_resources(pdev, res, 2)) {
		pr_err("unable to add resources to bluesleep device\n");
		goto err_free_res;
	}

	if (platform_device_add(pdev)) {
		pr_err("unable to add bluesleep device\n");
		goto err_free_res;
	}

	tegra_gpio_enable(TEGRA_GPIO_PS2);

	return;

err_free_res:
	kfree(res);
err_free_dev:
	platform_device_put(pdev);
	return;
}
#else
static inline void tegra_setup_bluesleep(void) { }
#endif

static __initdata struct tegra_clk_init_table nbx02_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "uartd",	"clk_m",	26000000,	true},
	{ "uartc",	"pll_m",	600000000,	false},
	{ "uarta",	"pll_p",	216000000,	true}, // debug console
	{ "blink",	"clk_32k",	32768,		false},
	{ "pll_p_out4",	"pll_p",	24000000,	true },
	{ "pwm",        "clk_m",        13000000,       false},
	{ "pll_a",	NULL,		56448000,	true},
	{ "pll_a_out0",	NULL,		11289600,	true},
	{ "i2s1",	"pll_a_out0",	11289600,	true},
	{ "i2s2",	"pll_a_out0",	11289600,	true},
	{ "audio",	"pll_a_out0",	11289600,	true},
	{ "audio_2x",	"audio",	22579200,	true},
	{ "spdif_out",	"pll_a_out0",	5644800,	false},
	{ "kbc",	"clk_32k",	32768,		true},
	{ NULL,		NULL,		0,		0},
};

#define USB_MANUFACTURER_NAME		"SONY"
#define USB_PRODUCT_NAME		"Sony Tablet"
#define USB_PRODUCT_ID_MTP_ADB		0x04d2
#define USB_PRODUCT_ID_MTP		0x04d1
#define USB_PRODUCT_ID_RNDIS		0x7103
#define USB_VENDOR_ID			0x054c

static char *usb_functions_mtp[] = { "mtp" };
static char *usb_functions_mtp_adb[] = { "mtp", "adb" };
#ifdef CONFIG_USB_ANDROID_RNDIS
static char *usb_functions_rndis[] = { "rndis" };
static char *usb_functions_rndis_adb[] = { "rndis", "adb" };
#endif
static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
	"mtp",
	"adb"
};

static struct android_usb_product usb_products[] = {
	{
		.product_id     = USB_PRODUCT_ID_MTP,
		.num_functions  = ARRAY_SIZE(usb_functions_mtp),
		.functions      = usb_functions_mtp,
	},
	{
		.product_id     = USB_PRODUCT_ID_MTP_ADB,
		.num_functions  = ARRAY_SIZE(usb_functions_mtp_adb),
		.functions      = usb_functions_mtp_adb,
	},
#ifdef CONFIG_USB_ANDROID_RNDIS
	{
		.product_id     = USB_PRODUCT_ID_RNDIS,
		.num_functions  = ARRAY_SIZE(usb_functions_rndis),
		.functions      = usb_functions_rndis,
	},
	{
		.product_id     = USB_PRODUCT_ID_RNDIS,
		.num_functions  = ARRAY_SIZE(usb_functions_rndis_adb),
		.functions      = usb_functions_rndis_adb,
	},
#endif
};

/* standard android USB platform data */
static struct android_usb_platform_data andusb_plat = {
	.vendor_id              = USB_VENDOR_ID,
	.product_id             = USB_PRODUCT_ID_MTP_ADB,
	.manufacturer_name      = USB_MANUFACTURER_NAME,
	.product_name           = USB_PRODUCT_NAME,
	.serial_number          = NULL,
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
};

static struct platform_device androidusb_device = {
	.name   = "android_usb",
	.id     = -1,
	.dev    = {
		.platform_data  = &andusb_plat,
	},
};

#ifdef CONFIG_USB_ANDROID_RNDIS
static struct usb_ether_platform_data rndis_pdata = {
	.ethaddr = {0, 0, 0, 0, 0, 0},
	.vendorID = USB_VENDOR_ID,
	.vendorDescr = USB_MANUFACTURER_NAME,
};

static struct platform_device rndis_device = {
	.name   = "rndis",
	.id     = -1,
	.dev    = {
		.platform_data  = &rndis_pdata,
	},
};
#endif

static struct wm8903_platform_data wm8903_pdata = {
        .irq_active_low = 0,
        .micdet_cfg = 0x83,
        .micdet_delay = 0,
        .gpio_base = WM8903_GPIO_BASE,
        .gpio_cfg = {
                (WM8903_GPn_FN_GPIO_MICBIAS_SHORT_DETECT
                 << WM8903_GP1_FN_SHIFT),
                WM8903_GPIO_NO_CONFIG,
                0,                     /* as output pin */
                (WM8903_GPn_FN_GPIO_MICBIAS_CURRENT_DETECT
                 << WM8903_GP4_FN_SHIFT),
                WM8903_GPIO_NO_CONFIG,
        },
};

static struct i2c_board_info __initdata nbx02_i2c_bus1_board_info[] = {
	{
		I2C_BOARD_INFO("wm8903", 0x1a),
                .platform_data = &wm8903_pdata,
	},
};

static struct tegra_ulpi_config nbx02_ehci2_ulpi_phy_config = {
	.reset_gpio = TEGRA_GPIO_PV1,
	.clk = "clk_dev2",
};

static struct tegra_ehci_platform_data nbx02_ehci2_ulpi_platform_data = {
	.operating_mode = TEGRA_USB_HOST,
	.power_down_on_bus_suspend = 0,
	.phy_config = &nbx02_ehci2_ulpi_phy_config,
};

static struct tegra_i2c_platform_data nbx02_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
};

static struct tegra_i2c_platform_data nbx02_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
};

static struct tegra_i2c_platform_data nbx02_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
};

static struct tegra_i2c_platform_data nbx02_dvc_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.is_dvc		= true,
};

static struct tegra_audio_platform_data tegra_audio_pdata[] = {
	/* For I2S1 */
	[0] = {
		.i2s_master	= true,
		.dma_on		= true,  /* use dma by default */
		.i2s_master_clk = 44100,
		.i2s_clk_rate	= 240000000,
		.dap_clk	= "clk_dev1",
		.audio_sync_clk = "audio_2x",
		.mode		= I2S_BIT_FORMAT_I2S,
		.fifo_fmt	= I2S_FIFO_PACKED,
		.bit_size	= I2S_BIT_SIZE_16,
		.i2s_bus_width = 32,
		.dsp_bus_width = 16,
	},
	/* For I2S2 */
	[1] = {
		.i2s_master	= true,
		.dma_on		= true,  /* use dma by default */
		.i2s_master_clk = 8000,
		.dsp_master_clk = 8000,
		.i2s_clk_rate	= 256000,
		.dap_clk	= "clk_dev1",
		.audio_sync_clk = "audio_2x",
		.mode		= I2S_BIT_FORMAT_DSP,
		.fifo_fmt	= I2S_FIFO_16_LSB,
		.bit_size	= I2S_BIT_SIZE_16,
		.i2s_bus_width = 32,
		.dsp_bus_width = 16,
	}
};

static struct tegra_das_platform_data tegra_das_pdata = {
	.dap_clk = "clk_dev1",
	.tegra_dap_port_info_table = {
		/* I2S1 <--> DAC1 <--> DAP1 <--> Hifi Codec */
		[0] = {
			.dac_port = tegra_das_port_i2s1,
			.dap_port = tegra_das_port_dap1,
			.codec_type = tegra_audio_codec_type_hifi,
			.device_property = {
				.num_channels = 2,
				.bits_per_sample = 16,
				.rate = 44100,
				.dac_dap_data_comm_format =
						dac_dap_data_format_all,
			},
		},
		[1] = {
			.dac_port = tegra_das_port_none,
			.dap_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.dac_dap_data_comm_format = 0,
			},
		},
		[2] = {
			.dac_port = tegra_das_port_none,
			.dap_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.dac_dap_data_comm_format = 0,
			},
		},
		/* I2S2 <--> DAC2 <--> DAP4 <--> BT SCO Codec */
		[3] = {
			.dac_port = tegra_das_port_i2s2,
			.dap_port = tegra_das_port_dap4,
			.codec_type = tegra_audio_codec_type_bluetooth,
			.device_property = {
				.num_channels = 1,
				.bits_per_sample = 16,
				.rate = 8000,
				.dac_dap_data_comm_format =
					dac_dap_data_format_dsp,
			},
		},
		[4] = {
			.dac_port = tegra_das_port_none,
			.dap_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.dac_dap_data_comm_format = 0,
			},
		},
	},

	.tegra_das_con_table = {
		[0] = {
			.con_id = tegra_das_port_con_id_hifi,
			.num_entries = 2,
			.con_line = {
				[0] = {tegra_das_port_i2s1, tegra_das_port_dap1, true},
				[1] = {tegra_das_port_dap1, tegra_das_port_i2s1, false},
			},
		},
		[1] = {
			.con_id = tegra_das_port_con_id_bt_codec,
			.num_entries = 4,
			.con_line = {
				[0] = {tegra_das_port_i2s2, tegra_das_port_dap4, true},
				[1] = {tegra_das_port_dap4, tegra_das_port_i2s2, false},
				[2] = {tegra_das_port_i2s1, tegra_das_port_dap1, true},
				[3] = {tegra_das_port_dap1, tegra_das_port_i2s1, false},
			},
		},
	}
};

static void nbx02_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &nbx02_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &nbx02_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &nbx02_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &nbx02_dvc_platform_data;

	i2c_register_board_info(0, nbx02_i2c_bus1_board_info, 1);

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);
}

#ifdef CONFIG_KEYBOARD_GPIO
#define GPIO_KEY(_id, _gpio, _iswake)		\
	{					\
		.code = _id,			\
		.gpio = TEGRA_GPIO_##_gpio,	\
		.active_low = 1,		\
		.desc = #_id,			\
		.type = EV_KEY,			\
		.wakeup = _iswake,		\
		.debounce_interval = 10,	\
	}

static struct gpio_keys_button nbx02_keys[] = {
	[0] = GPIO_KEY(KEY_MENU, PQ3, 0),
	[1] = GPIO_KEY(KEY_HOME, PQ1, 0),
	[2] = GPIO_KEY(KEY_BACK, PQ2, 0),
	[3] = GPIO_KEY(KEY_VOLUMEUP, PQ5, 0),
	[4] = GPIO_KEY(KEY_VOLUMEDOWN, PQ4, 0),
	[5] = GPIO_KEY(KEY_POWER, PV2, 1),
};

static struct gpio_keys_platform_data nbx02_keys_platform_data = {
	.buttons	= nbx02_keys,
	.nbuttons	= ARRAY_SIZE(nbx02_keys),
};

static struct platform_device nbx02_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &nbx02_keys_platform_data,
	},
};

static void nbx02_keys_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(nbx02_keys); i++)
		tegra_gpio_enable(nbx02_keys[i].gpio);
}
#endif

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct platform_device *nbx02_devices[] __initdata = {
	&androidusb_device,
	&debug_uart,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&pmu_device,
	&tegra_udc_device,
	//&tegra_ehci2_device,
	&tegra_gart_device,
	&tegra_aes_device,
#ifdef CONFIG_KEYBOARD_GPIO
	&nbx02_keys_device,
#endif
	&tegra_wdt_device,
	&tegra_das_device,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_avp_device,
	&tegra_camera,
};


#ifdef CONFIG_TOUCHSCREEN_PANJIT_I2C
static struct panjit_i2c_ts_platform_data panjit_data = {
	.gpio_reset = TEGRA_GPIO_PQ7,
};

static const struct i2c_board_info nbx02_i2c_bus1_touch_info[] = {
	{
	 I2C_BOARD_INFO("panjit_touch", 0x3),
	 .irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	 .platform_data = &panjit_data,
	 },
};

static int __init nbx02_touch_init_panjit(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PV6);

	tegra_gpio_enable(TEGRA_GPIO_PQ7);
	i2c_register_board_info(0, nbx02_i2c_bus1_touch_info, 1);

	return 0;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MT_T9
/* Atmel MaxTouch touchscreen              Driver data */
/*-----------------------------------------------------*/
/*
 * Reads the CHANGELINE state; interrupt is valid if the changeline
 * is low.
 */
static u8 read_chg(void)
{
	return gpio_get_value(TEGRA_GPIO_PV6);
}

static u8 valid_interrupt(void)
{
	return !read_chg();
}

static struct mxt_platform_data Atmel_mxt_info = {
	/* Maximum number of simultaneous touches to report. */
	.numtouch = 10,
	// TODO: no need for any hw-specific things at init/exit?
	.init_platform_hw = NULL,
	.exit_platform_hw = NULL,
	.max_x = 1366,
	.max_y = 768,
	.valid_interrupt = &valid_interrupt,
	.read_chg = &read_chg,
};

static struct i2c_board_info __initdata i2c_info[] = {
	{
	 I2C_BOARD_INFO("maXTouch", MXT_I2C_ADDRESS),
	 .irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	 .platform_data = &Atmel_mxt_info,
	 },
};

static int __init nbx02_touch_init_atmel(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PV6);
	tegra_gpio_enable(TEGRA_GPIO_PQ7);

	gpio_set_value(TEGRA_GPIO_PQ7, 0);
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PQ7, 1);
	msleep(100);

	i2c_register_board_info(0, i2c_info, 1);

	return 0;
}
#endif

#if defined (CONFIG_TOUCHSCREEN_TMA300) || (CONFIG_TOUCHSCREEN_TMA300_MODULE)
#include <linux/input/nbx_touch.h>

static void touch_gpio_enable(int gpio_num){
	tegra_gpio_enable(gpio_num);
	return;
}

static void touch_pinmux_set_pullupdown(int group, int pullupdown){
	tegra_pinmux_set_pullupdown(group, pullupdown);
	return;
}

static struct nbx_touch_platform_data nbx02_i2c_touch_info = {

	.gpio_enable = touch_gpio_enable,
	.pinmux_set_pullupdown = touch_pinmux_set_pullupdown,
};

static const struct i2c_board_info nbx02_i2c_bus1_touch_info[] = {
	{
		I2C_BOARD_INFO("i2c_nbx_touch", 0x1b),
		.platform_data = &nbx02_i2c_touch_info,
	},
	{
		I2C_BOARD_INFO("i2c_nbx_touch", 0x3b),
		.platform_data = &nbx02_i2c_touch_info,
	},
};

static int __init nbx02_touch_init_tma300(void)
{
	i2c_register_board_info(1, nbx02_i2c_bus1_touch_info, ARRAY_SIZE(nbx02_i2c_bus1_touch_info));

	return 0;
}
#endif

static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
			.phy_config = &utmi_phy_config[0],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 0,
	},
	[1] = {
			.phy_config = &ulpi_phy_config,
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
	[2] = {
			.phy_config = &utmi_phy_config[1],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 0,
	},
};

static struct platform_device *tegra_usb_otg_host_register(void)
{
	struct platform_device *pdev;
	void *platform_data;
	int val;

	pdev = platform_device_alloc(tegra_ehci1_device.name, tegra_ehci1_device.id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, tegra_ehci1_device.resource,
		tegra_ehci1_device.num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask =  tegra_ehci1_device.dev.dma_mask;
	pdev->dev.coherent_dma_mask = tegra_ehci1_device.dev.coherent_dma_mask;

	platform_data = kmalloc(sizeof(struct tegra_ehci_platform_data), GFP_KERNEL);
	if (!platform_data)
		goto error;

	memcpy(platform_data, &tegra_ehci_pdata[0],
				sizeof(struct tegra_ehci_platform_data));
	pdev->dev.platform_data = platform_data;

	val = platform_device_add(pdev);
	if (val)
		goto error_add;

	return pdev;

error_add:
	kfree(platform_data);
error:
	pr_err("%s: failed to add the host contoller device\n", __func__);
	platform_device_put(pdev);
	return NULL;
}

static void tegra_usb_otg_host_unregister(struct platform_device *pdev)
{
	kfree(pdev->dev.platform_data);
	platform_device_unregister(pdev);
}

static struct tegra_otg_platform_data tegra_otg_pdata = {
	.host_register = &tegra_usb_otg_host_register,
	.host_unregister = &tegra_usb_otg_host_unregister,
};

static int __init nbx02_gps_init(void)
{
	struct clk *clk32 = clk_get_sys(NULL, "blink");
	if (!IS_ERR(clk32)) {
		clk_set_rate(clk32,clk32->parent->rate);
		clk_enable(clk32);
	}

	tegra_gpio_enable(TEGRA_GPIO_PZ3);
	return 0;
}

static void nbx02_power_off(void)
{
	int ret;

	ret = tps6586x_power_off();
	if (ret)
		pr_err("nbx02: failed to power off\n");

	while(1);
}

static void __init nbx02_power_off_init(void)
{
	pm_power_off = nbx02_power_off;
}

#define SERIAL_NUMBER_LENGTH 20
static char usb_serial_num[SERIAL_NUMBER_LENGTH];
static void nbx02_usb_init(void)
{
	char *src = NULL;
	int i;

	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	tegra_ehci3_device.dev.platform_data=&tegra_ehci_pdata[2];
	platform_device_register(&tegra_ehci3_device);

#ifdef CONFIG_USB_ANDROID_RNDIS
	src = usb_serial_num;

	/* create a fake MAC address from our serial number.
	 * first byte is 0x02 to signify locally administered.
	 */
	rndis_pdata.ethaddr[0] = 0x02;
	for (i = 0; *src; i++) {
		/* XOR the USB serial across the remaining bytes */
		rndis_pdata.ethaddr[i % (ETH_ALEN - 1) + 1] ^= *src++;
	}
	platform_device_register(&rndis_device);
#endif
}

static struct resource mbm_wow_resources[] = {
    {
        .start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PN7),
        .end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PN7),
        .flags = IORESOURCE_IRQ,
    },
};

static struct platform_device mbm_wow_device = {
    .name          = "mbm_wow",
    .id            = -1,
    .resource      = mbm_wow_resources,
    .num_resources = ARRAY_SIZE(mbm_wow_resources),
};

static void __init nbx02_wwan_init(void)
{
    int ret = 0;
    u32 odm_rsvd[8];
    int disable_wwan;
    int DISABLE_WWAN_BIT = 28;

    ret = tegra_fuse_read(ODM_RSVD, odm_rsvd, sizeof(odm_rsvd));

    if(ret == 0) {
        disable_wwan = (odm_rsvd[7] >> DISABLE_WWAN_BIT) & 0x01;

        if(disable_wwan == 0) {
            wireless_power_control(WPC_MODULE_WWAN, 1);
            msleep(10);
            wireless_power_control(WPC_MODULE_WWAN_RF, 1);

            platform_device_register(&mbm_wow_device);
        }
    }
}
 
static struct nbx_ec_battery_platform_data nbx_ec_battery_pdata = {
	.degradation = 0,
};

static struct platform_device nbx_ec_battery_device = {
	.name = "nbx_ec_battery",
	.id = 0,
	.dev = {
		.platform_data = &nbx_ec_battery_pdata,
	}
};

static void __init nbx02_power_supply_init(void)
{
	platform_device_register(&nbx_ec_battery_device);
}

static struct a1026_platform_data a1026_pdata = {
	.reset_pin = TEGRA_GPIO_PU1,
	.wake_pin = TEGRA_GPIO_PU0,
	.clk = "clk_dev2",
	.is_awake = 0,
};

static const struct i2c_board_info i2c_a1026_info[] = {
	{
		I2C_BOARD_INFO("a1026", 0x3e),
		.platform_data = &a1026_pdata,
	},
};


static void __init tegra_nbx02_init(void)
{
#if defined(CONFIG_TOUCHSCREEN_PANJIT_I2C) || \
	defined(CONFIG_TOUCHSCREEN_ATMEL_MT_T9)
	struct board_info BoardInfo;
#endif

	pr_info("board-nbx02.c: nbx_odmdata=%08x\n", nbx_get_odmdata());

	tegra_common_init();
	tegra_clk_init_from_table(nbx02_clk_init_table);
	nbx02_pinmux_init();
	nbx02_i2c_init();
	snprintf(usb_serial_num, sizeof(usb_serial_num), "%llx", tegra_chip_uid());
	andusb_plat.serial_number = kstrdup(usb_serial_num, GFP_KERNEL);
	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata[0];
	tegra_i2s_device2.dev.platform_data = &tegra_audio_pdata[1];
	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;
	tegra_das_device.dev.platform_data = &tegra_das_pdata;
	tegra_ehci2_device.dev.platform_data
		= &nbx02_ehci2_ulpi_platform_data;
	platform_add_devices(nbx02_devices, ARRAY_SIZE(nbx02_devices));

	nbx02_sdhci_init();
	//nbx02_charge_init();
	nbx02_regulator_init();

#if defined(CONFIG_TOUCHSCREEN_PANJIT_I2C) || \
	defined(CONFIG_TOUCHSCREEN_ATMEL_MT_T9)

	tegra_get_board_info(&BoardInfo);

	/* boards with sku > 0 have atmel touch panels */
	if (BoardInfo.sku) {
		pr_info("Initializing Atmel touch driver\n");
		nbx02_touch_init_atmel();
	} else {
		pr_info("Initializing Panjit touch driver\n");
		nbx02_touch_init_panjit();
	}
#endif

#if defined (CONFIG_TOUCHSCREEN_TMA300) || (CONFIG_TOUCHSCREEN_TMA300_MODULE)
	pr_info("Initializing TMA300 touch driver\n");
	nbx02_touch_init_tma300();
#endif

#ifdef CONFIG_KEYBOARD_GPIO
	nbx02_keys_init();
#endif
#ifdef CONFIG_KEYBOARD_TEGRA
	nbx02_kbc_init();
#endif

	nbx02_usb_init();
	//nbx02_gps_init();
	nbx02_panel_init();
	nbx02_sensors_init();
	//nbx02_bt_rfkill();
	nbx02_power_off_init();
	nbx02_emc_init();
#ifdef CONFIG_BT_BLUESLEEP
        tegra_setup_bluesleep();
#endif
	nbx02_wired_jack_init();
	nbx02_wwan_init();

	nbx02_power_supply_init();

	i2c_register_board_info(0, i2c_a1026_info, 
	    ARRAY_SIZE(i2c_a1026_info));

}

int __init tegra_nbx02_protected_aperture_init(void)
{
	tegra_protected_aperture_init(tegra_grhost_aperture);
	return 0;
}
late_initcall(tegra_nbx02_protected_aperture_init);

#define ODMDATA_MEMSIZE_MASK 0x000000c0
void __init tegra_nbx02_reserve(void)
{
	unsigned long cmdline_memsize, odmdata_memsize;

	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	cmdline_memsize = memblock_end_of_DRAM();
	odmdata_memsize = (nbx_get_odmdata() & ODMDATA_MEMSIZE_MASK) ? SZ_1G : SZ_512M;
	if (cmdline_memsize != odmdata_memsize) {
		pr_warn("Memory size mismatch between cmdline and odmdata\n");
		pr_warn(" cmdline: 0x%08lx, odmdata: 0x%08lx\n", cmdline_memsize, odmdata_memsize);
	}

	if (memblock_remove(SZ_512M - SZ_2M, SZ_2M) < 0)
		pr_warn("Cannot reserve memory\n");

	/* FIXME: carveout_size, fb_size, fb2_size(for 2nd display)
	 * carveout_size: minimum=128M, with flash=160M, recommendation=256M
	 * refer to NvBug#786465 */
	if (min(cmdline_memsize, odmdata_memsize) == SZ_512M)
		tegra_reserve(SZ_128M, SZ_8M, 0);
	else
		tegra_reserve(SZ_256M, SZ_8M, 0);
}

MACHINE_START(NBX02, "nbx02")
	.boot_params    = 0x00000100,
	.phys_io        = IO_APB_PHYS,
	.io_pg_offst    = ((IO_APB_VIRT) >> 18) & 0xfffc,
	.init_irq       = tegra_init_irq,
	.init_machine   = tegra_nbx02_init,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_nbx02_reserve,
	.timer          = &tegra_timer,
MACHINE_END
