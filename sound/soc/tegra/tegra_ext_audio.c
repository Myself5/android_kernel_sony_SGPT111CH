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
#include <linux/delay.h>
#include "tegra_ext_audio.h"
#include "../codecs/wm8903.h"

//#define EXT_AUDIO_DEBUG

extern struct class *sound_class;

static struct ext_audio_device *channel_switch = NULL;

static atomic_t device_count;

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
                          char *buf)
{
	if (channel_switch->print_state) {
		int ret = channel_switch->print_state(channel_switch, buf);
		if (ret >= 0)
			return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", channel_switch->state);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
    int new_state;
    char *endp;
    
    new_state = simple_strtoul(buf, &endp, 0);
    if (*endp && !isspace(*endp))
        return -EINVAL;
    
    if (channel_switch->state != new_state) {
        mutex_lock(&channel_switch->mutex_lock);
        
        switch (new_state) {
        case 0:
            channel_switch->state = new_state;
            break;
        case 90:
            channel_switch->state = new_state;
            break;
        case 270:
            channel_switch->state = new_state;
            break;
        default:
            return -EINVAL;
        }
        mutex_unlock(&channel_switch->mutex_lock);

        if (!channel_switch->hp_state)
	        ext_audio_channel_switch(true);
    }
    
    return count;
}

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
                         char *buf)
{
	if (channel_switch->print_name) {
		int ret = channel_switch->print_name(channel_switch, buf);
		if (ret >= 0)
			return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%s\n", channel_switch->name);
}

static DEVICE_ATTR(state, (S_IRUSR|S_IRGRP) | (S_IWUSR|S_IWGRP), state_show, state_store);
static DEVICE_ATTR(name, (S_IRUSR|S_IRGRP) | S_IWUSR, name_show, NULL);

#ifdef EXT_AUDIO_DEBUG

static ssize_t debug_store(struct device *dev, struct device_attribute *attr,
                           const char *buf, size_t count)
{
    int reg;
    int val;

    sscanf(buf, "%x%x\n", &reg, &val);
    printk("reg: 0x%x, val: 0x%x\n", reg, val);

    snd_soc_write(channel_switch->codec, reg, val);
    
    return count;
}

static DEVICE_ATTR(debug, S_IRUGO | S_IWUGO, NULL, debug_store);
#endif

int ext_audio_get_rotation(void)
{
    return channel_switch->state;
}
EXPORT_SYMBOL_GPL(ext_audio_get_rotation);

void ext_audio_set_hp_state(int new_state)
{
        if (new_state != channel_switch->hp_state){
		mutex_lock(&channel_switch->mutex_lock);
		channel_switch->hp_state = new_state;
		mutex_unlock(&channel_switch->mutex_lock);
        }
}
EXPORT_SYMBOL_GPL(ext_audio_set_hp_state);

int ext_audio_get_hp_state(void)
{
    return channel_switch->hp_state;
}
EXPORT_SYMBOL_GPL(ext_audio_get_hp_state);


static void ext_audio_headphone_enable(void)
{
        int val;
        val |= (WM8903_HPL_ENA | WM8903_HPR_ENA);
        snd_soc_write(channel_switch->codec, WM8903_ANALOGUE_HP_0, val);
        val |= (WM8903_HPL_ENA_DLY | WM8903_HPR_ENA_DLY);
        snd_soc_write(channel_switch->codec, WM8903_ANALOGUE_HP_0, val);

        snd_soc_write(channel_switch->codec, WM8903_DC_SERVO_2, 0x2);
        snd_soc_write(channel_switch->codec, WM8903_DC_SERVO_0, 0xc);
        msleep(256);
        snd_soc_write(channel_switch->codec, WM8903_DC_SERVO_0, 0xc);
        msleep(8);

        val |= ((WM8903_HPL_ENA_OUTP | WM8903_HPR_ENA_OUTP));
        snd_soc_write(channel_switch->codec, WM8903_ANALOGUE_HP_0, val);
        val |= ((WM8903_HPL_RMV_SHORT | WM8903_HPR_RMV_SHORT));
        snd_soc_write(channel_switch->codec, WM8903_ANALOGUE_HP_0, val);
}

static void ext_audio_headphone_disable(void)
{
        int val = snd_soc_read(channel_switch->codec, WM8903_ANALOGUE_HP_0);
        val &= ~(WM8903_HPL_RMV_SHORT | WM8903_HPR_RMV_SHORT);
        snd_soc_write(channel_switch->codec, WM8903_ANALOGUE_HP_0, val);
        val &= ~(WM8903_HPL_ENA | WM8903_HPR_ENA | WM8903_HPL_ENA_DLY | WM8903_HPR_ENA_DLY
		 | WM8903_HPL_RMV_SHORT | WM8903_HPR_RMV_SHORT);
        snd_soc_write(channel_switch->codec, WM8903_ANALOGUE_HP_0, val);
}

enum {
    DAC_MONO = 0,
    DAC_MUX,
    SPK_AMP_L,
    SPK_AMP_R,
    SPK_MUX_L,
    SPK_MUX_R,
    HP_AMP_L,
    HP_AMP_R,
    HP_MUX_L,
    HP_MUX_R,
    ENUM_MAX,
};

static struct ext_audio_reg ext_audio_reg[] = {
    { .reg = WM8903_DAC_DIGITAL_1, .val = 0, },
    { .reg = WM8903_POWER_MANAGEMENT_6, .val = 0, },
    { .reg = WM8903_ANALOGUE_OUT3_LEFT, .val = 0, },
    { .reg = WM8903_ANALOGUE_OUT3_RIGHT, .val = 0, },
    { .reg = WM8903_ANALOGUE_SPK_MIX_LEFT_0, .val = 0, },
    { .reg = WM8903_ANALOGUE_SPK_MIX_RIGHT_0, .val = 0, },
    { .reg = WM8903_ANALOGUE_OUT1_LEFT, .val = 0, },
    { .reg = WM8903_ANALOGUE_OUT1_RIGHT, .val = 0, },
    { .reg = WM8903_ANALOGUE_LEFT_MIX_0, .val = 0, },
    { .reg = WM8903_ANALOGUE_RIGHT_MIX_0, .val = 0, },
    {}
};

void ext_audio_channel_switch(bool on_off)
{
    int i;

    /* get current value for DAC and amps */
    for (i = 0; i < ENUM_MAX; i++) {
       ext_audio_reg[i].val = snd_soc_read(channel_switch->codec, ext_audio_reg[i].reg);
    }
    /* mute headphone amp and speaker amp */
    ext_audio_reg[HP_AMP_L].val |= (WM8903_HPL_MUTE | WM8903_HPOUTLZC);
    ext_audio_reg[HP_AMP_R].val |= (WM8903_HPR_MUTE | WM8903_HPOUTRZC);

    snd_soc_write(channel_switch->codec, ext_audio_reg[HP_AMP_L].reg, ext_audio_reg[HP_AMP_L].val);
    snd_soc_write(channel_switch->codec, ext_audio_reg[HP_AMP_R].reg, ext_audio_reg[HP_AMP_R].val);

    if (!on_off) {
        /* set DAC to stereo */
        ext_audio_reg[DAC_MONO].val &= ~(WM8903_DAC_MONO);
        snd_soc_write(channel_switch->codec, ext_audio_reg[DAC_MONO].reg, ext_audio_reg[DAC_MONO].val);
        /* enable DACL and DACR */
        ext_audio_reg[DAC_MUX].val |= (WM8903_DACL_ENA | WM8903_DACR_ENA);
        snd_soc_write(channel_switch->codec, ext_audio_reg[DAC_MUX].reg, ext_audio_reg[DAC_MUX].val);
        msleep(2);
        /* unmute hp amps */
        ext_audio_reg[HP_AMP_L].val &= ~(WM8903_HPL_MUTE);
        ext_audio_reg[HP_AMP_R].val &= ~(WM8903_HPR_MUTE);
        snd_soc_write(channel_switch->codec, ext_audio_reg[HP_AMP_L].reg, ext_audio_reg[HP_AMP_L].val);
        snd_soc_write(channel_switch->codec, ext_audio_reg[HP_AMP_R].reg, ext_audio_reg[HP_AMP_R].val);
    } else {
            snd_soc_write(channel_switch->codec, 0x76, 0x00);
        if (!channel_switch->state) {
            /* set DAC to stereo */
            ext_audio_reg[DAC_MONO].val &= ~(WM8903_DAC_MONO);
            snd_soc_write(channel_switch->codec, ext_audio_reg[DAC_MONO].reg, ext_audio_reg[DAC_MONO].val);
            /* enable DACL and DACR */
            ext_audio_reg[DAC_MUX].val |= (WM8903_DACL_ENA | WM8903_DACR_ENA);
            snd_soc_write(channel_switch->codec, ext_audio_reg[DAC_MUX].reg, ext_audio_reg[DAC_MUX].val);
            msleep(2);
            /* mux speaker for stereo (L->L, R->R)*/
            ext_audio_reg[SPK_MUX_L].val = WM8903_DACL_TO_MIXSPKL;
            ext_audio_reg[SPK_MUX_R].val = WM8903_DACR_TO_MIXSPKR;
            snd_soc_write(channel_switch->codec, ext_audio_reg[SPK_MUX_L].reg, ext_audio_reg[SPK_MUX_L].val);
            snd_soc_write(channel_switch->codec, ext_audio_reg[SPK_MUX_R].reg, ext_audio_reg[SPK_MUX_R].val);
        } else {
        /* disable DACR */
            ext_audio_reg[DAC_MUX].val &= ~(WM8903_DACR_ENA);
            snd_soc_write(channel_switch->codec, ext_audio_reg[DAC_MUX].reg, ext_audio_reg[DAC_MUX].val);
            msleep(2);
        /* set DAC to mono */
            ext_audio_reg[DAC_MONO].val |= WM8903_DAC_MONO;
            snd_soc_write(channel_switch->codec, ext_audio_reg[DAC_MONO].reg, ext_audio_reg[DAC_MONO].val);
        /* mux speaker for mono (L->L, L->R) */
            ext_audio_reg[SPK_MUX_L].val = WM8903_DACL_TO_MIXSPKL;
            ext_audio_reg[SPK_MUX_R].val = WM8903_DACL_TO_MIXSPKR;
            snd_soc_write(channel_switch->codec, ext_audio_reg[SPK_MUX_L].reg, ext_audio_reg[SPK_MUX_L].val);
            snd_soc_write(channel_switch->codec, ext_audio_reg[SPK_MUX_R].reg, ext_audio_reg[SPK_MUX_R].val);
        }
        snd_soc_write(channel_switch->codec, 0x76, 0x10);
    }
}
EXPORT_SYMBOL_GPL(ext_audio_channel_switch);


void local_audio_set_state(struct ext_audio_device *sdev, int state)
{
	char name_buf[120];
	char state_buf[120];
	char *prop_buf;
	char *envp[3];
	int env_offset = 0;
	int length;

	if (sdev->state != state) {
		sdev->state = state;

		prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
		if (prop_buf) {
			length = name_show(sdev->dev, NULL, prop_buf);
			if (length > 0) {
				if (prop_buf[length - 1] == '\n')
					prop_buf[length - 1] = 0;
				snprintf(name_buf, sizeof(name_buf),
					"LOCAL_AUDIO_NAME=%s", prop_buf);
				envp[env_offset++] = name_buf;
			}
			length = state_show(sdev->dev, NULL, prop_buf);
			if (length > 0) {
				if (prop_buf[length - 1] == '\n')
					prop_buf[length - 1] = 0;
				snprintf(state_buf, sizeof(state_buf),
					"LOCAL_AUDIO_STATE=%s", prop_buf);
				envp[env_offset++] = state_buf;
			}
			envp[env_offset] = NULL;
			kobject_uevent_env(&sdev->dev->kobj, KOBJ_CHANGE, envp);
			free_page((unsigned long)prop_buf);
		} else {
			printk(KERN_ERR "out of memory in local_audio_set_state\n");
			kobject_uevent(&sdev->dev->kobj, KOBJ_CHANGE);
		}
	}
}
EXPORT_SYMBOL_GPL(local_audio_set_state);

int channel_switch_dev_register(struct device *dev)
{
	int ret;

	struct snd_soc_device *socdev = dev_get_drvdata(dev);
	channel_switch = kzalloc(sizeof(struct ext_audio_device), GFP_KERNEL);
	if (!channel_switch)
		return -ENOMEM;

	channel_switch->codec = socdev->card->codec;

	channel_switch->name = "rotation";
	channel_switch->hp_state = 2;
	channel_switch->index = atomic_inc_return(&device_count);
	channel_switch->dev = device_create(sound_class, NULL,
		MKDEV(0, channel_switch->index), NULL, channel_switch->name);
	if (IS_ERR(channel_switch->dev))
		return PTR_ERR(channel_switch->dev);

	mutex_init(&channel_switch->mutex_lock);
	ret = device_create_file(channel_switch->dev, &dev_attr_state);
	if (ret < 0)
		goto err_create_file_1;
	ret = device_create_file(channel_switch->dev, &dev_attr_name);
	if (ret < 0)
		goto err_create_file_2;

#ifdef EXT_AUDIO_DEBUG
	ret = device_create_file(channel_switch->dev, &dev_attr_debug);
	if (ret < 0){
        	device_remove_file(channel_switch->dev, &dev_attr_debug);
            goto err_create_file_2;
	}

#endif
	dev_set_drvdata(channel_switch->dev, &channel_switch);
	channel_switch->state = 0;

	return 0;

err_create_file_2:
	device_remove_file(channel_switch->dev, &dev_attr_state);
err_create_file_1:
	printk(KERN_ERR "local_audio: Failed to register driver %s\n", channel_switch->name);

	return ret;
}
EXPORT_SYMBOL_GPL(channel_switch_dev_register);

void channel_switch_dev_unregister(void)
{
	device_remove_file(channel_switch->dev, &dev_attr_name);
	device_remove_file(channel_switch->dev, &dev_attr_state);
#ifdef VOL_LIMIT_DEBUG
	device_remove_file(channel_switch->dev, &dev_attr_debug);
#endif
	dev_set_drvdata(channel_switch->dev, NULL);
	if(channel_switch){
		kfree(channel_switch);
	}
}
EXPORT_SYMBOL_GPL(channel_switch_dev_unregister);

MODULE_AUTHOR("SONY");
MODULE_DESCRIPTION("extended audio driver");
MODULE_LICENSE("GPL");
