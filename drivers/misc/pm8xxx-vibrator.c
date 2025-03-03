/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/vibrator.h>

#include "../staging/android/timed_output.h"

#define VIB_DRV			0x4A

#define VIB_DRV_SEL_MASK	0xf8
#define VIB_DRV_SEL_SHIFT	0x03
#define VIB_DRV_EN_MANUAL_MASK	0xfc
#define VIB_DRV_LOGIC_SHIFT	0x2

#define VIB_MAX_LEVEL_mV	3100
#define VIB_MIN_LEVEL_mV	1200
//ASUS BSP TIM-2011.09.25++
#include <linux/microp_notify.h>
#include <linux/microp_api.h>
#include <linux/microp_pin_def.h>
#include <linux/delay.h>
//Tim++
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
//Tim--
static int vibrator_mode = 0;


enum define_vibrator_mode{
    a60k=0,
    P01,
    };
//ASUS BSP TIM-2011.09.25--
struct pm8xxx_vib {
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	spinlock_t lock;
	struct work_struct work;
	struct device *dev;
	const struct pm8xxx_vibrator_platform_data *pdata;
	int state;
	int level;
	u8  reg_vib_drv;
};

static struct pm8xxx_vib *vib_dev;
static ssize_t pm8xxx_level_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(tdev, struct pm8xxx_vib,
							 timed_dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vib->level);
}

static ssize_t pm8xxx_level_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct pm8xxx_vib *vib = container_of(tdev, struct pm8xxx_vib,
							 timed_dev);
	int val;
	int rc;

	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		pr_err("%s: error getting level\n", __func__);
		return -EINVAL;
	}

	if (val < VIB_MIN_LEVEL_mV / 100) {
		pr_err("%s: level %d not in range (%d - %d), using min.", __func__, val, VIB_MIN_LEVEL_mV / 100, VIB_MAX_LEVEL_mV / 100);
		val = VIB_MIN_LEVEL_mV / 100;
	} else if (val > VIB_MAX_LEVEL_mV / 100) {
		pr_err("%s: level %d not in range (%d - %d), using max.", __func__, val, VIB_MIN_LEVEL_mV / 100, VIB_MAX_LEVEL_mV / 100);
		val = VIB_MAX_LEVEL_mV / 100;
	}

	vib->level = val;

	return strnlen(buf, count);
}

static DEVICE_ATTR(level, S_IRUGO | S_IWUSR, pm8xxx_level_show, pm8xxx_level_store);

int pm8xxx_vibrator_config(struct pm8xxx_vib_config *vib_config)
{
	u8 reg = 0;
	int rc;

	if (vib_dev == NULL) {
		pr_err("%s: vib_dev is NULL\n", __func__);
		return -EINVAL;
	}

	if (vib_config->drive_mV) {
		if ((vib_config->drive_mV < VIB_MIN_LEVEL_mV) ||
			(vib_config->drive_mV > VIB_MAX_LEVEL_mV)) {
			pr_err("Invalid vibrator drive strength\n");
			return -EINVAL;
		}
	}

	reg = (vib_config->drive_mV / 100) << VIB_DRV_SEL_SHIFT;

	reg |= (!!vib_config->active_low) << VIB_DRV_LOGIC_SHIFT;

	reg |= vib_config->enable_mode;

	rc = pm8xxx_writeb(vib_dev->dev->parent, VIB_DRV, reg);
	if (rc)
		pr_err("%s: pm8xxx write failed: rc=%d\n", __func__, rc);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_vibrator_config);

/* REVISIT: just for debugging, will be removed in final working version */
static void __dump_vib_regs(struct pm8xxx_vib *vib, char *msg)
{
	u8 temp;

	dev_dbg(vib->dev, "%s\n", msg);

	pm8xxx_readb(vib->dev->parent, VIB_DRV, &temp);
	dev_dbg(vib->dev, "VIB_DRV - %X\n", temp);
}

static int pm8xxx_vib_read_u8(struct pm8xxx_vib *vib,
				 u8 *data, u16 reg)
{
	int rc;

	rc = pm8xxx_readb(vib->dev->parent, reg, data);
	if (rc < 0)
		dev_warn(vib->dev, "Error reading pm8xxx: %X - ret %X\n",
				reg, rc);

	return rc;
}

static int pm8xxx_vib_write_u8(struct pm8xxx_vib *vib,
				 u8 data, u16 reg)
{
	int rc;

	rc = pm8xxx_writeb(vib->dev->parent, reg, data);
	if (rc < 0)
		dev_warn(vib->dev, "Error writing pm8xxx: %X - ret %X\n",
				reg, rc);
	return rc;
}

static int pm8xxx_vib_set(struct pm8xxx_vib *vib, int on)
{
	int rc;
	u8 val;

	if (on) {
		val = vib->reg_vib_drv;
		val |= ((vib_dev->level << VIB_DRV_SEL_SHIFT) & VIB_DRV_SEL_MASK);
		rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;
	} else {
		val = vib->reg_vib_drv;
		val &= ~VIB_DRV_SEL_MASK;
		rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);
		if (rc < 0)
			return rc;
		vib->reg_vib_drv = val;
	}
	__dump_vib_regs(vib, "vib_set_end");

	return rc;
}

static void pm8xxx_vib_enable(struct timed_output_dev *dev, int value)
{
    int ret;
    struct pm8xxx_vib *vib = container_of(dev, struct pm8xxx_vib,
                timed_dev);
    unsigned long flags;
    if (vibrator_mode == a60k)
    {
        retry:
	        spin_lock_irqsave(&vib->lock, flags);
	        if (hrtimer_try_to_cancel(&vib->vib_timer) < 0) {
		        spin_unlock_irqrestore(&vib->lock, flags);
		        cpu_relax();
		        goto retry;
	        }
        
	    if (value == 0)
		    vib->state = 0;
	    else {
            if (value >=10000){
                vib_dev->level = 30;
                value = value - 10000;
                printk(DBGMSK_BL_G0"[VIBRATOR] Voltage=3V  msec=%d\n",value);
            }
            else if (value <10000){  
                if (value % 2 ){
                    vib_dev->level = 30;
                    value = value - 1;
                    printk(DBGMSK_BL_G0"[VIBRATOR] Voltage=3V  msec=%d\n",value);
                }
                else{
                    vib_dev->level = 20;
                    printk(DBGMSK_BL_G0"[VIBRATOR] Voltage=2V  msec=%d\n",value);
                }
            }
		    value = (value > vib->pdata->max_timeout_ms ?
				    vib->pdata->max_timeout_ms : value);
		    vib->state = 1;
		    hrtimer_start(&vib->vib_timer,
			        ktime_set(value / 1000, (value % 1000) * 1000000),
			        HRTIMER_MODE_REL);
	    }
	    spin_unlock_irqrestore(&vib->lock, flags);
	    schedule_work(&vib->work);
    }
    else if (vibrator_mode == P01)
    {
        if (value >=10000){
            value = value - 10000;
            printk(DBGMSK_BL_G0"[VIBRATOR] p02  msec=%d\n",value);
        }
        else if (value <10000){  
            printk(DBGMSK_BL_G0"[VIBRATOR] p02  msec=%d\n",value);
        }
        value = (value > vib->pdata->max_timeout_ms ?
        vib->pdata->max_timeout_ms : value);
        ret = AX_MicroP_setGPIOOutputPin(OUT_uP_VIB_EN,1);
        if (ret<0)
                pr_err("%s: Error set P01 vibrator pin \n", __func__);

        msleep(value);
        ret = AX_MicroP_setGPIOOutputPin(OUT_uP_VIB_EN,0);
        if (ret<0)
                pr_err("%s: Error set P01 vibrator pin \n", __func__);
    }
}

static void pm8xxx_vib_update(struct work_struct *work)
{
	struct pm8xxx_vib *vib = container_of(work, struct pm8xxx_vib,
					 work);

	pm8xxx_vib_set(vib, vib->state);
}

static int pm8xxx_vib_get_time(struct timed_output_dev *dev)
{
	struct pm8xxx_vib *vib = container_of(dev, struct pm8xxx_vib,
							 timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);
	} else
		return 0;
}

static enum hrtimer_restart pm8xxx_vib_timer_func(struct hrtimer *timer)
{
	struct pm8xxx_vib *vib = container_of(timer, struct pm8xxx_vib,
							 vib_timer);

	vib->state = 0;
	schedule_work(&vib->work);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int pm8xxx_vib_suspend(struct device *dev)
{
	struct pm8xxx_vib *vib = dev_get_drvdata(dev);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	/* turn-off vibrator */
	pm8xxx_vib_set(vib, 0);

	return 0;
}

static const struct dev_pm_ops pm8xxx_vib_pm_ops = {
	.suspend = pm8xxx_vib_suspend,
};
#endif

static int __devinit pm8xxx_vib_probe(struct platform_device *pdev)

{
	const struct pm8xxx_vibrator_platform_data *pdata =
						pdev->dev.platform_data;
	struct pm8xxx_vib *vib;
	u8 val;
	int rc;

	if (!pdata)
		return -EINVAL;

	if (pdata->level_mV < VIB_MIN_LEVEL_mV ||
			 pdata->level_mV > VIB_MAX_LEVEL_mV)
		return -EINVAL;

	vib = kzalloc(sizeof(*vib), GFP_KERNEL);
	if (!vib)
		return -ENOMEM;

	vib->pdata	= pdata;
	vib->level	= pdata->level_mV / 100;
	vib->dev	= &pdev->dev;

	spin_lock_init(&vib->lock);
	INIT_WORK(&vib->work, pm8xxx_vib_update);

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = pm8xxx_vib_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = pm8xxx_vib_get_time;
	vib->timed_dev.enable = pm8xxx_vib_enable;

	__dump_vib_regs(vib, "boot_vib_default");

	/*
	 * Configure the vibrator, it operates in manual mode
	 * for timed_output framework.
	 */
	rc = pm8xxx_vib_read_u8(vib, &val, VIB_DRV);
	if (rc < 0)
		goto err_read_vib;
	val &= ~VIB_DRV_EN_MANUAL_MASK;
	rc = pm8xxx_vib_write_u8(vib, val, VIB_DRV);
	if (rc < 0)
		goto err_read_vib;

	vib->reg_vib_drv = val;

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0)
		goto err_read_vib;

	rc = device_create_file(vib->timed_dev.dev, &dev_attr_level);
	if (rc < 0)
		goto err_read_vib;
	pm8xxx_vib_enable(&vib->timed_dev, pdata->initial_vibrate_ms);

	platform_set_drvdata(pdev, vib);

	vib_dev = vib;

	return 0;

err_read_vib:
	kfree(vib);
	return rc;
}
//ASUS BSP TIM-2011.09.25++

static int change_vibrator_mode(struct notifier_block *this, unsigned long event, void *ptr)
{
        switch (event) {
        case P01_ADD:
                printk("[ADD_vibrator] Disable vibrator of a60k \r\n");
                vibrator_mode = P01;

                return NOTIFY_DONE;
        case P01_REMOVE:
                printk("[REMOVE_vibrator] Enable vibrator of a60k \r\n");
                vibrator_mode = a60k;

                return NOTIFY_DONE;
        default:
                return NOTIFY_DONE;
        }
}
 
static struct notifier_block my_hs_notifier = {
        .notifier_call = change_vibrator_mode,
        .priority = VIBRATOR_MP_NOTIFY,
};
/*
//ASUS BSP TIM-2011.09.25--

//Tim++ change voltage of vibrator
#ifdef  CONFIG_PROC_FS
#define VIBRATOR_VOLTAGE  "driver/vibrator_voltage"
static struct proc_dir_entry *vibrator_voltage_entry;

#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/file.h>
static mm_segment_t oldfs;
static void initKernelEnv(void)
{
    oldfs = get_fs();
    set_fs(KERNEL_DS);
}

static void deinitKernelEnv(void)
{
    set_fs(oldfs);
}
    
static ssize_t vibrator_voltage_proc_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{    
    char messages[256];
    memset(messages, 0, sizeof(messages));
    if (len > 256)
    {
        len = 256;
    }
    if (copy_from_user(messages, buff, len))
    {
        return -EFAULT;
    }

    initKernelEnv();
    if(strncmp(messages, "2", 1) == 0)
    {
        vib_dev->level = 20;
        printk("[vibrator] vibrator_voltage = 2V\n");
    }
    else if(strncmp(messages, "3", 1) == 0)
    {
        vib_dev->level = 30;
        printk("[vibrator] vibrator_voltage = 3V\n");
    }
    
    deinitKernelEnv(); 
    return len;
}

static struct file_operations vibrator_voltage_proc_ops = {
    //.read = audio_debug_proc_read,
    .write = vibrator_voltage_proc_write,
};

static void create_vibrator_voltage_proc_file(void)
{
    printk("[Vibrator] create_vibrator_proc_file\n");
    vibrator_voltage_entry = create_proc_entry(VIBRATOR_VOLTAGE, 0666, NULL);
    if (vibrator_voltage_entry) {
        vibrator_voltage_entry->proc_fops = &vibrator_voltage_proc_ops;
    }
}

static void remove_vibrator_voltage_proc_file(void)
{
    extern struct proc_dir_entry proc_root;
    printk("[Vibrator] remove_vibrator_proc_file\n");   
    remove_proc_entry(VIBRATOR_VOLTAGE, &proc_root);
}
#endif //#ifdef CONFIG_PROC_FS
//Tim-- change voltage of vibrator
*/
static int __devexit pm8xxx_vib_remove(struct platform_device *pdev)
{
	struct pm8xxx_vib *vib = platform_get_drvdata(pdev);

	cancel_work_sync(&vib->work);
	hrtimer_cancel(&vib->vib_timer);
	timed_output_dev_unregister(&vib->timed_dev);
	platform_set_drvdata(pdev, NULL);
    //remove_vibrator_voltage_proc_file();
	kfree(vib);

	return 0;
}

static struct platform_driver pm8xxx_vib_driver = {
	.probe		= pm8xxx_vib_probe,
	.remove		= __devexit_p(pm8xxx_vib_remove),
	.driver		= {
		.name	= PM8XXX_VIBRATOR_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm8xxx_vib_pm_ops,
#endif
	},
};

static int __init pm8xxx_vib_init(void)
{
//ASUS BSP TIM-2011.09.25++
    register_microp_notifier(&my_hs_notifier);
    //create_vibrator_voltage_proc_file();
//ASUS BSP TIM-2011.09.25++
	return platform_driver_register(&pm8xxx_vib_driver);
}
module_init(pm8xxx_vib_init);

static void __exit pm8xxx_vib_exit(void)
{
	platform_driver_unregister(&pm8xxx_vib_driver);
    unregister_microp_notifier(&my_hs_notifier);
}
module_exit(pm8xxx_vib_exit);

MODULE_ALIAS("platform:" PM8XXX_VIBRATOR_DEV_NAME);
MODULE_DESCRIPTION("pm8xxx vibrator driver");
MODULE_LICENSE("GPL v2");
