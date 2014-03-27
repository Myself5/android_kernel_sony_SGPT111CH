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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#include <linux/nbx_ec_ipc.h>
#include <linux/nbx_ec_ipc_lidsw.h>

static struct input_dev* nbx_lidsw_ec_dev;

static void nbx_lidsw_ec_changed(void)
{
	int lidsw_state;

	if(nbx_lidsw_ec_dev == NULL) return;

	lidsw_state = nbx_ec_ipc_lidsw_get_state();

	if(0 <= lidsw_state) {
		input_report_switch(nbx_lidsw_ec_dev, SW_LID, lidsw_state);
		input_sync(nbx_lidsw_ec_dev);
	}
}

static int nbx_lidsw_ec_probe(struct platform_device* pdev)
{
	int ret = 0;

	nbx_lidsw_ec_dev = input_allocate_device();
	if(nbx_lidsw_ec_dev == NULL) {
		pr_err("nbx_lidsw_ec:input_allocate_device() failed.\n");
		ret = -ENOMEM;
		goto error_exit;
	}

	nbx_lidsw_ec_dev->name = "nbx_lidsw";
	set_bit(EV_SW, nbx_lidsw_ec_dev->evbit);
	set_bit(SW_LID, nbx_lidsw_ec_dev->swbit);

	ret = input_register_device(nbx_lidsw_ec_dev);
	if(ret != 0) {
		pr_err("nbx_lidsw_ec:input_register_device() failed. %d\n", ret);
		goto error_exit;
	}

	ret = nbx_ec_ipc_lidsw_register_callback(nbx_lidsw_ec_changed);
	if(ret != 0) {
		pr_err("nbx_lidsw_ec:nbx_ec_ipc_lidsw_register_callback() failed. %d\n", ret);
		goto error_exit;
	}

	nbx_lidsw_ec_changed(); /* initialize */

	return 0;

error_exit:
	input_unregister_device(nbx_lidsw_ec_dev);
	input_free_device(nbx_lidsw_ec_dev);
	nbx_ec_ipc_lidsw_unregister_callback(nbx_lidsw_ec_changed);
	nbx_lidsw_ec_dev = NULL;

	return ret;
}

static int nbx_lidsw_ec_remove(struct platform_device* pdev)
{
	input_unregister_device(nbx_lidsw_ec_dev);
	input_free_device(nbx_lidsw_ec_dev);
	nbx_ec_ipc_lidsw_unregister_callback(nbx_lidsw_ec_changed);
	nbx_lidsw_ec_dev = NULL;

	return 0;
}

static struct platform_driver nbx_lidsw_ec_driver = {
	.probe   = nbx_lidsw_ec_probe,
	.remove  = nbx_lidsw_ec_remove,
	.driver  = {
		.name = "nbx_lidsw",
	},
};

static struct platform_device* nbx_lidsw_ec_platform_dev;

static int __init nbx_lidsw_ec_init(void)
{
	int ret;

	nbx_lidsw_ec_dev = NULL;

	ret = platform_driver_register(&nbx_lidsw_ec_driver);
	if (ret < 0) {
		return ret;
	}

	nbx_lidsw_ec_platform_dev =
		platform_device_register_simple("nbx_lidsw", 0, NULL, 0);
	if (IS_ERR(nbx_lidsw_ec_platform_dev)) {
		ret = PTR_ERR(nbx_lidsw_ec_platform_dev);
		platform_driver_unregister(&nbx_lidsw_ec_driver);
		return ret;
	}

	return 0;


}
static void  __exit nbx_lidsw_ec_exit(void)
{
	platform_device_unregister(nbx_lidsw_ec_platform_dev);
	platform_driver_unregister(&nbx_lidsw_ec_driver);
}

module_init(nbx_lidsw_ec_init);
module_exit(nbx_lidsw_ec_exit);

MODULE_LICENSE("GPL");
