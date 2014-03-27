/* 2011-06-10: File added and changed by Sony Corporation */
#ifndef _NBX_DOCK_H_
#define _NBX_DOCK_H_

#include <linux/switch.h>

struct nbx_dock_platform_data {
	int ac_state;
	struct switch_dev sdev;
	int dockdet_pin;
	struct delayed_work work;
};

#endif /* _NBX_DOCK_H_ */
