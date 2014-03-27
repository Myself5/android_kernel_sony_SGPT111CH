/* 2011-06-10: File added and changed by Sony Corporation */
/*
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

#include "tegra_soc.h"

#ifndef __LINUX_CHANNEL_SWITCH_H__
#define __LINUX_CHANNEL_SWITCH_H__

struct ext_audio_device {
	const char	*name;
	struct device	*dev;
	int		index;
	int		state;
	struct mutex	mutex_lock;

	ssize_t	(*print_name)(struct ext_audio_device *sdev, char *buf);
	ssize_t	(*print_state)(struct ext_audio_device *sdev, char *buf);
	struct snd_soc_codec *codec;
	int 		hp_state;
};

struct ext_audio_reg {
    u16 reg;
    int val;
};

extern int channel_switch_dev_register(struct device *dev);
extern void channel_switch_dev_unregister(void);
//extern void channel_switch_dev_unregister(struct ext_audio_device *sdev);

static inline int local_audio_get_state(struct ext_audio_device *sdev)
{
	return sdev->state;
}

extern void local_audio_set_state(struct ext_audio_device *sdev, int state);
extern int ext_audio_get_rotation(void);
extern void ext_audio_channel_switch(bool on_off);
extern void ext_audio_set_hp_state(int new_state);
extern int ext_audio_get_hp_state(void);

#endif /* __LINUX_CHANNEL_SWITCH_H__ */
