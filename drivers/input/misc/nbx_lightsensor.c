/* 2011-06-10: File added and changed by Sony Corporation */
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/nbx_ec_ipc.h>
#include <nbx_lightsensor.h>

MODULE_LICENSE("GPL");

struct nbx_light_data {
	struct input_dev	*input_dev;
	struct work_struct	work;
	struct semaphore	sem;
	s32			delay;
	int			open_count;
	u16			value;
	volatile u16		is_available;
	int			last_frame_nr;
	s8			is_active;
	s8			is_first;
};

struct nbx_light_data *g_light_data;

static void nbx_light_set_active(struct nbx_light_data *light, s8 is_active);
static void nbx_light_start_measurement(struct nbx_light_data *light);
static void nbx_light_stop_measurement(struct nbx_light_data *light);
static void timer_func(unsigned long arg);

static DEFINE_TIMER(g_polling_timer, timer_func, 0, 0);

#define MAKE_U16(a, b) ( (u16) ((((a) << 8) & 0xff00) + ((b) & 0xff)) )

#define MIN_DELAY	(200)
#define EC_CID_READ_LIGHTSENSOR	0x50
#define EC_CID_SET_LIGHTSENSOR_GAIN 0x52


static ssize_t is_active_show(struct device *dev, struct device_attribute *attr, char* buf)
{
	return snprintf(buf, 8, "%x\n", g_light_data ? g_light_data->is_active : 0);
}
static DEVICE_ATTR(is_active, S_IRUGO, is_active_show, NULL);


static int initialize_device_files(struct device* dev)
{
	int i;
	int err = 0;
	static struct device_attribute* attrs[] = {
		&dev_attr_is_active,
	};
	for (i = 0; i < ARRAY_SIZE(attrs); ++i) {
		err = device_create_file(dev, attrs[i]);
		if (err) {
			pr_err("device_create_file: %d\n", err);
			break;
		}
	}
	return err;
}


static void nbx_light_set_active(struct nbx_light_data *light, s8 is_active)
{
	is_active = is_active ? 1 : 0;
	if (light->is_active == is_active) {
		return ;
	}
	if (is_active) {
		nbx_light_start_measurement(light);
		light->is_active = 1;
	} else {
		light->is_active = 0;
		nbx_light_stop_measurement(light);
	}
	return ;
}

static void gain_res_func(const uint8_t* buf, int size, void* private_data)
{
	struct nbx_light_data *light = (struct nbx_light_data*)private_data;

	pr_debug("%s: called\n", __FUNCTION__);
	if ( (buf == 0) || (size != 1) || (buf[0] == 1)) {
		pr_err("SET_LIGHTSENSOR_GAIN: EC communication error (%d)\n", size);
		goto out;
	}
	light->is_first = 0;
out:
	return ;
}

static int send_sensor_gain(struct nbx_light_data *light, u8 gain)
{
	int err;
	u8 cmd;

	pr_debug("%s: called\n", __FUNCTION__);
	cmd = gain;
	err = ec_ipc_send_request_async(EC_IPC_PID_LIGHTSENSOR,
			EC_CID_SET_LIGHTSENSOR_GAIN,
			&cmd, sizeof cmd, gain_res_func, (void*)light);

	return err;
}

static void data_res_func(const uint8_t* buf, int size, void* private_data)
{
	struct nbx_light_data *light = (struct nbx_light_data*)private_data;

	if ( (buf == 0) || (size != 4) ) {
		pr_err("EC communication error (%d)\n", size);
		goto out;
	}
	if (light->is_available == 0) {
		light->value = MAKE_U16(buf[3], buf[2]);
		light->is_available = 1;
	}
out:
	return ;
}

static int request_sensor_data(struct nbx_light_data *light)
{
	int err;

	pr_debug("%s: called\n", __FUNCTION__);
	err = ec_ipc_send_request_async(EC_IPC_PID_LIGHTSENSOR, EC_CID_READ_LIGHTSENSOR,
			NULL, 0, data_res_func, (void*)light);

	return err;
}

static int read_data(struct nbx_light_data *light, u16* pvalue)
{
	int retval;

	retval = 0;
	if (light->is_available) {
		*pvalue = light->value;
		light->is_available = 0;
		retval = 1;
	}

	return retval;
}

static void work_func(struct work_struct *work)
{
	int err;
	u16 data;
	struct nbx_light_data *light = g_light_data;

	/* check data */
	err = read_data(light, &data);
	if (err > 0) {
		input_report_abs(light->input_dev, ABS_X, data);
		input_sync(light->input_dev);
	}

	if (light->last_frame_nr >= 0) {
		ec_ipc_cancel_request(EC_IPC_PID_LIGHTSENSOR,
			EC_CID_READ_LIGHTSENSOR, light->last_frame_nr);
	}

	if (light->is_first) {
		err = send_sensor_gain(light, 1);
	} else {
		err = request_sensor_data(light);
	}
	light->last_frame_nr = err;
	if (err < 0) {
		pr_err("ec_ipc_send_request_async (%d)\n", err);
	}

	return ;
}


static void timer_func(unsigned long arg)
{
	struct nbx_light_data *light = (struct nbx_light_data*)arg;

	if (!work_pending(&light->work)) {
		schedule_work(&light->work);
	}

	g_polling_timer.expires = jiffies + msecs_to_jiffies(light->delay);
	g_polling_timer.data = (unsigned long)light;
	add_timer(&g_polling_timer);
}


static void nbx_light_start_measurement(struct nbx_light_data *light)
{
	pr_debug("called\n");

	light->is_available = 0;
	g_polling_timer.expires = jiffies + msecs_to_jiffies(1);
	g_polling_timer.data = (unsigned long)light;
	add_timer(&g_polling_timer);
	return ;
}

static void nbx_light_stop_measurement(struct nbx_light_data *light)
{
	pr_debug("called\n");
	del_timer_sync(&g_polling_timer);

	cancel_work_sync(&light->work);
}


static int nbx_light_open(struct inode *inode, struct file* filp)
{
	int err;
	int retval;
	struct nbx_light_data *light = g_light_data;

	retval = 0;
	if (down_interruptible(&light->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	pr_debug("called: %d\n", light->open_count);
	{
		light->open_count++;
		if (light->open_count == 1) {
			light->delay = MIN_DELAY;
			if (light->is_first) {
				err = send_sensor_gain(light, 1);
				if (err < 0) {
					pr_err("ec_ipc_send_request_async (%d)\n", err);
				}
			}
		}
	}
	up(&light->sem);
out:
	return retval;
}


static int nbx_light_release(struct inode *inode, struct file* filp)
{
	int retval;
	struct nbx_light_data *light = g_light_data;

	retval = 0;
	if (down_interruptible(&light->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	pr_debug("called: %d\n", light->open_count);
	{
		light->open_count--;
		if (light->open_count < 1) {
			light->open_count = 0;
			nbx_light_set_active(light, 0);
		}
	}
	up(&light->sem);
out:
	return retval;
}


static long nbx_light_ioctl(
		struct file* filp, unsigned int cmd, unsigned long arg)
{
	struct nbx_light_data *light = g_light_data;
	int	 retval;

	retval = -ENOTTY;
	if (_IOC_TYPE(cmd) != NBX_LIGHTSENSOR_IOC_MAGIC) {
		goto out;
	}
	if (_IOC_NR(cmd) >= NBX_LIGHTSENSOR_IOC_MAXNR) {
		goto out;
	}
	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (!access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto out;
		}
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (!access_ok(VERIFY_WRITE,
					(void __user *)arg, _IOC_SIZE(cmd))) {
			retval = -EFAULT;
			goto out;
		}
	}

	if (down_interruptible(&light->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}

	{
		s8 prev;
		s32 n;
		switch (cmd) {
			case NBX_LIGHTSENSOR_IOC_GET_ACTIVE:
				n = light->is_active;
				pr_debug("NBX_LIGHTSENSOR_IOC_GET_ACTIVE: %d\n", n);
				retval = copy_to_user(
							(void __user*)arg, &n, sizeof(n));
				if (retval != 0) {
					goto out_locked;
				}
				break;
			case NBX_LIGHTSENSOR_IOC_SET_ACTIVE:
				retval = copy_from_user(
							&n, (void __user*)arg, sizeof(n));
				if (retval != 0) {
					goto out_locked;
				}
				pr_debug("NBX_LIGHTSENSOR_IOC_SET_ACTIVE: %d, %d\n",
							n, light->is_active);
				nbx_light_set_active(light, n);
				break;
			case NBX_LIGHTSENSOR_IOC_GET_DELAY:
				retval = copy_to_user(
							(void __user*)arg, &light->delay, sizeof(light->delay));
				if (retval != 0) {
					goto out_locked;
				}
				pr_debug("NBX_LIGHTSENSOR_IOC_GET_DELAY: %d\n", light->delay);
				break;
			case NBX_LIGHTSENSOR_IOC_SET_DELAY:
				retval = copy_from_user(
							&n, (void __user*)arg, sizeof(n));
				if (retval != 0) {
					goto out_locked;
				}
				if (n < MIN_DELAY) {
					n = MIN_DELAY;
				}
				light->delay = n;
				prev = light->is_active;
				nbx_light_set_active(light, 0);
				pr_debug("NBX_LIGHTSENSOR_IOC_SET_DELAY: %d\n", light->delay);
				nbx_light_set_active(light, prev);
				break;
			default:
				goto out_locked;
		}
	}
	retval = 0;
out_locked:
	up(&light->sem);

out:
	return retval;
}


static struct file_operations nbx_light_fops = {
	.owner = THIS_MODULE,
	.open = nbx_light_open,
	.release = nbx_light_release,
	.unlocked_ioctl = nbx_light_ioctl,
};

static struct miscdevice nbx_light_dev = {
	MISC_DYNAMIC_MINOR,
	"nbx_lightsensor",
	&nbx_light_fops,
};

static int nbx_light_probe(struct platform_device *pdev)
{
	int err;
	int retval;
	struct nbx_light_data *light;

	retval = 0;
	light = kzalloc(sizeof *light, GFP_KERNEL);
	if (light == NULL) {
		pr_err("kzalloc()\n");
		retval = -ENOMEM;
		goto err_kzalloc;
	}

	light->last_frame_nr = -1;

	init_MUTEX(&light->sem);
	INIT_WORK(&light->work, work_func);

	light->input_dev = input_allocate_device();
	if (light->input_dev == NULL) {
		pr_err("input_allocate_device()\n");
		retval = -ENOMEM;
		goto err_input_allocate_device;
	}

	light->input_dev->name = "nbx_lightsensor";
	set_bit(EV_ABS, light->input_dev->evbit);
	input_set_abs_params(light->input_dev, ABS_X, 0, 65535, 0, 0);

	err = input_register_device(light->input_dev);
	if (err) {
		pr_err("input_alloc_device(): %d\n", err);
		retval = err;
		goto err_input_register_device;
	}

	err = misc_register(&nbx_light_dev);
	if (err) {
		pr_err("misc_register(): %d\n", err);
		retval = err;
		goto err_misc_register;
	}

	light->is_first = 1;
	g_light_data = light;

	err = initialize_device_files(nbx_light_dev.this_device);
	if (err < 0) {
		pr_err("initialize_device_files (%d)\n", err);
	}

	return retval;

err_misc_register:
err_input_register_device:
	input_unregister_device(light->input_dev);
err_input_allocate_device:
	kfree(light);
err_kzalloc:
	return retval;
}

static int nbx_light_remove(struct platform_device* pdev)
{
	struct nbx_light_data *light = g_light_data;

	pr_debug("called\n");
	input_unregister_device(light->input_dev);
	g_light_data = NULL;
	kfree(light);

	return 0;
}

#ifdef CONFIG_PM

static int nbx_light_suspend(struct platform_device* pdev, pm_message_t state)
{
	int retval;
	struct nbx_light_data *light = g_light_data;

	pr_debug("called\n");
	retval = 0;
	if (down_interruptible(&light->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	{
		if (light->open_count > 0) {
			nbx_light_stop_measurement(light);
		}
	}
	up(&light->sem);
out:
	return retval;
}

static int nbx_light_resume(struct platform_device* pdev)
{
	int retval;
	struct nbx_light_data *light = g_light_data;

	pr_debug("called\n");
	retval = 0;
	if (down_interruptible(&light->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	{
		if (light->open_count > 0) {
			nbx_light_set_active(light, light->is_active);
		}
		light->last_frame_nr = -1;
	}
	up(&light->sem);
out:
	return retval;
}

#endif /* CONFIG_PM */

static struct platform_driver nbx_light_driver = {
	.probe = nbx_light_probe,
	.remove = nbx_light_remove,
	.suspend = nbx_light_suspend,
	.resume = nbx_light_resume,
	.driver = {
		.name = "nbx_lightsensor",
	},
};

static struct platform_device* nbx_light_platform_dev;

static int __init nbx_light_driver_init(void)
{
	int retval;

	retval = platform_driver_register(&nbx_light_driver);
	if (retval < 0) {
		goto out;
	}

	nbx_light_platform_dev = platform_device_register_simple(
			"nbx_lightsensor", 0, NULL, 0);
	if (IS_ERR(nbx_light_platform_dev)) {
		retval = PTR_ERR(nbx_light_platform_dev);
		platform_driver_unregister(&nbx_light_driver);
	}
out:
	return retval;
}

static void __exit nbx_light_driver_exit(void)
{
	platform_device_unregister(nbx_light_platform_dev);
	platform_driver_unregister(&nbx_light_driver);
}

module_init(nbx_light_driver_init);
module_exit(nbx_light_driver_exit);

