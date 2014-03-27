/* 2011-06-10: File added and changed by Sony Corporation */
#ifndef _NBX03_LIMIT_VOLUME_H_
#define _NBX03_LIMIT_VOLUME_H_

struct volume_limit_device {
	const char	*name;
	struct device	*dev;
	int		index;
	int		state;
	struct mutex	mutex_lock;

	ssize_t	(*print_name)(struct volume_limit_device *sdev, char *buf);
	ssize_t	(*print_state)(struct volume_limit_device *sdev, char *buf);
	struct snd_soc_codec *codec;
        int lvolume;
        int rvolume;
};


int volume_limit_dev_register(struct snd_soc_codec *codec);
void volume_limit_dev_unregister(void);

#endif /*_NBX03_LIMIT_VOLUME_H_*/
