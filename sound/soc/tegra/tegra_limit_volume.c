/* 2011-06-10: File added and changed by Sony Corporation */
/*
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

#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include "tegra_limit_volume.h"

//#define VOL_LIMIT_DEBUG

#define VOLUME_MASK		0x3F
#define LIMIT_VALUE		0x32
#define LEFT_VOLUME_REG	0x39
#define RIGHT_VOLUME_REG	0x3A

extern struct class *sound_class;

static struct volume_limit_device *volume_limit = NULL;

static atomic_t device_count;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
	if (volume_limit->print_state) {
		int ret = volume_limit->print_state(volume_limit, buf);
		if (ret >= 0)
			return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", volume_limit->state);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
    int new_state;
    int lval, rval;
    char *endp;
    char *vol_name = "Headphone Volume";
    int ret = 0;

    new_state = simple_strtoul(buf, &endp, 0);
    if (*endp && !isspace(*endp))
        return -EINVAL;
    
    mutex_lock(&volume_limit->mutex_lock);
    
    if (volume_limit->state != new_state) {
        volume_limit->state = new_state;
    }

    if (volume_limit->state) {
            lval = snd_soc_read(volume_limit->codec, LEFT_VOLUME_REG);
            rval = snd_soc_read(volume_limit->codec, RIGHT_VOLUME_REG);
            lval &= ~VOLUME_MASK;
            rval &= ~VOLUME_MASK;
            lval |= LIMIT_VALUE;
            rval |= LIMIT_VALUE;
            snd_soc_write(volume_limit->codec, LEFT_VOLUME_REG, lval);
            snd_soc_write(volume_limit->codec, RIGHT_VOLUME_REG, rval);
            volume_limit->lvolume = (lval & VOLUME_MASK);
            volume_limit->rvolume = (rval & VOLUME_MASK);

            ret = snd_soc_limit_volume(volume_limit->codec, vol_name, LIMIT_VALUE);
    }

    mutex_unlock(&volume_limit->mutex_lock);
    
    return count;
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
	if (volume_limit->print_name) {
		int ret = volume_limit->print_name(volume_limit, buf);
		if (ret >= 0)
			return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", volume_limit->name);
}

static DEVICE_ATTR(state, (S_IRUSR|S_IRGRP) | (S_IWUSR|S_IWGRP), state_show, state_store);
static DEVICE_ATTR(name, (S_IRUSR|S_IRGRP) | S_IWUSR, name_show, NULL);

#ifdef VOL_LIMIT_DEBUG

static ssize_t debug_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
        int val;
        char *endp;
        
        val = snd_soc_read(volume_limit->codec, LEFT_VOLUME_REG);
        val &= ~VOLUME_MASK;
        val |= simple_strtoul(buf, &endp, 0);
        if (*endp && !isspace(*endp))
                return -EINVAL;
        
        mutex_lock(&volume_limit->mutex_lock);

        snd_soc_write(volume_limit->codec, LEFT_VOLUME_REG, val);
        snd_soc_write(volume_limit->codec, RIGHT_VOLUME_REG, val);
        volume_limit->lvolume = (val & VOLUME_MASK);
        volume_limit->rvolume = (val & VOLUME_MASK);

        mutex_unlock(&volume_limit->mutex_lock);
    
        return count;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x\n", volume_limit->lvolume);
}

static DEVICE_ATTR(debug, S_IRUGO | S_IWUGO, debug_show, debug_store);
#endif

int volume_limit_dev_register(struct snd_soc_codec *codec)
{
	int ret;

	volume_limit = kzalloc(sizeof(struct volume_limit_device), GFP_KERNEL);                       
	if (!volume_limit)                                                                 
		return -ENOMEM;                                                      
                         
	volume_limit->codec = codec;

	volume_limit->name = "vol_limit";
	volume_limit->state = 0;
	volume_limit->index = atomic_inc_return(&device_count);
	volume_limit->dev = device_create(sound_class, NULL,
		MKDEV(0, volume_limit->index), NULL, volume_limit->name);
	if (IS_ERR(volume_limit->dev))
		return PTR_ERR(volume_limit->dev);

	mutex_init(&volume_limit->mutex_lock);
	ret = device_create_file(volume_limit->dev, &dev_attr_state);
	if (ret < 0)
		goto err_create_file_1;
	ret = device_create_file(volume_limit->dev, &dev_attr_name);
	if (ret < 0)
		goto err_create_file_2;

#ifdef VOL_LIMIT_DEBUG
	ret = device_create_file(volume_limit->dev, &dev_attr_debug);
	if (ret < 0){
        	device_remove_file(volume_limit->dev, &dev_attr_debug);
            goto err_create_file_2;
	}

#endif
	dev_set_drvdata(volume_limit->dev, &volume_limit);
	volume_limit->state = 0;

	return 0;
    
err_create_file_2:
	device_remove_file(volume_limit->dev, &dev_attr_state);
err_create_file_1:
	printk(KERN_ERR "volume_limit: Failed to register driver %s\n", volume_limit->name);
    
	return ret;
}
EXPORT_SYMBOL_GPL(volume_limit_dev_register);

void volume_limit_dev_unregister(void)
{
	device_remove_file(volume_limit->dev, &dev_attr_name);
	device_remove_file(volume_limit->dev, &dev_attr_state);
#ifdef VOL_LIMIT_DEBUG
	device_remove_file(volume_limit->dev, &dev_attr_debug);
#endif
	dev_set_drvdata(volume_limit->dev, NULL);

	if(volume_limit){
		kfree(volume_limit);
	}
}
EXPORT_SYMBOL_GPL(volume_limit_dev_unregister);

MODULE_AUTHOR("SONY");
MODULE_DESCRIPTION("volume limit driver");
MODULE_LICENSE("GPL");
