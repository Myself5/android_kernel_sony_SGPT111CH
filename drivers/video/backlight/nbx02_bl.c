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
/*
 * linux/drivers/video/backlight/nbx02_bl.c
 *
 * NBX02 backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/nbx02_backlight.h>
#include <linux/slab.h>

struct off_keep_info {
	int index;
	struct backlight_device *bd;
};

static struct off_keep_info ok_info[2];

struct nbx02_bl_data {
	struct {
		struct pwm_device	*pwm;
		int flags;
		int off_keeper;
		int duty;

#define BL_FLAGS_KEEP_TIMER	(1 << 0)
#define BL_FLAGS_PWM_ENABLE	(1 << 1)

		struct timer_list timer;
	} panel[2];
	int expire_time;
	struct device		*dev;
	unsigned int		period;
	int			(*notify)(struct device *,
					  int brightness);
};

/*
** conversion table
** from brightness(0-255) to backlight pwm duty ratio(0-1000)
*/
static const int bl_tbl[256] = {
   28,   28,   28,   29,   29,   30,   30,   31,   31,   31,
   32,   32,   33,   33,   34,   34,   35,   35,   36,   36,
   37,   37,   38,   38,   39,   39,   40,   41,   41,   42,
   42,   43,   44,   44,   45,   45,   46,   47,   47,   48,
   49,   49,   50,   51,   52,   52,   53,   54,   55,   55,
   56,   57,   58,   59,   59,   60,   61,   62,   63,   64,
   65,   66,   67,   67,   68,   69,   70,   71,   72,   73,
   74,   76,   77,   78,   79,   80,   81,   82,   83,   85,
   86,   87,   88,   89,   91,   92,   93,   95,   96,   97,
   99,  100,  102,  103,  104,  106,  107,  109,  110,  112,
  114,  115,  117,  119,  120,  122,  124,  125,  127,  129,
  131,  133,  135,  136,  138,  140,  142,  144,  146,  148,
  151,  153,  155,  157,  159,  161,  164,  166,  168,  171,
  173,  176,  178,  181,  183,  186,  188,  191,  194,  197,
  199,  202,  205,  208,  211,  214,  217,  220,  223,  226,
  229,  233,  236,  239,  243,  246,  250,  253,  257,  260,
  264,  268,  271,  275,  279,  283,  287,  291,  295,  299,
  304,  308,  312,  317,  321,  326,  330,  335,  340,  345,
  349,  354,  359,  364,  370,  375,  380,  385,  391,  396,
  402,  408,  413,  419,  425,  431,  437,  443,  450,  456,
  462,  469,  476,  482,  489,  496,  503,  510,  517,  525,
  532,  540,  547,  555,  563,  571,  579,  587,  595,  604,
  612,  621,  629,  638,  647,  656,  666,  675,  685,  694,
  704,  714,  724,  734,  745,  755,  766,  777,  788,  799,
  810,  821,  833,  845,  857,  869,  881,  894,  906,  919,
  932,  945,  958,  972,  986, 1000
};

static int nbx02_backlight_update_status(struct backlight_device *bl)
{
	struct nbx02_bl_data *pb = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;
	int max = bl->props.max_brightness;
	int index;
	int duty;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

	if (brightness == 0) {
		for (index = 0; index < 2; index++) {
			if (pb->panel[index].flags & BL_FLAGS_PWM_ENABLE) {
				pwm_config(pb->panel[index].pwm, 0, pb->period);
				pwm_disable(pb->panel[index].pwm);
				pb->panel[index].flags &= ~BL_FLAGS_PWM_ENABLE;
				pb->panel[index].duty = 0;
			}
		}
	} else {
		for (index = 0; index < 2; index++) {
			if (pb->panel[index].off_keeper) {
				duty = pb->period * bl_tbl[brightness] / 1000;
				if (!(pb->panel[index].flags & BL_FLAGS_PWM_ENABLE) ||
						(pb->panel[index].duty != duty)) {
					pwm_config(pb->panel[index].pwm, duty, pb->period);
					pb->panel[index].duty = duty;
				}
				if (!(pb->panel[index].flags & BL_FLAGS_PWM_ENABLE)) {
					if (!pwm_enable(pb->panel[index].pwm)) {
						pb->panel[index].flags |= BL_FLAGS_PWM_ENABLE;
					}
				}
			}
			else {
				if (pb->panel[index].flags & BL_FLAGS_PWM_ENABLE) {
					pwm_config(pb->panel[index].pwm, 0, pb->period);
					pwm_disable(pb->panel[index].pwm);
					pb->panel[index].flags &= ~BL_FLAGS_PWM_ENABLE;
					pb->panel[index].duty = 0;
				}
			}
		}
	}
	return 0;
}

static int nbx02_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops nbx02_backlight_ops = {
	.update_status	= nbx02_backlight_update_status,
	.get_brightness	= nbx02_backlight_get_brightness,
};

static ssize_t nbx02_backlight_show_expire_time(struct device *dev,
		struct device_attribute *attr,char *buf)
{
	struct nbx02_bl_data *pb = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", pb->expire_time);
}

static ssize_t nbx02_backlight_store_expire_time(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct backlight_device *bd = to_backlight_device(dev);
	struct nbx02_bl_data *pb = dev_get_drvdata(dev);
	unsigned long expire_time;

	rc = strict_strtoul(buf, 0, &expire_time);
	if (rc)
		return rc;

	rc = -ENXIO;
	mutex_lock(&bd->ops_lock);
	if (bd->ops) {
		if ((expire_time != 0) && (expire_time < 50 || expire_time > INT_MAX))
			rc = -EINVAL;
		else {
			pr_debug("nbx02_backlight: set expire time to %lu\n", expire_time);
			pb->expire_time = expire_time;
		}
		rc = count;
	}
	mutex_unlock(&bd->ops_lock);

	return rc;
}

void off_keep_timer_stop(struct backlight_device *bd)
{
	struct nbx02_bl_data *pb = dev_get_drvdata(&bd->dev);
	int index;

	for (index = 0; index < 2; index++) {
		if (pb->panel[index].flags & BL_FLAGS_KEEP_TIMER) {
			pr_debug("nbx02_backlight: off keep timer stop(%d)", index);
			del_timer_sync(&pb->panel[index].timer);
			pb->panel[index].off_keeper = 1;
			backlight_update_status(bd);
			pb->panel[index].flags &= ~BL_FLAGS_KEEP_TIMER;
		}
	}
}

static void off_keep_timer_handler(unsigned long *info_addr)
{
	struct off_keep_info *info = (struct off_keep_info *)info_addr;
	struct nbx02_bl_data *pb = dev_get_drvdata(&info->bd->dev);

	if (pb->panel[info->index].flags & BL_FLAGS_KEEP_TIMER) {
		pr_debug("nbx02_backlight: off keep timeup(%d)", info->index);
		pb->panel[info->index].off_keeper = 1;
		backlight_update_status(info->bd);
		pb->panel[info->index].flags &= ~BL_FLAGS_KEEP_TIMER;
	}
}

static ssize_t nbx02_backlight_show_off_keeper(struct device *dev,
		struct device_attribute *attr,char *buf,int index)
{
	struct nbx02_bl_data *pb = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", pb->panel[index].off_keeper);
}

static ssize_t nbx02_backlight_store_off_keeper(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count,int index)
{
	int rc;
	struct backlight_device *bd = to_backlight_device(dev);
	struct nbx02_bl_data *pb = dev_get_drvdata(dev);
	unsigned long off_keeper;
	unsigned long timeout;

	rc = strict_strtoul(buf, 0, &off_keeper);
	if (rc)
		return rc;

	rc = -ENXIO;
	mutex_lock(&bd->ops_lock);
	if (bd->ops) {
		pr_debug("nbx02_backlight: set off keeper%d to %lu\n", index, off_keeper);
		if (pb->panel[index].off_keeper != off_keeper) {
			if (off_keeper) {
				if (pb->panel[index].flags & BL_FLAGS_KEEP_TIMER) {
					pr_debug("nbx02_backlight: off keep timer stop(%d)", index);
					del_timer_sync(&pb->panel[index].timer);
					pb->panel[index].flags &= ~BL_FLAGS_KEEP_TIMER;
				}
			}
			else if (pb->expire_time != 0) {
				if (!(pb->panel[index].flags & BL_FLAGS_KEEP_TIMER)) {
					ok_info[index].bd = bd;

					init_timer(&pb->panel[index].timer);
					timeout = pb->expire_time / (1000 / HZ);
					pb->panel[index].timer.function = off_keep_timer_handler;
					pb->panel[index].timer.expires = jiffies + timeout;
					pb->panel[index].timer.data = (unsigned long)&ok_info[index];
					pr_debug("nbx02_backlight: off keep timer start(%d)=%lu",
									 index, timeout);
					add_timer(&pb->panel[index].timer);
					pb->panel[index].flags |= BL_FLAGS_KEEP_TIMER;
				}
			}
			pb->panel[index].off_keeper = off_keeper;
			backlight_update_status(bd);
		}
		else if (!off_keeper) {
			if (pb->expire_time != 0) {
				if (pb->panel[index].flags & BL_FLAGS_KEEP_TIMER) {
					ok_info[index].bd = bd;

					timeout = pb->expire_time / (1000 / HZ);
					pr_debug("nbx02_backlight: off keep timer restart(%d)=%lu",
									 index, timeout);
					mod_timer(&pb->panel[index].timer, jiffies + timeout);
				}
			}
			else {
				pr_debug("nbx02_backlight: off keep timer stop(%d)", index);
				del_timer_sync(&pb->panel[index].timer);
			}
			pb->panel[index].off_keeper = off_keeper;
			backlight_update_status(bd);
		}
		rc = count;
	}
	mutex_unlock(&bd->ops_lock);

	return rc;
}

static ssize_t nbx02_backlight_show_off_keeper0(struct device *dev,
		struct device_attribute *attr,char *buf)
{
	return nbx02_backlight_show_off_keeper(dev, attr, buf, 0);
}

static ssize_t nbx02_backlight_store_off_keeper0(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return nbx02_backlight_store_off_keeper(dev, attr, buf, count, 0);
}

static ssize_t nbx02_backlight_show_off_keeper1(struct device *dev,
		struct device_attribute *attr,char *buf)
{
	return nbx02_backlight_show_off_keeper(dev, attr, buf, 1);
}

static ssize_t nbx02_backlight_store_off_keeper1(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return nbx02_backlight_store_off_keeper(dev, attr, buf, count, 1);
}

static DEVICE_ATTR(expire_time_ms, 0644, nbx02_backlight_show_expire_time,
									 nbx02_backlight_store_expire_time);
static DEVICE_ATTR(0, 0644, nbx02_backlight_show_off_keeper0,
									 nbx02_backlight_store_off_keeper0);
static DEVICE_ATTR(1, 0644, nbx02_backlight_show_off_keeper1,
									 nbx02_backlight_store_off_keeper1);

static struct attribute *nbx02_bl_attributes[] = {
	&dev_attr_expire_time_ms.attr,
	&dev_attr_0.attr,
	&dev_attr_1.attr,
	NULL
};

static const struct attribute_group nbx02_bl_attr_group = {
	.attrs = nbx02_bl_attributes,
};

static int nbx02_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct platform_nbx02_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct nbx02_bl_data *pb;
	int index;
	int ret;

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->pwm[0].init) {
		ret = data->pwm[0].init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = kzalloc(sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->period = data->pwm[0].pwm_period_ns;
	pb->notify = data->pwm[0].notify;
	pb->dev = &pdev->dev;

	pb->panel[0].pwm = pwm_request(data->pwm[0].pwm_id, "backlight");
	if (IS_ERR(pb->panel[0].pwm)) {
		dev_err(&pdev->dev, "unable to request nbx02 for upper backlight\n");
		ret = PTR_ERR(pb->panel[0].pwm);
		goto err_pwm0;
	} else
		dev_dbg(&pdev->dev, "got nbx02 for upper backlight\n");

	pb->panel[1].pwm = pwm_request(data->pwm[1].pwm_id, "backlight");
	if (IS_ERR(pb->panel[1].pwm)) {
		dev_err(&pdev->dev, "unable to request nbx02 for lower backlight\n");
		ret = PTR_ERR(pb->panel[1].pwm);
		goto err_pwm1;
	} else
		dev_dbg(&pdev->dev, "got nbx02 for lower backlight\n");

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = data->pwm[0].max_brightness;
	bl = backlight_device_register(dev_name(&pdev->dev), &pdev->dev, pb,
				       &nbx02_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	ret = sysfs_create_group(&bl->dev.kobj, &nbx02_bl_attr_group);

	bl->props.brightness = data->pwm[0].dft_brightness;

	for (index = 0; index < 2; index++) {
		ok_info[index].index = index;
		pb->panel[index].off_keeper = 1;
		pb->panel[index].flags = 0;
		pb->panel[index].duty = 0;
	}
	pb->expire_time = 1000;

	nbx02_backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
	return 0;

err_bl:
	pwm_free(pb->panel[1].pwm);
err_pwm1:
	pwm_free(pb->panel[0].pwm);
err_pwm0:
	kfree(pb);
err_alloc:
	if (data->pwm[0].exit)
		data->pwm[0].exit(&pdev->dev);
	return ret;
}

static int nbx02_backlight_remove(struct platform_device *pdev)
{
	struct platform_nbx02_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct nbx02_bl_data *pb = dev_get_drvdata(&bl->dev);
	int index;

	sysfs_remove_group(&bl->dev.kobj, &nbx02_bl_attr_group);

	backlight_device_unregister(bl);
	for (index = 0; index < 2; index++) {
		if (pb->panel[index].flags & BL_FLAGS_PWM_ENABLE) {
			pwm_config(pb->panel[index].pwm, 0, pb->period);
			pwm_disable(pb->panel[index].pwm);
			pb->panel[index].flags &= ~BL_FLAGS_PWM_ENABLE;
			pb->panel[index].duty = 0;
		}
		pwm_free(pb->panel[index].pwm);
	}
	kfree(pb);
	if (data->pwm[0].exit)
		data->pwm[0].exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int nbx02_backlight_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct nbx02_bl_data *pb = dev_get_drvdata(&bl->dev);
	int index;

	if (pb->notify)
		pb->notify(pb->dev, 0);

	off_keep_timer_stop(bl);

	for (index = 0; index < 2; index++) {
		if (pb->panel[index].flags & BL_FLAGS_PWM_ENABLE) {
			pwm_config(pb->panel[index].pwm, 0, pb->period);
			pwm_disable(pb->panel[index].pwm);
			pb->panel[index].flags &= ~BL_FLAGS_PWM_ENABLE;
			pb->panel[index].duty = 0;
		}
	}
	return 0;
}

static int nbx02_backlight_resume(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);

	nbx02_backlight_update_status(bl);
	return 0;
}
#else
#define nbx02_backlight_suspend	NULL
#define nbx02_backlight_resume	NULL
#endif

static struct platform_driver nbx02_backlight_driver = {
	.driver		= {
		.name	= "nbx02_backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= nbx02_backlight_probe,
	.remove		= nbx02_backlight_remove,
	.suspend	= nbx02_backlight_suspend,
	.resume		= nbx02_backlight_resume,
};

static int __init nbx02_backlight_init(void)
{
	return platform_driver_register(&nbx02_backlight_driver);
}
module_init(nbx02_backlight_init);

static void __exit nbx02_backlight_exit(void)
{
	platform_driver_unregister(&nbx02_backlight_driver);
}
module_exit(nbx02_backlight_exit);

MODULE_DESCRIPTION("nbx02 Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:nbx02_backlight");

