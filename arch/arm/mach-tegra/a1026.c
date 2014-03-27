/* 2011-06-10: File added and changed by Sony Corporation */
/*
 * Copyright (c) 2010 Sony Corporation.
 * All rights reserved.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/earlysuspend.h>

#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <mach/a1026.h>
#include <mach/audio.h>

#define DEV_NAME "a1026"
#define SYNC_POLLING_COUNT 10

#define ERROR(format, args...) \
	printk(KERN_ERR "ERR[%s:%d]" format, __FILE__, __LINE__ , ##args)

#if 1
#define DPRINTF(format, args...) \
	printk("["DEV_NAME"]" format, ##args)
#else
#define DPRINTF(format, args...) do {} while(0)
#endif

/* parameters for FW version B5767 for NBX0300 */
const uint8_t ptApiCmd_nbx0300[] = {
	0x80, 0x1C, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x02,
	0x80, 0x17, 0x00, 0x03,
	0x80, 0x18, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x34,
	0x80, 0x18, 0x00, 0x0A,
	0x80, 0x17, 0x00, 0x12,
	0x80, 0x18, 0xFF, 0xFB,
	0x80, 0x17, 0x00, 0x04,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x32, 0x00, 0x98,
	0x80, 0x17, 0x00, 0x20,
	0x80, 0x18, 0x00, 0x02,
	0x80, 0x17, 0x00, 0x36,
	0x80, 0x18, 0x00, 0x01,
	0x80, 0x0C, 0x03, 0x00,
	0x80, 0x0D, 0x00, 0x01,
	0x80, 0x1B, 0x00, 0x11,
	0x80, 0x26, 0x00, 0x14,
};

const uint8_t ptApiAck_nbx0300[] = {
	0x80, 0x1C,
	0x80, 0x17,
	0x80, 0x18,
	0x80, 0x17,
	0x80, 0x18,
	0x80, 0x17,
	0x80, 0x18,
	0x80, 0x17,
	0x80, 0x18,
	0x80, 0x17,
	0x80, 0x18,
	0x80, 0x32,
	0x80, 0x17,
	0x80, 0x18,
	0x80, 0x17,
	0x80, 0x18,
	0x80, 0x0C,
	0x80, 0x0D,
	0x80, 0x1B,
	0x80, 0x26,
};

/* parameters for FW version B5987 for NBX0200 */
/* start with pass through setting until tuned parameter is ready */
const uint8_t ptApiCmd_nbx0200[] = {
	0x80, 0x1C, 0x00, 0x00,
};

const uint8_t ptApiAck_nbx0200[] = {
	0x80, 0x1C,
};

static struct i2c_device_id a1026_id[] = {
	{DEV_NAME, 0},
	{},
};

static struct i2c_client *a1026_i2c;

static int
boot_command(struct i2c_client *cl)
{
	int ret;
	uint8_t bootcmd[] = {0x00, 0x01};
	uint8_t bootack[] = {0x01, 0x01};
	uint8_t buf[sizeof(bootack)];

	if ((ret = i2c_master_send(cl, bootcmd, sizeof(bootcmd))) < 0) {
		ERROR("bootcmd fail.  (ret = %d)\n", ret);
		return -1;
	}
	msleep(1);
	if ((ret = i2c_master_recv(cl, buf, sizeof(bootack))) < 0) {
		ERROR("bootack fail.  (ret = %d)\n", ret);
		return -1;
	}
	if (memcmp(buf, bootack, sizeof(bootack))) {
		ERROR("invallid ack %02x:%02x\n", buf[0], buf[1]);
		return -1;
	}
	return 0;
}

static int
path_through_command(struct i2c_client *cl)
{
	int ret;
	uint8_t ptcmd[] = {0x80, 0x1c, 0x00, 0x00};
	uint8_t ptack[] = {0x80, 0x1c};
	uint8_t buf[sizeof(ptack)];

	if (( ret = i2c_master_send(cl, ptcmd, sizeof(ptcmd))) < 0) {
		ERROR("ptcmd fail.  (ret = %d)\n", ret);
		return -1;
	}
	msleep(1);
	if (( ret = i2c_master_recv(cl, buf, sizeof(ptack))) < 0) {
		ERROR("ptcmd fail.  (ret = %d)\n", ret);
		return -1;
	}
	if (memcmp(buf, ptack, sizeof(ptack))) {
		ERROR("invallid ack %02x:%02x\n", buf[0], buf[1]);
		return -1;
	}
	return 0;
}

static int
sleep_command(struct i2c_client *cl)
{
	int ret;
	uint8_t sleepcmd[] = {0x80, 0x10, 0x00, 0x01};

	if (( ret = i2c_master_send(cl, sleepcmd, sizeof(sleepcmd))) < 0) {
		ERROR("sleepcmd fail.  (ret = %d)\n", ret);
		return -1;
	}

	return 0;
}

static int
sync_command(struct i2c_client *cl)
{
	int ret;
	uint8_t synccmd[] = {0x80, 0x00, 0x00, 0x00};
	uint8_t syncnack[] = {0x00, 0x00, 0x00, 0x00};
	uint8_t buf[sizeof(syncnack)];
	int i;


	for (i = 0; i < SYNC_POLLING_COUNT; i++) {

		if ((ret = i2c_master_send(cl, synccmd, sizeof(synccmd))) < 0) {
			ERROR("synccmd fail.  (ret = %d)\n", ret);
			continue;
		}
		msleep(20);
		if ((ret = i2c_master_recv(cl, buf, sizeof(syncnack))) < 0) {
			ERROR("syncack fail.  (ret = %d)\n", ret);
			continue;
		}
		if (memcmp(buf, syncnack, sizeof(syncnack)))
			break;

		DPRINTF("not ready\n");
	}
	if (i == SYNC_POLLING_COUNT) {
		ERROR("sync_command: time out\n");
		return -1;
	} else {
		return 0;
	}
}

static int
check_version(struct i2c_client *cl)
{
        int ret;
	int max_version_length = 64;
	uint8_t ptVersion_cmd0[] = {0x80, 0x20, 0x00, 0x00};
	uint8_t ptVersion_ack0[] = {0x80, 0x20};
	uint8_t ptVersion_cmd1[] = {0x80, 0x21, 0x00, 0x00};
	uint8_t ptVersion_resp1[sizeof(ptVersion_cmd1)];
	uint8_t *ptVersion;
	uint8_t *buff_temp;

	ptVersion = kzalloc(max_version_length, GFP_KERNEL);
	if (!ptVersion) {
		printk("Fail to allocate memory\n");	
		return -1;
	}
	buff_temp = ptVersion;

	if (( ret = i2c_master_send(cl, ptVersion_cmd0, sizeof(ptVersion_cmd0))) < 0) {
		ERROR("ptVersion_cmd0 fail.  (ret = %d)\n", ret);
		goto err;
	}
	msleep(20);
	if (( ret = i2c_master_recv(cl, ptVersion_resp1, sizeof(ptVersion_resp1))) < 0) {
		ERROR("ptVersion_cmd1 fail.  (ret = %d)\n", ret);
		goto err;
	}

	if (memcmp(ptVersion_resp1, ptVersion_ack0, 2)) {
		ERROR("invallid ack %02x:%02x\n", ptVersion_resp1[0], ptVersion_resp1[1]);
		goto err;
	}
	msleep(20);
	while (ptVersion_resp1[3]) {
		if (( ret = i2c_master_send(cl, ptVersion_cmd1, sizeof(ptVersion_cmd1))) < 0) {
			ERROR("ptVersion_cmd1 fail.  (ret = %d)\n", ret);
			goto err;
		}
		msleep(20);
		if (( ret = i2c_master_recv(cl, ptVersion_resp1, sizeof(ptVersion_cmd1))) < 0) {
			ERROR("ptVersion_cmd1 fail.  (ret = %d)\n", ret);
			goto err;
		}
		*buff_temp = ptVersion_resp1[3];
		buff_temp++;
	}
	if (strstr(ptVersion, "NBX0200")) {
		ret = VERSION_NBX0200;
	} else if (strstr(ptVersion, "B4099")) {
		ret = VERSION_PASSTHROUGH;
	} else if (strstr(ptVersion, "NBX0300")) {
		ret = VERSION_NBX0300;
	} else {
		ret = VERSION_UNKNOWN;
	}

 err:
	kfree(ptVersion);
	return ret;
}

static int
load_parameters(struct i2c_client *cl)
{
	int ret;
	int i, j;
	int cmd_size = 4;
	int ack_size = 2;
	uint8_t buf[ack_size];
	struct a1026_platform_data *sdat;

	sdat = cl->dev.platform_data;

	if (!sdat->fw_version){
		sdat->fw_version = check_version(cl);
	}
	switch (sdat->fw_version) {
	case VERSION_PASSTHROUGH:
		DPRINTF("VERSION: PATH THROUGH\n");
		path_through_command(cl);
                break;
	case VERSION_NBX0200:
		DPRINTF("VERSION: NBX0200\n");
		for (i = 0, j = 0; i < sizeof(ptApiCmd_nbx0200); i += cmd_size, j += ack_size) {
			if (( ret = i2c_master_send(cl, ptApiCmd_nbx0200 + i, cmd_size)) < 0) {
				ERROR("ptApiCmd_nbx0200 fail.  (ret = %d)\n", ret);
				return -1;
			}
			msleep(20);
			if (( ret = i2c_master_recv(cl, buf, ack_size)) < 0) {
				ERROR("ptApiCmd_nbx0200 fail.  (ret = %d)\n", ret);
				return -1;
			}
			if (memcmp(buf, ptApiAck_nbx0200 + j, ack_size)) {
				ERROR("invallid ack %02x:%02x\n", buf[0], buf[1]);
				printk("ack count: %d\n", j);
				return -1;
			}
		}
                break;
	case VERSION_NBX0300:
		DPRINTF("VERSION: NBX0300\n");
		for (i = 0, j = 0; i < sizeof(ptApiCmd_nbx0300); i += cmd_size, j += ack_size) {
			if (( ret = i2c_master_send(cl, ptApiCmd_nbx0300 + i, cmd_size)) < 0) {
				ERROR("ptApiCmd_nbx0300 fail.  (ret = %d)\n", ret);
				return -1;
			}
			msleep(20);
			if (( ret = i2c_master_recv(cl, buf, ack_size)) < 0) {
				ERROR("ptApiCmd_nbx0300 fail.  (ret = %d)\n", ret);
				return -1;
			}
			if (memcmp(buf, ptApiAck_nbx0300 + j, ack_size)) {
				ERROR("invallid ack %02x:%02x\n", buf[0], buf[1]);
				printk("ack count: %d\n", j);
				return -1;
			}
		}
                break;
	default:
		ERROR("VERSION unknown\n");
                break;
	}

	return 0;
}

static void
load_firmware(const struct firmware *firm, void *arg)
{
	struct a1026_platform_data *sdat;
	int i, r;
	struct i2c_client *cl;

	sdat = (struct a1026_platform_data *)arg;
	if (!sdat) {
		ERROR("sdat == NULL");
		return;
	}

	if (!sdat->client) {
		ERROR("sdat->client == NULL");
		return;
	}
	cl = sdat->client;

	if (!firm) {
		msleep(1000 * 1);
		request_firmware_nowait(THIS_MODULE, 1, DEV_NAME".bin", 
		    &cl->dev, GFP_KERNEL, sdat, load_firmware);
		return;
	}

	for (r = 0; r < 5; r++) {
		/* hardware reset */
		gpio_set_value(sdat->reset_pin, 0);
		msleep(10);
		gpio_set_value(sdat->reset_pin, 1);

		msleep(50);

		if (boot_command(cl) < 0)
			continue;

		for (i = 0; i < firm->size; i += 32) {
			int len;

			len = i + 32 > firm->size ? firm->size - i : 32;
			if (i2c_master_send(cl, &firm->data[i], len) < 0) {
				ERROR("i2c_master_send\n");
				continue;
			}
		}
		msleep(10);
		if (sync_command(cl) >= 0)
			break;

		DPRINTF("retry:\n");
	}
	if (r == 5)
		ERROR("firmware download failed.\n");
	else
		DPRINTF("firmware download done.\n");

	release_firmware(firm);

	load_parameters(cl);
	sdat->is_awake = 1;

	return;
}

static int __devinit
a1026_probe(struct i2c_client *cl, const struct i2c_device_id *id)
{
	struct a1026_platform_data *sdat;
	int ret = -1;	

	a1026_i2c = cl;
	sdat = cl->dev.platform_data;

	sdat->client = cl;
	sdat->fw_version = 0;

	/* initialize RESET/WAKE PIN */
	tegra_gpio_enable(sdat->reset_pin);
	tegra_gpio_enable(sdat->wake_pin);

	if ((ret = gpio_request(sdat->reset_pin, DEV_NAME)) < 0) {
		ERROR("gpio_request\n");
		goto err0;
	}
	if ((ret = gpio_request(sdat->wake_pin, DEV_NAME)) < 0) {
		ERROR("gpio_request\n");
		goto err1;
	}
	if ((ret = gpio_direction_output(sdat->reset_pin, 1)) < 0) {
		ERROR("gpio_direction_output\n");
		goto err2;
	}
	if ((ret = gpio_direction_output(sdat->wake_pin, 1)) < 0) {
		ERROR("gpio_direction_output\n");
		goto err2;
	}

	/* initialize CLK26M_OUT */
	if ((sdat->clock = clk_get(&cl->dev, sdat->clk)) == NULL) {
		ERROR("clk_get %s\n", sdat->clk);
		goto err2;
	}
	if ((ret = clk_enable(sdat->clock))) {
		ERROR("clk_enable\n");
		goto err3;
	}

	/* request firmware to init */
	if ((ret = request_firmware_nowait(THIS_MODULE, 1, DEV_NAME".bin", 
	    &cl->dev, GFP_KERNEL, sdat, load_firmware))) {
		ERROR("request_firmware_nowait\n");
		goto err4;
	}

	return 0;
err4:
	clk_disable(sdat->clock);
err3:
	clk_put(sdat->clock);
err2:
	gpio_free(sdat->wake_pin);
err1:
	gpio_free(sdat->reset_pin);
err0:
	return ret;
}

static int __devexit
a1026_remove(struct i2c_client *cl)
{
	struct a1026_platform_data *sdat;
	sdat = cl->dev.platform_data;

	clk_disable(sdat->clock);
	clk_put(sdat->clock);
	gpio_free(sdat->wake_pin);
	gpio_free(sdat->reset_pin);

	return 0;
}

int
a1026_suspend_command(void)
{
	struct a1026_platform_data *sdat;
	sdat = a1026_i2c->dev.platform_data;

	if (sdat->is_awake) {
		sleep_command(a1026_i2c);
		msleep(120);
		clk_disable(sdat->clock);
		sdat->is_awake = 0;
	}

        return 0;
}
EXPORT_SYMBOL_GPL(a1026_suspend_command);

static int
a1026_suspend(struct i2c_client *cl, pm_message_t mesg)
{
	struct a1026_platform_data *sdat;
	sdat = cl->dev.platform_data;

	sleep_command(cl);
	msleep(120);
	clk_disable(sdat->clock);

        return 0;
}

static int
a1026_resume(struct i2c_client *cl)
{
	struct a1026_platform_data *sdat;
	sdat = cl->dev.platform_data;

	clk_enable(sdat->clock);
	gpio_set_value(sdat->wake_pin, 0);
	msleep(30);
	sync_command(cl);
	gpio_set_value(sdat->wake_pin, 1);
	load_parameters(cl);

        return 0;
}

int
a1026_resume_command(void)
{
	struct a1026_platform_data *sdat;
	sdat = a1026_i2c->dev.platform_data;

	if (!sdat->is_awake) {
		a1026_resume(a1026_i2c);
		sdat->is_awake = 1;
	}

        return 0;
}
EXPORT_SYMBOL_GPL(a1026_resume_command);

static struct i2c_driver a1026_i2c_driver = {
	.driver = {
		.name = DEV_NAME,
		.owner = THIS_MODULE,
	},
	.probe = a1026_probe,
	.remove = a1026_remove,
	/*.suspend = a1026_suspend,*/
	/*.resume = a1026_resume,*/
	.id_table = a1026_id,
};

struct early_suspend a1026_early_suspender;
static void a1026_early_suspend(struct early_suspend *h)
{
	a1026_suspend_command();
}
static void a1026_late_resume(struct early_suspend *h)
{
	a1026_resume_command();
}

static int
a1026_i2c_init(void)
{
	a1026_early_suspender.suspend = a1026_early_suspend;
	a1026_early_suspender.resume = a1026_late_resume;
	a1026_early_suspender.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	register_early_suspend(&a1026_early_suspender);

	return i2c_add_driver(&a1026_i2c_driver);
}

static void
a1026_i2c_exit(void)
{
	i2c_del_driver(&a1026_i2c_driver);
	return;
}


module_init(a1026_i2c_init);
module_exit(a1026_i2c_exit);
MODULE_LICENSE("GPL");

