/* 2011-06-10: File added by Sony Corporation */
/*
 * Copyright (c) 2011 Sony Corporation.
 * All rights reserved.
 */

#ifndef _HCI_EVENT_H_
#define _HCI_EVENT_H_

#include <linux/switch.h>

struct hci_event_platform_data {
#define TEGRA_HCI_STATE_OC	0
#define TEGRA_HCI_STATE_OH	1
#define TEGRA_HCI_STATE_NS	2
#define TEGRA_HCI_STATE_NUM	3

#define TEGRA_HCI_STATE_OC_MASK	(1 << TEGRA_HCI_STATE_OC)
#define TEGRA_HCI_STATE_OH_MASK	(1 << TEGRA_HCI_STATE_OH)
#define TEGRA_HCI_STATE_NS_MASK	(1 << TEGRA_HCI_STATE_NS)

	int hci_state;
	struct switch_dev sdev;
	int ocdet_pin;
	int pwren_pin;
	struct work_struct work;
};

#endif /* _HCI_EVENT_H_ */
