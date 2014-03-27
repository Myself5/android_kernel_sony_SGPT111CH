/* 2011-06-10: File added by Sony Corporation */
/*
 * drivers/misc/ril_pm.c
 *
 * Copyright (c) 2008-2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/percpu.h>

pid_t s_ril_pid = 0;

#if defined(CONFIG_PM)
//
// /sys/power/ril/notifier
//

wait_queue_head_t ril_ack_wait;
wait_queue_head_t ril_pm_notifier_wait;

int ril_pm_continue_ok;

struct kobject *ril_kobj;

const char* sys_ril_notifier;

static const char *STRING_PM_SUSPEND_PREPARE = "PM_SUSPEND_PREPARE";
static const char *STRING_PM_POST_SUSPEND    = "PM_POST_SUSPEND";
static const char *STRING_PM_CONTINUE        = "PM_CONTINUE";
static const char *STRING_PM_SIGNAL          = "PM_SIGNAL";


static void notify_ril(const char* notice);

// Reading blocks if the value is not available.
static ssize_t
ril_pm_notifier_show(struct kobject *kobj, struct kobj_attribute *attr,
                   char *buf)
{
    int nchar;

    // Block if the value is not available yet.
    if (! sys_ril_notifier)
    {
        printk(KERN_INFO "%s: blocking\n", __func__);
        wait_event_interruptible(ril_pm_notifier_wait, sys_ril_notifier);
    }

    // In case of false wakeup, return "".
    if (! sys_ril_notifier)
    {
        printk(KERN_INFO "%s: false wakeup, returning with '\\n'\n", __func__);
        nchar = sprintf(buf, "\n");
        return nchar;
    }

    // Return the value, and clear.
    printk(KERN_INFO "%s: returning with '%s'\n", __func__, sys_ril_notifier);
    nchar = sprintf(buf, "%s\n", sys_ril_notifier);
    sys_ril_notifier = NULL;
    return nchar;
}

// Writing is no blocking.
static ssize_t
ril_pm_notifier_store(struct kobject *kobj, struct kobj_attribute *attr,
                    const char *buf, size_t count)
{
    if (!strncmp(buf, STRING_PM_CONTINUE, strlen(STRING_PM_CONTINUE))) {
        // Wake up pm_notifier.
        printk(KERN_INFO "%s: PM_CONTINUE\n", __func__);
        ril_pm_continue_ok = 1;
        wake_up(&ril_ack_wait);
    }
    else if (!strncmp(buf, STRING_PM_SIGNAL, strlen(STRING_PM_SIGNAL))) {
        s_ril_pid = 0;
        sscanf(buf, "%*s %d", &s_ril_pid);
        printk(KERN_INFO "%s: RIL pid=%d\n", __func__, s_ril_pid);
    }
    else {
        printk(KERN_ERR "%s: wrong value '%s'\n", __func__, buf);
    }

    return count;
}


static struct kobj_attribute ril_pm_notifier_attribute =
       __ATTR(notifier, 0400, ril_pm_notifier_show, ril_pm_notifier_store);


//
// PM notifier
//

static void notify_ril(const char* notice)
{
    long timeout = HZ * 30;

    // In case daemon's pid is not reported, do not signal or wait.
    if (!s_ril_pid) {
        printk(KERN_ERR "%s: don't know RIL pid\n", __func__);
        return;
    }

    ril_pm_continue_ok = 0;

    // Notify RIL
    sys_ril_notifier = notice;
    wake_up(&ril_pm_notifier_wait);

    // Wait for the reply from GPS HAL.
    printk(KERN_INFO "%s: wait for RIL\n", __func__);
    if (wait_event_timeout(ril_ack_wait,
                   ril_pm_continue_ok, timeout) == 0) {
        printk(KERN_ERR "%s: timed out. RIL did not reply\n", __func__);
    }

    // Go back to the initial state.
    sys_ril_notifier = NULL;
}

int ril_pm_notifier(struct notifier_block *nb,
                      unsigned long event, void *nouse)
{
    printk(KERN_INFO "%s: start processing event=%lx\n", __func__, event);

    // Notify the event to RIL.
    switch (event) {
    case PM_SUSPEND_PREPARE:
        notify_ril(STRING_PM_SUSPEND_PREPARE);
        break;
    case PM_POST_SUSPEND:
        notify_ril(STRING_PM_POST_SUSPEND);
        break;
    default:
        printk(KERN_ERR "%s: unknown event %ld\n", __func__, event);
        return NOTIFY_DONE;
    }

    printk(KERN_INFO "%s: finished processing event=%ld\n", __func__, event);
    return NOTIFY_OK;
}
#endif


static int __init ril_pm_init(void)
{
    int ret = 0;
    printk(KERN_INFO "%s called\n", __func__);

    #if defined(CONFIG_PM)
    // Create /sys/power/ril/notifier.
    ril_kobj = kobject_create_and_add("ril", power_kobj);
    ret = sysfs_create_file(ril_kobj, &ril_pm_notifier_attribute.attr);
    if(ret) {
        printk(KERN_ERR "%s: entry with the given name already exists\n", __func__);
        return ret;
    }

    init_waitqueue_head(&ril_pm_notifier_wait);
    init_waitqueue_head(&ril_ack_wait);

    sys_ril_notifier = NULL;
    ril_pm_continue_ok = 0;

    // Register PM notifier.
    pm_notifier(ril_pm_notifier, 0);
    #endif

    return ret;
}

static void __exit ril_pm_deinit(void)
{
    printk(KERN_INFO "%s called\n", __func__);
}

module_init(ril_pm_init);
module_exit(ril_pm_deinit);
