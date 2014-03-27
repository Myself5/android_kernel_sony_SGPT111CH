/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * arch/arm/mach-tegra/board-nbx03.c
 *
 * Copyright (c) 2010, NVIDIA Corporation.
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
#include <linux/usb/f_accessory.h>
#include <linux/mfd/tps6586x.h>
#include <linux/memblock.h>
#include <linux/nbx_dock.h>
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
#include <mach/hci-event.h>
#include <mach/a1026.h>

#include "board.h"
#include "clock.h"
#include "board-nbx03.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "board-nbx-common.h"
#include "wireless_power_control.h"

#if defined(CONFIG_TOUCHSCREEN_CYTTSP_I2C) || defined(CONFIG_TOUCHSCREEN_CYTTSP_I2C_MODULE)
#define TOUCHSCREEN_CYTTSP_I2C
#endif

#ifdef TOUCHSCREEN_CYTTSP_I2C
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
#endif


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

static struct resource nbx03_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = TEGRA_GPIO_PU0,
		.end    = TEGRA_GPIO_PU0,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device nbx03_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(nbx03_bcm4329_rfkill_resources),
	.resource       = nbx03_bcm4329_rfkill_resources,
};

static noinline void __init nbx03_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", nbx03_bcm4329_rfkill_device.name, \
				"blink", NULL);

	platform_device_register(&nbx03_bcm4329_rfkill_device);

	return;
}
#else
static inline void nbx03_bt_rfkill(void) { }
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

static __initdata struct tegra_clk_init_table nbx03_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "uartd",	"pll_p",	216000000,	true},
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
#if 1
        { "disp1",      "pll_p",        216000000,      true},
#endif
	{ "spdif_out",	"pll_a_out0",	5644800,	false},
	{ "kbc",	"clk_32k",	32768,		true},
	{ NULL,		NULL,		0,		0},
};

#define USB_MANUFACTURER_NAME		"SONY"
#define USB_PRODUCT_NAME		"Sony Tablet"
#define USB_PRODUCT_ID_MTP_ADB		0x05b4
#define USB_PRODUCT_ID_MTP		0x05b3
#define USB_PRODUCT_ID_RNDIS		0x7103
#define USB_VENDOR_ID			0x054c

static char *usb_functions_mtp[] = { "mtp" };
static char *usb_functions_mtp_adb[] = { "mtp", "adb" };

#ifdef CONFIG_USB_ANDROID_ACCESSORY
static char *usb_functions_accessory[] = { "accessory" };
static char *usb_functions_accessory_adb[] = { "accessory", "adb" };
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
static char *usb_functions_rndis[] = { "rndis" };
static char *usb_functions_rndis_adb[] = { "rndis", "adb" };
#endif
static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_ACCESSORY
       "accessory",
#endif
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
#ifdef CONFIG_USB_ANDROID_ACCESSORY
       {
               .vendor_id      = USB_ACCESSORY_VENDOR_ID,
               .product_id     = USB_ACCESSORY_PRODUCT_ID,
               .num_functions  = ARRAY_SIZE(usb_functions_accessory),
               .functions      = usb_functions_accessory,
       },
       {
               .vendor_id      = USB_ACCESSORY_VENDOR_ID,
               .product_id     = USB_ACCESSORY_ADB_PRODUCT_ID,
               .num_functions  = ARRAY_SIZE(usb_functions_accessory_adb),
               .functions      = usb_functions_accessory_adb,
       },
#endif
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

static struct i2c_board_info __initdata nbx03_i2c_bus1_board_info[] = {
	{
		I2C_BOARD_INFO("wm8903", 0x1a),
                .platform_data = &wm8903_pdata,
	},
};

static struct tegra_ulpi_config nbx03_ehci2_ulpi_phy_config = {
	.reset_gpio = TEGRA_GPIO_PV1,
	.clk = "clk_dev2",
};

static struct tegra_ehci_platform_data nbx03_ehci2_ulpi_platform_data = {
	.operating_mode = TEGRA_USB_HOST,
	.power_down_on_bus_suspend = 0,
	.phy_config = &nbx03_ehci2_ulpi_phy_config,
};

static struct tegra_i2c_platform_data nbx03_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
};

static struct tegra_i2c_platform_data nbx03_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
};

static struct tegra_i2c_platform_data nbx03_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
};

static struct tegra_i2c_platform_data nbx03_dvc_platform_data = {
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
		.i2s_clk_rate	= 11289600,
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

#ifdef TOUCHSCREEN_CYTTSP_I2C
static int cyttsp_wakeup(void)
{
	int ret;

	ret = gpio_direction_output(TEGRA_GPIO_PA4, 1);
	if (ret) {
		printk(KERN_ERR "%s: Failed to request gpio_direction_output\n",
		__func__);
                return ret;
	}
	gpio_set_value(TEGRA_GPIO_PA4, 1);
	msleep(300);
	return 0;
}

static int cyttsp_poweroff(void)
{
	int ret;

	ret = gpio_direction_output(TEGRA_GPIO_PA4, 0);
	if (ret) {
		printk(KERN_ERR "%s: Failed to request gpio_direction_output\n",
		__func__);
                return ret;
	}        
	gpio_set_value(TEGRA_GPIO_PA4, 0);
	/*msleep(3); */
	return 0;
}

static int cyttsp_init(int on)
{
	int ret;
	if (on) {
#if 1                
		ret = gpio_request(TEGRA_GPIO_PG0, "CYTTSP IRQ GPIO");
		if (ret) {
			printk(KERN_ERR "%s: Failed to request GPIO %d\n",
			       __func__, TEGRA_GPIO_PG0);
			return ret;
		}
#endif
		tegra_gpio_enable(TEGRA_GPIO_PG0);
		gpio_direction_input(TEGRA_GPIO_PG0);

#if 1
        	ret = gpio_request(TEGRA_GPIO_PA4, "CYTTSP POWER GPIO");
        	if (ret) {
        		printk(KERN_ERR "%s: Failed to request GPIO %d\n",
        		       __func__, TEGRA_GPIO_PA4);
        		return ret;
        	}
#endif
		tegra_gpio_enable(TEGRA_GPIO_PA4);
		gpio_direction_output(TEGRA_GPIO_PA4, 1);

	} else {
		gpio_free(TEGRA_GPIO_PG0);
		gpio_free(TEGRA_GPIO_PA4);
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
	.irq_gpio = TEGRA_GPIO_PG0,
	.bl_keys = dflt_bl_keys,
};

#endif /* TOUCHSCREEN_CYTTSP_I2C */

#ifdef TOUCHSCREEN_CYTTSP_I2C
static const struct i2c_board_info nbx03_i2c_bus1_touch_info[] = {
	{
		I2C_BOARD_INFO(CY_I2C_NAME, 0x67),
		.platform_data = &cypress_i2c_touch_data,
	},
};

static int __init nbx03_touch_init(void)
{
	i2c_register_board_info(1, nbx03_i2c_bus1_touch_info, ARRAY_SIZE(nbx03_i2c_bus1_touch_info));

	return 0;
}
#endif

static void nbx03_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &nbx03_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &nbx03_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &nbx03_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &nbx03_dvc_platform_data;

	i2c_register_board_info(0, nbx03_i2c_bus1_board_info, 1);

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

static struct gpio_keys_button nbx03_keys[] = {
	[0] = GPIO_KEY(KEY_MENU, PQ3, 0),
	[1] = GPIO_KEY(KEY_HOME, PQ1, 0),
	[2] = GPIO_KEY(KEY_BACK, PQ2, 0),
	[3] = GPIO_KEY(KEY_VOLUMEUP, PQ5, 0),
	[4] = GPIO_KEY(KEY_VOLUMEDOWN, PQ4, 0),
	[5] = GPIO_KEY(KEY_POWER, PV2, 1),
};

static struct gpio_keys_platform_data nbx03_keys_platform_data = {
	.buttons	= nbx03_keys,
	.nbuttons	= ARRAY_SIZE(nbx03_keys),
};

static struct platform_device nbx03_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &nbx03_keys_platform_data,
	},
};

static void nbx03_keys_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(nbx03_keys); i++)
		tegra_gpio_enable(nbx03_keys[i].gpio);
}
#endif

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct hci_event_platform_data hci_event_platform = {
	.ocdet_pin = TEGRA_GPIO_PO1, /* USB_OC */
	.pwren_pin = TEGRA_GPIO_PD0, /* PWR_EN */
};
static struct platform_device hci_event_device = {
	.name = "hci-event",
	.dev = {
	.platform_data = &hci_event_platform,
	},
};

static struct nbx_dock_platform_data nbx_dock_platform = {
	.dockdet_pin = TEGRA_GPIO_PU6, /* DOCK_DET */
};
static struct platform_device nbx_dock_device = {
	.name = "nbx_dock",
	.dev = {
	.platform_data = &nbx_dock_platform,
	},
};


static struct platform_device *nbx03_devices[] __initdata = {
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
	&nbx03_keys_device,
#endif
	&tegra_wdt_device,
	&tegra_das_device,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_avp_device,
	&tegra_camera,
	&hci_event_device,
	&nbx_dock_device,
};


#ifdef CONFIG_TOUCHSCREEN_PANJIT_I2C
static struct panjit_i2c_ts_platform_data panjit_data = {
	.gpio_reset = TEGRA_GPIO_PQ7,
};

static const struct i2c_board_info nbx03_i2c_bus1_touch_info[] = {
	{
	 I2C_BOARD_INFO("panjit_touch", 0x3),
	 .irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	 .platform_data = &panjit_data,
	 },
};

static int __init nbx03_touch_init_panjit(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PV6);

	tegra_gpio_enable(TEGRA_GPIO_PQ7);
	i2c_register_board_info(0, nbx03_i2c_bus1_touch_info, 1);

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

static int __init nbx03_touch_init_atmel(void)
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


static struct usb_phy_plat_data tegra_usb_phy_pdata[] = {
	[0] = {
			.instance = 0,
			.vbus_irq = TPS6586X_INT_BASE + TPS6586X_INT_USB_DET,
			.vbus_gpio = TEGRA_GPIO_PD0,
	},
	[1] = {
			.instance = 1,
			.vbus_gpio = -1,
	},
	[2] = {
			.instance = 2,
			.vbus_gpio = TEGRA_GPIO_PD3,
	},
};


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
	pdev->dev.platform_data = NULL;
	platform_device_unregister(pdev);
}

static struct tegra_otg_platform_data tegra_otg_pdata = {
	.host_register = &tegra_usb_otg_host_register,
	.host_unregister = &tegra_usb_otg_host_unregister,
};

static int __init nbx03_gps_init(void)
{
	struct clk *clk32 = clk_get_sys(NULL, "blink");
	if (!IS_ERR(clk32)) {
		clk_set_rate(clk32,clk32->parent->rate);
		clk_enable(clk32);
	}

	tegra_gpio_enable(TEGRA_GPIO_PZ3);
	return 0;
}

static void nbx03_power_off(void)
{
	int ret;

	ret = tps6586x_power_off();
	if (ret)
		pr_err("nbx03: failed to power off\n");

	while(1);
}

static void __init nbx03_power_off_init(void)
{
	pm_power_off = nbx03_power_off;
}

#define SERIAL_NUMBER_LENGTH 20
static char usb_serial_num[SERIAL_NUMBER_LENGTH];
static void nbx03_usb_init(void)
{
	char *src = NULL;
	int i;

	tegra_usb_phy_init(tegra_usb_phy_pdata, ARRAY_SIZE(tegra_usb_phy_pdata));

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

static void __init nbx03_wwan_init(void)
{
	wireless_power_control(WPC_MODULE_WWAN, 1);
	msleep(10);
	wireless_power_control(WPC_MODULE_WWAN_RF, 1);

	platform_device_register(&mbm_wow_device);
}

static struct nbx_ec_battery_platform_data nbx_ec_battery_pdata = {
	.degradation = 1,
};

static struct platform_device nbx_ec_battery_device = {
	.name = "nbx_ec_battery",
	.id = 0,
	.dev = {
		.platform_data = &nbx_ec_battery_pdata,
	}
};

static void __init nbx03_power_supply_init(void)
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

static void __init tegra_nbx03_init(void)
{
#if defined(CONFIG_TOUCHSCREEN_PANJIT_I2C) || \
	defined(CONFIG_TOUCHSCREEN_ATMEL_MT_T9)
	struct board_info BoardInfo;
#endif

	pr_info("board-nbx03: odmdata=%08x\n", nbx_get_odmdata());

	tegra_common_init();
	tegra_clk_init_from_table(nbx03_clk_init_table);
	nbx03_pinmux_init();
	nbx03_i2c_init();
	snprintf(usb_serial_num, sizeof(usb_serial_num), "%llx", tegra_chip_uid());
	andusb_plat.serial_number = kstrdup(usb_serial_num, GFP_KERNEL);
	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata[0];
	tegra_i2s_device2.dev.platform_data = &tegra_audio_pdata[1];
	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;
	tegra_das_device.dev.platform_data = &tegra_das_pdata;
	tegra_ehci2_device.dev.platform_data
		= &nbx03_ehci2_ulpi_platform_data;
	platform_add_devices(nbx03_devices, ARRAY_SIZE(nbx03_devices));

	nbx03_sdhci_init();
	nbx03_charge_init();
	nbx03_regulator_init();

#if defined(CONFIG_TOUCHSCREEN_PANJIT_I2C) || \
	defined(CONFIG_TOUCHSCREEN_ATMEL_MT_T9)

	tegra_get_board_info(&BoardInfo);

	/* boards with sku > 0 have atmel touch panels */
	if (BoardInfo.sku) {
		pr_info("Initializing Atmel touch driver\n");
		nbx03_touch_init_atmel();
	} else {
		pr_info("Initializing Panjit touch driver\n");
		nbx03_touch_init_panjit();
	}
#endif

#ifdef TOUCHSCREEN_CYTTSP_I2C
	pr_info("Initializing cypress touch driver\n");
	nbx03_touch_init();
#endif

#ifdef CONFIG_KEYBOARD_GPIO
	nbx03_keys_init();
#endif
#ifdef CONFIG_KEYBOARD_TEGRA
	nbx03_kbc_init();
#endif

	nbx03_usb_init();
	//nbx03_gps_init();
	nbx03_panel_init();
	nbx03_sensors_init();
	//nbx03_bt_rfkill();
	nbx03_power_off_init();
	nbx03_emc_init();
#ifdef CONFIG_BT_BLUESLEEP
        tegra_setup_bluesleep();
#endif
	nbx03_wired_jack_init();
	nbx03_wwan_init();

	nbx03_power_supply_init();

	i2c_register_board_info(0, i2c_a1026_info, 
	    ARRAY_SIZE(i2c_a1026_info));

}

int __init tegra_nbx03_protected_aperture_init(void)
{
	tegra_protected_aperture_init(tegra_grhost_aperture);
	return 0;
}
late_initcall(tegra_nbx03_protected_aperture_init);

#define ODMDATA_MEMSIZE_MASK 0x000000c0
void __init tegra_nbx03_reserve(void)
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

MACHINE_START(NBX03, "nbx03")
	.boot_params    = 0x00000100,
	.phys_io        = IO_APB_PHYS,
	.io_pg_offst    = ((IO_APB_VIRT) >> 18) & 0xfffc,
	.init_irq       = tegra_init_irq,
	.init_machine   = tegra_nbx03_init,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_nbx03_reserve,
	.timer          = &tegra_timer,
MACHINE_END
