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
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <nbx_accelerometer.h>

MODULE_LICENSE("GPL");

/*
 * LIS331DLH Registers
 */
#define REG_WHO_AM_I		0x0f
#define REG_CTRL_REG1		0x20
#define REG_CTRL_REG2		0x21
#define REG_CTRL_REG3		0x22
#define REG_CTRL_REG4		0x23
#define REG_CTRL_REG5		0x24
#define REG_HP_FILTER_RESET	0x25
#define REG_REFERENCE		0x26
#define REG_STATUS_REG		0x27
#define REG_OUTS		0xa8
#define REG_OUT_X		0x28
#define REG_OUT_Y		0x2a
#define REG_OUT_Z		0x2c
#define REG_OUT_X_L		0x28
#define REG_OUT_X_H		0x29
#define REG_OUT_Y_L		0x2a
#define REG_OUT_Y_H		0x2b
#define REG_OUT_Z_L		0x2c
#define REG_OUT_Z_H		0x2d
#define REG_INT1_CFG		0x30
#define REG_INT1_SOURCE		0x31
#define REG_INT1_THS		0x32
#define REG_INT1_DURATION	0x33
#define REG_INT2_CFG		0x34
#define REG_INT2_SOURCE		0x35
#define REG_INT2_THS		0x36
#define REG_INT2_DURATION	0x37


struct nbx_accel_data {
	struct input_dev	*input_dev;
	struct work_struct	work;
	struct semaphore	sem;
	s16			offset[3];
	s32			delay;
	int			open_count;
	s8			is_active;
};

static void set_power(s8 on);
static void nbx_accel_set_active(struct nbx_accel_data *accel, s8 is_active);
static void nbx_accel_start_measurement(struct nbx_accel_data *accel);
static void nbx_accel_stop_measurement(struct nbx_accel_data *accel);
static void timer_func(unsigned long arg);

static DEFINE_TIMER(g_polling_timer, timer_func, 0, 0);
static struct i2c_client *g_client;

#define MAKE_S16(a, b)	( (s16) ((((a) << 8) & 0xff00) + ((b) & 0xff)) )

#define MIN_DELAY	(16)

static s32 _i2c_read_byte(struct i2c_client *client, u8 command)
{
	s32 err;

	err = i2c_smbus_read_byte_data(client, command);
	if (err < 0) {
		pr_err("i2c_smbus_read_byte_data: %d\n", err);
	}
	return err;
}


static s32 _i2c_write_byte(struct i2c_client *client, u8 command, u8 value)
{
	s32 err;

	err = i2c_smbus_write_byte_data(client, command, value);
	if (err < 0) {
		pr_err("i2c_smbus_write_byte_data: %d\n", err);
	}
	return err;
}


#define DEF_REG_SHOW(reg) \
static ssize_t reg##_show(struct device *dev, struct device_attribute *attr, char* buf) \
{ \
	return snprintf(buf, 8, "%02x\n", (u8)_i2c_read_byte(g_client, REG_##reg)); \
}

#define DEF_REG_STORE(reg) \
static ssize_t reg##_store(struct device *dev, struct device_attribute *attr, \
		const char* buf, size_t count) \
{ \
	s32 err; \
	unsigned long ul = simple_strtoul(buf, NULL, 0); \
	if (ul > 0xff) { \
		return -EINVAL; \
	}\
	err = _i2c_write_byte(g_client, REG_##reg, ul); \
	return err < 0 ? err : count; \
}

#define DIAG_REG_RO(reg) \
DEF_REG_SHOW(reg) \
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, reg##_show, NULL)

#define DIAG_REG_RW(reg) \
DEF_REG_SHOW(reg) \
DEF_REG_STORE(reg) \
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, reg##_show, reg##_store)


DIAG_REG_RO(WHO_AM_I);
DIAG_REG_RW(CTRL_REG1);
DIAG_REG_RW(CTRL_REG2);
DIAG_REG_RW(CTRL_REG3);
DIAG_REG_RW(CTRL_REG4);
DIAG_REG_RW(CTRL_REG5);
DIAG_REG_RO(HP_FILTER_RESET);
DIAG_REG_RW(REFERENCE);
DIAG_REG_RO(STATUS_REG);
DIAG_REG_RO(OUT_X_L);
DIAG_REG_RO(OUT_X_H);
DIAG_REG_RO(OUT_Y_L);
DIAG_REG_RO(OUT_Y_H);
DIAG_REG_RO(OUT_Z_L);
DIAG_REG_RO(OUT_Z_H);
DIAG_REG_RW(INT1_CFG);
DIAG_REG_RO(INT1_SOURCE);
DIAG_REG_RW(INT1_THS);
DIAG_REG_RW(INT1_DURATION);
DIAG_REG_RW(INT2_CFG);
DIAG_REG_RO(INT2_SOURCE);
DIAG_REG_RW(INT2_THS);
DIAG_REG_RW(INT2_DURATION);


static ssize_t is_active_show(struct device *dev, struct device_attribute *attr, char* buf)
{
	 struct nbx_accel_data *accel = i2c_get_clientdata(g_client);
	 return snprintf(buf, 8, "%x\n", accel->is_active);
}
static DEVICE_ATTR(is_active, S_IRUGO, is_active_show, NULL);


static int initialize_device_files(struct device* dev)
{
	int i;
	int err = 0;
	static struct device_attribute* attrs[] = {
		&dev_attr_WHO_AM_I,
		&dev_attr_CTRL_REG1,
		&dev_attr_CTRL_REG2,
		&dev_attr_CTRL_REG3,
		&dev_attr_CTRL_REG4,
		&dev_attr_CTRL_REG5,
		&dev_attr_HP_FILTER_RESET,
		&dev_attr_REFERENCE,
		&dev_attr_STATUS_REG,
		&dev_attr_OUT_X_L,
		&dev_attr_OUT_X_H,
		&dev_attr_OUT_Y_L,
		&dev_attr_OUT_Y_H,
		&dev_attr_OUT_Z_L,
		&dev_attr_OUT_Z_H,
		&dev_attr_INT1_CFG,
		&dev_attr_INT1_SOURCE,
		&dev_attr_INT1_THS,
		&dev_attr_INT1_DURATION,
		&dev_attr_INT2_CFG,
		&dev_attr_INT2_SOURCE,
		&dev_attr_INT2_THS,
		&dev_attr_INT2_DURATION,
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


static void set_power(s8 on)
{
	if (on) {
		_i2c_write_byte(g_client, REG_CTRL_REG1, 0x2f);
	} else {
		_i2c_write_byte(g_client, REG_CTRL_REG1, 0x00);
	}
}


static void nbx_accel_set_active(struct nbx_accel_data *accel, s8 is_active)
{
	is_active = is_active ? 1 : 0;
	if (accel->is_active == is_active) {
		return ;
	}
	if (is_active) {
		/* POWER ON */
		set_power(1);
		nbx_accel_start_measurement(accel);
		accel->is_active = 1;
	} else {
		accel->is_active = 0;
		nbx_accel_stop_measurement(accel);
		/* POWER OFF */
		set_power(1);
	}
	return ;
}


static int read_data(struct nbx_accel_data *accel, s32* px, s32* py, s32* pz)
{
	s32 err;
	s32 x, y, z;
	s8 values[6];

	err = i2c_smbus_read_i2c_block_data(g_client, REG_OUTS, sizeof values, values);
	if (err < 0) {
		goto out;
	}
	x = MAKE_S16(values[1], values[0]) / 16;
	y = MAKE_S16(values[3], values[2]) / 16;
	z = MAKE_S16(values[5], values[4]) / 16;

/*
 * Swap the axes if necessory
 */
#if defined(CONFIG_INPUT_NBX_ACC_AXES_XZY)
	{
		s32 t;
		t = y;
		y = z;
		z = t;
	}
#elif defined(CONFIG_INPUT_NBX_ACC_AXES_YZX)
	{
		s32 t;
		t = x;
		x = y;
		y = z;
		z = t;
	}
#elif defined(CONFIG_INPUT_NBX_ACC_AXES_YXZ)
	{
		s32 t;
		t = x;
		x = y;
		y = t;
	}
#elif defined(CONFIG_INPUT_NBX_ACC_AXES_ZXY)
	{
		s32 t;
		t = x;
		x = z;
		z = y;
		y = t;
	}
#elif defined(CONFIG_INPUT_NBX_ACC_AXES_ZYX)
	{
		s32 t;
		t = x;
		x = z;
		z = t;
	}
#endif

	x += accel->offset[0];
	y += accel->offset[1];
	z += accel->offset[2];

/*
 * Change the direction of axes if necessary
 */
#ifdef CONFIG_INPUT_NBX_ACC_INVERSE_X_AXIS
	x = -x;
#endif
#ifdef CONFIG_INPUT_NBX_ACC_INVERSE_Y_AXIS
	y = -y;
#endif
#ifdef CONFIG_INPUT_NBX_ACC_INVERSE_Z_AXIS
	z = -z;
#endif

	*px = x;
	*py = y;
	*pz = z;
	err = 0;
out:
	return err;
}


static void work_func(struct work_struct *work)
{
	s32 err;
	s32 x,y,z;
	s32 data;
	struct nbx_accel_data *accel = i2c_get_clientdata(g_client);

	err = data = _i2c_read_byte(g_client, REG_STATUS_REG);
	if (err < 0) {
		goto out;
	}
	if ((data & 0x08) == 0) {
		goto out;
	}

	err = read_data(accel, &x, &y, &z);
	if (err < 0) {
		goto out;
	}
	input_report_abs(accel->input_dev, ABS_X, x);
	input_report_abs(accel->input_dev, ABS_Y, y);
	input_report_abs(accel->input_dev, ABS_Z, z);
	input_sync(accel->input_dev);
out:
	return ;
}


static void timer_func(unsigned long arg)
{
	struct nbx_accel_data *accel = (struct nbx_accel_data*)arg;

	if (!work_pending(&accel->work)) {
		schedule_work(&accel->work);
	}

	g_polling_timer.expires = jiffies + msecs_to_jiffies(accel->delay);
	g_polling_timer.data = (unsigned long)accel;
	add_timer(&g_polling_timer);
}

static void nbx_accel_start_measurement(struct nbx_accel_data *accel)
{
	pr_debug("called\n");
	_i2c_write_byte(g_client, REG_CTRL_REG1, 0x2f);
	_i2c_write_byte(g_client, REG_CTRL_REG4, 0x80);
	g_polling_timer.expires = jiffies + msecs_to_jiffies(1);
	g_polling_timer.data = (unsigned long)accel;
	add_timer(&g_polling_timer);
}

static void nbx_accel_stop_measurement(struct nbx_accel_data *accel)
{
	pr_debug("called\n");
	del_timer_sync(&g_polling_timer);

	cancel_work_sync(&accel->work);
}


static int nbx_accel_open(struct inode *inode, struct file* filp)
{
	int retval;
	struct nbx_accel_data *accel = i2c_get_clientdata(g_client);

	retval = 0;
	if (down_interruptible(&accel->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	pr_debug("called: %d\n", accel->open_count);
	{
		accel->open_count++;
		if (accel->open_count == 1) {
			accel->delay = MIN_DELAY;
		}
	}
	up(&accel->sem);
out:
	return retval;
}


static int nbx_accel_release(struct inode *inode, struct file* filp)
{
	int retval;
	struct nbx_accel_data *accel = i2c_get_clientdata(g_client);

	retval = 0;
	if (down_interruptible(&accel->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	pr_debug("called: %d\n", accel->open_count);
	{
		accel->open_count--;
		if (accel->open_count < 1) {
			accel->open_count = 0;
			nbx_accel_set_active(accel, 0);
		}
	}
	up(&accel->sem);
out:
	return retval;
}


static long nbx_accel_ioctl(
		struct file* filp, unsigned int cmd, unsigned long arg)
{
	struct nbx_accel_data *accel = i2c_get_clientdata(g_client);
	int	 retval;

	retval = -ENOTTY;
	if (_IOC_TYPE(cmd) != NBX_ACCELEROMETER_IOC_MAGIC) {
		goto out;
	}
	if (_IOC_NR(cmd) >= NBX_ACCELEROMETER_IOC_MAXNR) {
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

	if (down_interruptible(&accel->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}

	{
		s8 prev;
		s32 n;
		s16 data[3];
		switch (cmd) {
			case NBX_ACCELEROMETER_IOC_GET_ACTIVE:
				n = accel->is_active;
				pr_debug("NBX_ACCELEROMETER_IOC_GET_ACTIVE: %d\n", n);
				retval = copy_to_user(
							(void __user*)arg, &n, sizeof(n));
				if (retval != 0) {
					goto out_locked;
				}
				break;
			case NBX_ACCELEROMETER_IOC_SET_ACTIVE:
				retval = copy_from_user(
							&n, (void __user*)arg, sizeof(n));
				if (retval != 0) {
					goto out_locked;
				}
				pr_debug("NBX_ACCELEROMETER_IOC_SET_ACTIVE: %d, %d\n",
							n, accel->is_active);
				nbx_accel_set_active(accel, n);
				break;
			case NBX_ACCELEROMETER_IOC_GET_OFS_DATA:
				memcpy(&data[0], accel->offset, sizeof(accel->offset));
				retval = copy_to_user(
							(void __user*)arg, data, sizeof(data));
				if (retval != 0) {
					goto out_locked;
				}
				pr_debug("NBX_ACCELEROMETER_IOC_GET_OFS_DATA: "
							"0x%04x, 0x%04x, 0x%04x\n",
							(u16)accel->offset[0],
							(u16)accel->offset[1],
							(u16)accel->offset[2]);
				break;
			case NBX_ACCELEROMETER_IOC_SET_OFS_DATA:
				retval = copy_from_user(
							data, (void __user*)arg, sizeof(data));
				if (retval != 0) {
					goto out_locked;
				}
				prev = accel->is_active;
				nbx_accel_set_active(accel, 0);
				memcpy(accel->offset, &data[0], sizeof(accel->offset));
				pr_debug("NBX_ACCELEROMETER_IOC_SET_OFS_DATA: "
							"0x%04x, 0x%04x, 0x%04x\n",
							(u16)accel->offset[0],
							(u16)accel->offset[1],
							(u16)accel->offset[2]);
				nbx_accel_set_active(accel, prev);
				break;
			case NBX_ACCELEROMETER_IOC_GET_DELAY:
				retval = copy_to_user(
							(void __user*)arg, &accel->delay, sizeof(accel->delay));
				if (retval != 0) {
					goto out_locked;
				}
				pr_debug("NBX_ACCELEROMETER_IOC_GET_DELAY: %d\n", accel->delay);
				break;
			case NBX_ACCELEROMETER_IOC_SET_DELAY:
				retval = copy_from_user(
							&n, (void __user*)arg, sizeof(n));
				if (retval != 0) {
					goto out_locked;
				}
				if (n < MIN_DELAY) {
					n = MIN_DELAY;
				}
				accel->delay = n;
				prev = accel->is_active;
				nbx_accel_set_active(accel, 0);
				pr_debug("NBX_ACCELEROMETER_IOC_SET_DELAY: %d\n", accel->delay);
				nbx_accel_set_active(accel, prev);
				break;
			default:
				goto out_locked;
		}
	}
	retval = 0;
out_locked:
	up(&accel->sem);

out:
	return retval;
}


static struct file_operations nbx_accel_fops = {
	.owner = THIS_MODULE,
	.open = nbx_accel_open,
	.release = nbx_accel_release,
	.unlocked_ioctl = nbx_accel_ioctl,
};

static struct miscdevice nbx_accel_dev = {
	MISC_DYNAMIC_MINOR,
	"nbx_accelerometer",
	&nbx_accel_fops,
};


static int nbx_accel_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	s32 data;
	int err;
	int retval;
	struct nbx_accel_data *accel;

	pr_info("probe: %s\n", client->name);
	retval = 0;

	data = _i2c_read_byte(client, REG_WHO_AM_I);
	if (data != 0x32) {
		pr_err("unknown device: %2x\n", data);
		retval = -ENODEV;
		goto err;
	}

	accel = kzalloc(sizeof *accel, GFP_KERNEL);
	if (accel == NULL) {
		pr_err("kzalloc()\n");
		retval = -ENOMEM;
		goto err_kzalloc;
	}

	init_MUTEX(&accel->sem);
	INIT_WORK(&accel->work, work_func);

	accel->input_dev = input_allocate_device();
	if (accel->input_dev == NULL) {
		pr_err("input_allocate_device()\n");
		retval = -ENOMEM;
		goto err_input_allocate_device;
	}

	accel->input_dev->name = "nbx_accelerometer";
	set_bit(EV_ABS, accel->input_dev->evbit);
	input_set_abs_params(accel->input_dev, ABS_X, -32768, 32767, 0, 0);
	input_set_abs_params(accel->input_dev, ABS_Y, -32768, 32767, 0, 0);
	input_set_abs_params(accel->input_dev, ABS_Z, -32768, 32767, 0, 0);

	err = input_register_device(accel->input_dev);
	if (err) {
		pr_err("input_alloc_device(): %d\n", err);
		retval = err;
		goto err_input_register_device;
	}

	err = misc_register(&nbx_accel_dev);
	if (err) {
		pr_err("misc_register(): %d\n", err);
		retval = err;
		goto err_misc_register;
	}

	i2c_set_clientdata(client, accel);
	g_client = client;

	err = initialize_device_files(&client->dev);
	if (err < 0) {
		pr_err("initialize_device_files (%d)\n", err);
	}

	return retval;

err_misc_register:
err_input_register_device:
	input_unregister_device(accel->input_dev);
err_input_allocate_device:
	kfree(accel);
err_kzalloc:
err:
	return retval;
}

static int nbx_accel_remove(struct i2c_client *client)
{
	struct nbx_accel_data *accel = i2c_get_clientdata(client);

	pr_debug("called\n");
	misc_deregister(&nbx_accel_dev);
	input_unregister_device(accel->input_dev);
	i2c_set_clientdata(client, NULL);
	g_client = NULL;
	kfree(accel);

	return 0;
}

#ifdef CONFIG_PM

static int nbx_accel_suspend(struct i2c_client* client, pm_message_t state)
{
	int retval;
	struct nbx_accel_data *accel = i2c_get_clientdata(g_client);
	pr_debug("called\n");

	retval = 0;
	if (down_interruptible(&accel->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	{
		if (accel->open_count > 0) {
			nbx_accel_stop_measurement(accel);
			set_power(0);
		}
	}
	up(&accel->sem);
out:
	return retval;
}

static int nbx_accel_resume(struct i2c_client* client)
{
	int retval;
	struct nbx_accel_data *accel = i2c_get_clientdata(g_client);

	pr_debug("called\n");
	retval = 0;
	if (down_interruptible(&accel->sem)) {
		retval = -ERESTARTSYS;
		goto out;
	}
	{
		if (accel->open_count > 0) {
			nbx_accel_set_active(accel, accel->is_active);
		}
	}
	up(&accel->sem);
out:
	return retval;
}

#endif /* CONFIG_PM */

static struct i2c_device_id nbx_accel_idtable[] = {
	{ "nbx_accelerometer", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, nbx_accel_idtable);

static struct i2c_driver nbx_accel_driver = {
	.driver = {
	.name = "nbx_accelerometer",
	},
	.probe = nbx_accel_probe,
	.remove = nbx_accel_remove,

#ifdef CONFIG_PM
	.suspend = nbx_accel_suspend,
	.resume = nbx_accel_resume,
#endif /* CONFIG_PM */

	.id_table = nbx_accel_idtable,
};

static int __init nbx_accel_driver_init(void)
{
	int res;

	res = i2c_add_driver(&nbx_accel_driver);
	return res;
}

static void __exit nbx_accel_driver_exit(void)
{
	i2c_del_driver(&nbx_accel_driver);
}

module_init(nbx_accel_driver_init);
module_exit(nbx_accel_driver_exit);

