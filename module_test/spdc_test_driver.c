/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * @file spdc_test_driver.c
 *
 * @brief Kernel Test module for MXC spdc framebuffer driver
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/mman.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <linux/device.h>
#include <linux/mxcfb.h>
#include <linux/mxcfb_epdc_kernel.h>

/* major number of device */
static int gMajor;
static struct class *spdc_tm_class;

static int spdc_test_open(struct inode * inode, struct file * filp)
{
	/* Open handle to actual spdc FB driver */
	printk("Opening spdc test handle\n");
	try_module_get(THIS_MODULE);

	return 0;
}

static int spdc_test_release(struct inode * inode, struct file * filp)
{
	/* Close handle to spdc FB driver */
	printk("Closing spdc test handle\n");
	module_put(THIS_MODULE);
	return 0;
}

static int spdc_test_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = -EINVAL;

	switch (cmd) {
	case MXCFB_SET_WAVEFORM_MODES:
		{
			struct mxcfb_waveform_modes modes;
			if (!copy_from_user(&modes, argp, sizeof(modes))) {
				mxc_spdc_fb_set_waveform_modes(&modes, NULL);
				ret = 0;
			}
			break;
		}
	case MXCFB_SET_TEMPERATURE:
		{
			int temperature;
			if (!get_user(temperature, (int32_t __user *) arg))
				ret =
				    mxc_spdc_fb_set_temperature(temperature,
					NULL);
			break;
		}
	case MXCFB_SET_AUTO_UPDATE_MODE:
		{
			u32 auto_mode = 0;
			if (!get_user(auto_mode, (__u32 __user *) arg))
				ret =
				    mxc_spdc_fb_set_auto_update(auto_mode,
					NULL);
			break;
		}
	case MXCFB_SET_UPDATE_SCHEME:
		{
			u32 update_scheme = 0;
			if (!get_user(update_scheme, (__u32 __user *) arg))
				ret =
				    mxc_spdc_fb_set_upd_scheme(update_scheme,
					NULL);
			break;
		}
	case MXCFB_SEND_UPDATE:
		{
			struct mxcfb_update_data upd_data;
			if (!copy_from_user(&upd_data, argp,
				sizeof(upd_data))) {
				ret = mxc_spdc_fb_send_update(&upd_data, NULL);
				if (ret == 0 && copy_to_user(argp, &upd_data,
					sizeof(upd_data)))
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}

			break;
		}
	case MXCFB_WAIT_FOR_UPDATE_COMPLETE:
		{
			struct mxcfb_update_marker_data upd_marker_data;
			if (!copy_from_user(&upd_marker_data, argp,
				sizeof(upd_marker_data))) {
				ret = mxc_spdc_fb_wait_update_complete(
					&upd_marker_data, NULL);
				if (copy_to_user(argp, &upd_marker_data,
					sizeof(upd_marker_data)))
					ret = -EFAULT;
			} else {
				ret = -EFAULT;
			}

			break;
		}

	case MXCFB_SET_PWRDOWN_DELAY:
		{
			int delay = 0;
			if (!get_user(delay, (__u32 __user *) arg))
				ret =
				    mxc_spdc_fb_set_pwrdown_delay(delay, NULL);
			break;
		}

	case MXCFB_GET_PWRDOWN_DELAY:
		{
			int pwrdown_delay = mxc_spdc_get_pwrdown_delay(NULL);
			if (put_user(pwrdown_delay,
				(int __user *)argp))
				ret = -EFAULT;
			ret = 0;
			break;
		}
	default:
		printk("Invalid ioctl for spdc test driver.  ioctl = 0x%x\n",
			cmd);
		break;
	}

	return ret;
}

static struct file_operations spdc_test_fops = {
	.open = spdc_test_open,
	.release = spdc_test_release,
	.unlocked_ioctl = spdc_test_ioctl,
};

static int __init spdc_test_init_module(void)
{
	struct device *temp_class;
	int error;

	/* register a character device */
	error = register_chrdev(0, "spdc_test", &spdc_test_fops);
	if (error < 0) {
		printk("spdc test driver can't get major number\n");
		return error;
	}
	gMajor = error;

	spdc_tm_class = class_create(THIS_MODULE, "spdc_test");
	if (IS_ERR(spdc_tm_class)) {
		printk(KERN_ERR "Error creating spdc test module class.\n");
		unregister_chrdev(gMajor, "spdc_test");
		return PTR_ERR(spdc_tm_class);
	}

	temp_class = device_create(spdc_tm_class, NULL,
				   MKDEV(gMajor, 0), NULL, "spdc_test");
	if (IS_ERR(temp_class)) {
		printk(KERN_ERR "Error creating spdc test class device.\n");
		class_destroy(spdc_tm_class);
		unregister_chrdev(gMajor, "spdc_test");
		return -1;
	}

	printk("spdc test Driver Module loaded\n");
	return 0;
}

static void spdc_test_cleanup_module(void)
{
	unregister_chrdev(gMajor, "spdc_test");
	device_destroy(spdc_tm_class, MKDEV(gMajor, 0));
	class_destroy(spdc_tm_class);

	printk("spdc test Driver Module Unloaded\n");
}


module_init(spdc_test_init_module);
module_exit(spdc_test_cleanup_module);

MODULE_AUTHOR("Freescale Semiconductor");
MODULE_DESCRIPTION("spdc test driver");
MODULE_LICENSE("GPL");
