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
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

#include <linux/nbx_ec_ipc.h>

static int nbx_ec_powerkey_state;
static int nbx_ec_powerkey_state_reported;
static struct input_dev* nbx_ec_powerkey_dev;

static DECLARE_MUTEX(powerkey_mutex);

#define LOCKED_STATEMENT(mutex, statement)			\
	do {							\
		if(0 != down_interruptible(mutex)) break;	\
		do {						\
			statement;				\
		} while(0);					\
		up(mutex);					\
	} while(0)

#define EC_IPC_CID_POWERKEY_REQUEST 0x34
#define EC_IPC_CID_POWERKEY_EVENT 0x35
#define EC_IPC_CID_CHECK_ECINT_REQUEST 0x94

static struct workqueue_struct* nbx_ec_powerkey_workqueue;
static struct delayed_work nbx_ec_powerkey_work;
#define FAIL_RETRY_LIMIT 3
static int fail_retry_count = FAIL_RETRY_LIMIT;

static void nbx_ec_powerkey_worker(struct work_struct* work)
{
	ssize_t ret;
	int powerkey_state = -1;

	enum {
		POWERKEY_RES_RESULT = 0,
		NOOF_POWERKEY_RES,
	};

#define POWERKEY_RES_RESULT_UP 0
#define POWERKEY_RES_RESULT_DOWN 1

	enum {
		CHECK_ECINT_RES_WAKEUP = 0,
		CHECK_ECINT_RES_SHUTDOWN,
		NOOF_CHECK_ECINT_RES,
	};
	uint8_t res_buf[(NOOF_CHECK_ECINT_RES<NOOF_POWERKEY_RES)?NOOF_POWERKEY_RES:NOOF_CHECK_ECINT_RES];

#define CHECK_ECINT_RES_WAKEUP_POWERKEY (1 << 0)
#define CHECK_ECINT_RES_WAKEUP_LID      (1 << 1)
#define CHECK_ECINT_RES_WAKEUP_TOPCOVER (1 << 2)
#define CHECK_ECINT_RES_WAKEUP_SHUTREQ  (1 << 3)
#define CHECK_ECINT_RES_WAKEUP_AC       (1 << 4)

	if(nbx_ec_powerkey_dev == NULL) return;

	LOCKED_STATEMENT(&powerkey_mutex,
			powerkey_state = nbx_ec_powerkey_state;
		);

	if(powerkey_state < 0) {
		ret = ec_ipc_send_request(EC_IPC_PID_POWERKEY, EC_IPC_CID_POWERKEY_REQUEST,
					NULL, 0,
					res_buf, sizeof(res_buf) );
		if(ret < NOOF_POWERKEY_RES){
			if(0 < fail_retry_count--) {
				pr_warning("nbx_ec_powerkey:ec_ipc_send_request failed. %d ...retry\n", ret);
				queue_delayed_work(nbx_ec_powerkey_workqueue, &nbx_ec_powerkey_work,
						msecs_to_jiffies(1000));
			}
			else {
				pr_err("nbx_ec_powerkey:ec_ipc_send_request failed. %d ...giveup\n", ret);
				fail_retry_count = FAIL_RETRY_LIMIT;
			}
			return;
		}
		powerkey_state = (res_buf[POWERKEY_RES_RESULT] == POWERKEY_RES_RESULT_UP)? 0: 1;

		if(powerkey_state == 0 ) {
			ret = ec_ipc_send_request(EC_IPC_PID_POWERKEY, EC_IPC_CID_CHECK_ECINT_REQUEST,
						NULL, 0,
						res_buf, sizeof(res_buf) );
			if(ret < NOOF_CHECK_ECINT_RES){
				if(0 < fail_retry_count--) {
					pr_warning("nbx_ec_powerkey:ec_ipc_send_request failed. %d ...retry\n", ret);
					queue_delayed_work(nbx_ec_powerkey_workqueue, &nbx_ec_powerkey_work,
							msecs_to_jiffies(1000));
				}
				else {
					pr_err("nbx_ec_powerkey:ec_ipc_send_request failed. %d ...giveup\n", ret);
					fail_retry_count = FAIL_RETRY_LIMIT;
				}
				return;
			}
			if( (res_buf[CHECK_ECINT_RES_WAKEUP] & CHECK_ECINT_RES_WAKEUP_POWERKEY) != 0) {
				/* if powerkey off & ecint powerkey then on -> off */
				input_report_key(nbx_ec_powerkey_dev, KEY_POWER, 1);
				input_sync(nbx_ec_powerkey_dev);
				nbx_ec_powerkey_state_reported = 1;
			}

		}
	}
	fail_retry_count = FAIL_RETRY_LIMIT;

	if(nbx_ec_powerkey_state_reported != powerkey_state) {
		input_report_key(nbx_ec_powerkey_dev, KEY_POWER, powerkey_state);
		input_sync(nbx_ec_powerkey_dev);
	}
	nbx_ec_powerkey_state_reported = powerkey_state;
}

static void nbx_ec_powerkey_event(const uint8_t* buf, int size)
{
	enum {
		POWERKEY_EVENT_STATE = 0,
		NOOF_POWERKEY_EVENT,
	};
#define POWERKEY_EVENT_STATE_UP 0
#define POWERKEY_EVENT_STATE_DOWN 1

	if(buf == NULL) return;
	if(nbx_ec_powerkey_workqueue == NULL) return;
	if(size < NOOF_POWERKEY_EVENT) return;

	LOCKED_STATEMENT(&powerkey_mutex,
			nbx_ec_powerkey_state = (buf[POWERKEY_EVENT_STATE] == POWERKEY_EVENT_STATE_UP) ? 0 : 1;
		);

	/* exec work, now */
	cancel_delayed_work(&nbx_ec_powerkey_work);
	queue_delayed_work(nbx_ec_powerkey_workqueue, &nbx_ec_powerkey_work, 0);
}

static int nbx_ec_powerkey_probe(struct platform_device* pdev)
{
	int ret = 0;

	nbx_ec_powerkey_dev = input_allocate_device();
	if(nbx_ec_powerkey_dev == NULL) {
		pr_err("nbx_ec_powerkey:input_allocate_device() failed.\n");
		ret = -ENOMEM;
		goto error_exit;
	}

	nbx_ec_powerkey_dev->name = "nbx_powerkey";
	set_bit(EV_KEY, nbx_ec_powerkey_dev->evbit);
	set_bit(KEY_POWER, nbx_ec_powerkey_dev->keybit);

	ret = input_register_device(nbx_ec_powerkey_dev);
	if(ret != 0) {
		pr_err("nbx_ec_powerkey:input_register_device() failed. %d\n", ret);
		goto error_exit;
	}

	nbx_ec_powerkey_workqueue = create_singlethread_workqueue("nbx_ec_powerkey_workqueue");
	if(nbx_ec_powerkey_workqueue == NULL) {
		pr_err("nbx_ec_powerkey:create_singlethread_workqueue() failed.\n");
		ret = -ENOMEM;
		goto error_exit;
	}

	INIT_DELAYED_WORK(&nbx_ec_powerkey_work, nbx_ec_powerkey_worker);

	LOCKED_STATEMENT(&powerkey_mutex,
			nbx_ec_powerkey_state = 0;
		);
	nbx_ec_powerkey_state_reported = -1;
	ec_ipc_register_recv_event(EC_IPC_CID_POWERKEY_EVENT, nbx_ec_powerkey_event);
	queue_delayed_work(nbx_ec_powerkey_workqueue, &nbx_ec_powerkey_work, 0); /* 1st check */

	return 0;

error_exit:
	ec_ipc_unregister_recv_event(EC_IPC_CID_POWERKEY_EVENT);
	if(nbx_ec_powerkey_workqueue != NULL) {
		destroy_workqueue(nbx_ec_powerkey_workqueue);
	}
	input_unregister_device(nbx_ec_powerkey_dev);
	input_free_device(nbx_ec_powerkey_dev);
	nbx_ec_powerkey_dev = NULL;
	nbx_ec_powerkey_workqueue = NULL;

	return ret;
}

static int nbx_ec_powerkey_remove(struct platform_device* pdev)
{
	ec_ipc_unregister_recv_event(EC_IPC_CID_POWERKEY_EVENT);

	if(nbx_ec_powerkey_workqueue != NULL) {
		cancel_delayed_work(&nbx_ec_powerkey_work);
		flush_workqueue(nbx_ec_powerkey_workqueue);
		cancel_delayed_work(&nbx_ec_powerkey_work);
		destroy_workqueue(nbx_ec_powerkey_workqueue);
	}

	input_unregister_device(nbx_ec_powerkey_dev);
	input_free_device(nbx_ec_powerkey_dev);
	nbx_ec_powerkey_dev = NULL;
	nbx_ec_powerkey_workqueue = NULL;

	return 0;
}

#ifdef CONFIG_PM

static int nbx_ec_powerkey_suspend(struct platform_device* pdev, pm_message_t state)
{
	if(nbx_ec_powerkey_workqueue != NULL) {
		cancel_delayed_work(&nbx_ec_powerkey_work);
		flush_workqueue(nbx_ec_powerkey_workqueue);
		cancel_delayed_work(&nbx_ec_powerkey_work);
	}

	return 0;
}
static int nbx_ec_powerkey_resume(struct platform_device* pdev)
{
	LOCKED_STATEMENT(&powerkey_mutex,
			nbx_ec_powerkey_state = -1;
		);

	if(nbx_ec_powerkey_workqueue != NULL) {
		queue_delayed_work(nbx_ec_powerkey_workqueue, &nbx_ec_powerkey_work, 0);
	}

	return 0;
}

#else /* CONFIG_PM */

static int nbx_ec_powerkey_suspend(struct platform_device* pdev, pm_message_t state)
{ return 0; }
static int nbx_ec_powerkey_resume(struct platform_device* pdev)
{ return 0; }

#endif /* CONFIG_PM */

static struct platform_driver nbx_ec_powerkey_driver = {
	.probe   = nbx_ec_powerkey_probe,
	.remove  = nbx_ec_powerkey_remove,
	.suspend = nbx_ec_powerkey_suspend,
	.resume  = nbx_ec_powerkey_resume,
	.driver  = {
		.name = "nbx_powerkey",
	},
};

static struct platform_device* nbx_ec_powerkey_platform_dev;

static int __init nbx_ec_powerkey_init(void)
{
	int ret;

	nbx_ec_powerkey_dev = NULL;
	nbx_ec_powerkey_workqueue = NULL;

	ret = platform_driver_register(&nbx_ec_powerkey_driver);
	if (ret < 0) {
		return ret;
	}

	nbx_ec_powerkey_platform_dev =
		platform_device_register_simple("nbx_powerkey", 0, NULL, 0);
	if (IS_ERR(nbx_ec_powerkey_platform_dev)) {
		ret = PTR_ERR(nbx_ec_powerkey_platform_dev);
		platform_driver_unregister(&nbx_ec_powerkey_driver);
		return ret;
	}

	return 0;


}
static void  __exit nbx_ec_powerkey_exit(void)
{
	platform_device_unregister(nbx_ec_powerkey_platform_dev);
	platform_driver_unregister(&nbx_ec_powerkey_driver);
}

module_init(nbx_ec_powerkey_init);
module_exit(nbx_ec_powerkey_exit);

MODULE_LICENSE("GPL");
