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

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include <asm/irq.h>
#include <asm/io.h>

#include <mach/gpio.h>

#define TEGRA_GPIO_PU5 165

struct hook_sw_dev_t {
        struct input_dev *button_dev;
        struct work_struct work;
        struct timer_list timer;
        struct mutex	mutex_lock;
        int timer_debounce;	/* in msecs */
        bool is_headset;	/* default 1 for test */
        int irq;
        bool button_state;
};

static struct hook_sw_dev_t *hook_sw_dev;

void hook_switch_is_headset(bool is_headset)
{
        mutex_lock(&hook_sw_dev->mutex_lock);
        hook_sw_dev->is_headset = is_headset;
        mutex_unlock(&hook_sw_dev->mutex_lock);
}

EXPORT_SYMBOL_GPL(hook_switch_is_headset);

static void button_pressed(void)
{
        input_report_key(hook_sw_dev->button_dev, KEY_MEDIA, 1);
        input_sync(hook_sw_dev->button_dev);
        mutex_lock(&hook_sw_dev->mutex_lock);
        hook_sw_dev->button_state = 1;
        mutex_unlock(&hook_sw_dev->mutex_lock);
}

static void button_released(void)
{
        input_report_key(hook_sw_dev->button_dev, KEY_MEDIA, 0);
        input_sync(hook_sw_dev->button_dev);
        mutex_lock(&hook_sw_dev->mutex_lock);
        hook_sw_dev->button_state = 0;
        mutex_unlock(&hook_sw_dev->mutex_lock);
}

static void button_event(int is_press)
{
        if (!hook_sw_dev->is_headset) {
                if (hook_sw_dev->button_state) {
                        button_released();
                } else {
                        return;
                }
        } else {
                if (is_press && !hook_sw_dev->button_state) {
                        button_pressed();
                } else if (!is_press && hook_sw_dev->button_state){
                        button_released();
                }
        }
}

static void hook_sw_timer(unsigned long _data)
{
        struct hook_sw_dev_t *data = (struct hook_sw_dev_t *)_data;

        schedule_work(&data->work);
}

static irqreturn_t button_interrupt(int irq, void *dummy)
{
	if (hook_sw_dev->timer_debounce)
		mod_timer(&hook_sw_dev->timer,
                          jiffies + msecs_to_jiffies(hook_sw_dev->timer_debounce));

        return IRQ_HANDLED;
}

static void hook_switch_intr_work(struct work_struct *work)
{
        button_event(gpio_get_value(TEGRA_GPIO_PU5));
}

int tegra_hooksw_init(void)
{
        int error;
        int trigger = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

        hook_sw_dev = kzalloc(sizeof(struct hook_sw_dev_t), GFP_KERNEL);
        if (!hook_sw_dev)
                return -ENOMEM;

        hook_sw_dev->is_headset = 0;
        hook_sw_dev->button_state = 0;
        hook_sw_dev->timer_debounce = 100;

        mutex_init(&hook_sw_dev->mutex_lock);
        setup_timer(&hook_sw_dev->timer, hook_sw_timer, (unsigned long)hook_sw_dev);
        INIT_WORK(&hook_sw_dev->work, hook_switch_intr_work);

        hook_sw_dev->button_dev = input_allocate_device();
        if (!hook_sw_dev->button_dev) {
                printk(KERN_ERR "tegra_hookswitch.c: Not enough memory\n");
                error = -ENOMEM;
                goto err_free_irq;
        }

        error = gpio_request(TEGRA_GPIO_PU5, "hook_switch");
        if (error < 0) {
                printk("failed to request GPIO %d, error %d\n",
                       TEGRA_GPIO_PU5, error);
        }

        error = gpio_direction_input(TEGRA_GPIO_PU5);
        if (error < 0) {
                printk("failed to configure"
                       " direction for GPIO %d, error %d\n",
                       TEGRA_GPIO_PU5, error);
        }

        hook_sw_dev->button_dev->name = "hook_switch";
        set_bit(EV_KEY, hook_sw_dev->button_dev->evbit);
        set_bit(KEY_MEDIA, hook_sw_dev->button_dev->keybit);

        error = input_register_device(hook_sw_dev->button_dev);
        if (error) {
                printk(KERN_ERR "tegra_hookswitch.c: Failed to register device\n");
                goto err_free_dev;
        }

        hook_sw_dev->irq = gpio_to_irq(TEGRA_GPIO_PU5);
        if (hook_sw_dev->irq < 0) {
                error = hook_sw_dev->irq;
                printk("Unable to get irq number for GPIO %d, error %d\n",
                       TEGRA_GPIO_PU5, error);
        }

        if (request_irq(hook_sw_dev->irq, button_interrupt,
                        trigger, "hook_switch", hook_sw_dev)) {
                printk(KERN_ERR "tegra_hookswitch.c: Can't allocate irq \n");
                return -EBUSY;
        }

        return 0;

 err_free_dev:
        input_free_device(hook_sw_dev->button_dev);
 err_free_irq:
        free_irq(hook_sw_dev->irq, button_interrupt);
        kfree(hook_sw_dev);
        hook_sw_dev = NULL;

        return error;
}

void tegra_hooksw_exit(void)
{
        input_unregister_device(hook_sw_dev->button_dev);
        free_irq(hook_sw_dev->irq, button_interrupt);
        if (hook_sw_dev) {
                kfree(hook_sw_dev);
                hook_sw_dev = NULL;
        }
}

MODULE_AUTHOR("SONY");
MODULE_DESCRIPTION("hookswitch driver");
MODULE_LICENSE("GPL");
