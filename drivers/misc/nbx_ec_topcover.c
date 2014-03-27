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
 * nbx_ec_topcover.c
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/switch.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/err.h>

#include <linux/nbx_ec_ipc.h>

static struct switch_dev topcover_switch_dev = {
	.name = "topcover",
};

#define EC_IPC_CID_TOPCOVER_REQUEST 0x32
#define EC_IPC_CID_TOPCOVER_EVENT 0x33

static struct workqueue_struct* nbx_ec_topcover_workqueue;
static struct delayed_work nbx_ec_topcover_work;
#define FAIL_RETRY_LIMIT 3
static int fail_retry_count = FAIL_RETRY_LIMIT;

static void nbx_ec_topcover_worker(struct work_struct* work)
{
	ssize_t ret;
	int topcover_state;

	enum {
		TOPCOVER_RES_RESULT = 0,
		NOOF_TOPCOVER_RES,
	};
	uint8_t res_buf[NOOF_TOPCOVER_RES];

#define TOPCOVER_RES_RESULT_OPEN 0
#define TOPCOVER_RES_RESULT_CLOSE 1

	ret = ec_ipc_send_request(EC_IPC_PID_TOPCOVER, EC_IPC_CID_TOPCOVER_REQUEST,
				NULL, 0,
				res_buf, sizeof(res_buf) );
	if(ret < (int)sizeof(res_buf)){
		if(0 < fail_retry_count--) {
			pr_err("nbx_ec_topcover:ec_ipc_send_request failed. %d ...retry\n", ret);
			queue_delayed_work(nbx_ec_topcover_workqueue, &nbx_ec_topcover_work,
					msecs_to_jiffies(1000));
		}
		else {
			pr_err("nbx_ec_topcover:ec_ipc_send_request failed. %d ...giveup\n", ret);
			fail_retry_count = FAIL_RETRY_LIMIT;
		}
		return;
	}
	fail_retry_count = FAIL_RETRY_LIMIT;

	topcover_state = (res_buf[TOPCOVER_RES_RESULT] == TOPCOVER_RES_RESULT_OPEN)? 1: 0;

	switch_set_state(&topcover_switch_dev, topcover_state);
}

static void nbx_ec_topcover_event(const uint8_t* buf, int size)
{
	if(buf == NULL) return;
	if(nbx_ec_topcover_workqueue == NULL) return;

	/* exec work, now */
	cancel_delayed_work(&nbx_ec_topcover_work);
	queue_delayed_work(nbx_ec_topcover_workqueue, &nbx_ec_topcover_work, 0);
}

static int nbx_ec_topcover_probe(struct platform_device* pdev)
{
	int ret = 0;

	ret = switch_dev_register(&topcover_switch_dev);
	if (ret < 0) {
		pr_err("nbx_ec_topcover:switch_dev_register() failed. %d\n", ret);
		goto error_exit;
	}

	nbx_ec_topcover_workqueue = create_singlethread_workqueue("nbx_ec_topcover_workqueue");
	if(nbx_ec_topcover_workqueue == NULL) {
		pr_err("nbx_ec_topcover:create_singlethread_workqueue() failed.\n");
		ret = -ENOMEM;
		goto error_exit;
	}

	INIT_DELAYED_WORK(&nbx_ec_topcover_work, nbx_ec_topcover_worker);

	ec_ipc_register_recv_event(EC_IPC_CID_TOPCOVER_EVENT, nbx_ec_topcover_event);
	queue_delayed_work(nbx_ec_topcover_workqueue, &nbx_ec_topcover_work, 0); /* 1st check */

	return 0;

error_exit:
	ec_ipc_unregister_recv_event(EC_IPC_CID_TOPCOVER_EVENT);
	if(nbx_ec_topcover_workqueue != NULL) {
		destroy_workqueue(nbx_ec_topcover_workqueue);
	}
	switch_dev_unregister(&topcover_switch_dev);
	nbx_ec_topcover_workqueue = NULL;

	return ret;
}

static int nbx_ec_topcover_remove(struct platform_device* pdev)
{
	ec_ipc_unregister_recv_event(EC_IPC_CID_TOPCOVER_EVENT);

	if(nbx_ec_topcover_workqueue != NULL) {
		cancel_delayed_work(&nbx_ec_topcover_work);
		flush_workqueue(nbx_ec_topcover_workqueue);
		cancel_delayed_work(&nbx_ec_topcover_work);
		destroy_workqueue(nbx_ec_topcover_workqueue);
	}

	switch_dev_unregister(&topcover_switch_dev);
	nbx_ec_topcover_workqueue = NULL;

	return 0;
}

#ifdef CONFIG_PM

static int nbx_ec_topcover_suspend(struct platform_device* pdev, pm_message_t state)
{
	if(nbx_ec_topcover_workqueue != NULL) {
		cancel_delayed_work(&nbx_ec_topcover_work);
		flush_workqueue(nbx_ec_topcover_workqueue);
		cancel_delayed_work(&nbx_ec_topcover_work);
	}

	return 0;
}
static int nbx_ec_topcover_resume(struct platform_device* pdev)
{
	if(nbx_ec_topcover_workqueue != NULL) {
		queue_delayed_work(nbx_ec_topcover_workqueue, &nbx_ec_topcover_work, 0);
	}

	return 0;
}

#else /* CONFIG_PM */

static int nbx_ec_topcover_suspend(struct platform_device* pdev, pm_message_t state)
{ return 0; }
static int nbx_ec_topcover_resume(struct platform_device* pdev)
{ return 0; }

#endif /* CONFIG_PM */

static struct platform_driver nbx_ec_topcover_driver = {
	.probe   = nbx_ec_topcover_probe,
	.remove  = nbx_ec_topcover_remove,
	.suspend = nbx_ec_topcover_suspend,
	.resume  = nbx_ec_topcover_resume,
	.driver  = {
		.name = "nbx_topcover",
	},
};

static struct platform_device* nbx_ec_topcover_platform_dev;

static int __init nbx_ec_topcover_init(void)
{
	int ret;

	nbx_ec_topcover_workqueue = NULL;

	ret = platform_driver_register(&nbx_ec_topcover_driver);
	if (ret < 0) {
		return ret;
	}

	nbx_ec_topcover_platform_dev =
		platform_device_register_simple("nbx_topcover", 0, NULL, 0);
	if (IS_ERR(nbx_ec_topcover_platform_dev)) {
		ret = PTR_ERR(nbx_ec_topcover_platform_dev);
		platform_driver_unregister(&nbx_ec_topcover_driver);
		return ret;
	}

	return 0;


}
static void  __exit nbx_ec_topcover_exit(void)
{
	platform_device_unregister(nbx_ec_topcover_platform_dev);
	platform_driver_unregister(&nbx_ec_topcover_driver);
}

module_init(nbx_ec_topcover_init);
module_exit(nbx_ec_topcover_exit);

MODULE_LICENSE("GPL");
