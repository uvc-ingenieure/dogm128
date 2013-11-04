/*
 *    Filename: dogm128fb.c
 *     Version: 0.1.0
 * Description: dogm128fb LCD framebuffer driver
 *     License: GPLv2
 *     Depends: dogm128fb
 *
 *      Author: Copyright (C) Miguel Ojeda Sandonis
 *        Date: 2006-10-31
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "dogm128.h"

#define dogm128fb_NAME "dogm128fb"

static struct fb_fix_screeninfo dogm128fb_fix __devinitdata = {
	.id = "dogm128fb",
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_MONO10,
	.xpanstep = 0,
	.ypanstep = 0,
	.ywrapstep = 0,
	.line_length = DOGM_RES_X / 8,
	.accel = FB_ACCEL_NONE,
};

static struct fb_var_screeninfo dogm128fb_var __devinitdata = {
	.xres = DOGM_RES_X,
	.yres = DOGM_RES_Y,
	.xres_virtual = DOGM_RES_X,
	.yres_virtual = DOGM_RES_Y,
	.bits_per_pixel = 1,
	.red = { 0, 1, 0 },
      	.green = { 0, 1, 0 },
      	.blue = { 0, 1, 0 },
	.left_margin = 0,
	.right_margin = 0,
	.upper_margin = 0,
	.lower_margin = 0,
	.vmode = FB_VMODE_NONINTERLACED,
};

static int dogm128fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	return vm_insert_page(vma, vma->vm_start,
		virt_to_page(dogm128_buffer));
}

static struct fb_ops dogm128fb_ops = {
	.owner = THIS_MODULE,
	.fb_read = fb_sys_read,
	.fb_write = fb_sys_write,
	.fb_fillrect = sys_fillrect,
	.fb_copyarea = sys_copyarea,
	.fb_imageblit = sys_imageblit,
	.fb_mmap = dogm128fb_mmap,
};

static int __devinit dogm128fb_probe(struct platform_device *device)
{
	int ret = -EINVAL;
 	struct fb_info *info = framebuffer_alloc(0, &device->dev);

	if (!info)
		goto none;

	info->screen_base = (char __iomem *) dogm128_buffer;
	info->screen_size = DOGM_RAM_SIZE;
	info->fbops = &dogm128fb_ops;
	info->fix = dogm128fb_fix;
	info->var = dogm128fb_var;
	info->pseudo_palette = NULL;
	info->par = NULL;
	info->flags = FBINFO_FLAG_DEFAULT;

	if (register_framebuffer(info) < 0)
		goto fballoced;

	platform_set_drvdata(device, info);

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
		info->fix.id);

	return 0;

fballoced:
	framebuffer_release(info);

none:
	return ret;
}

static int __devexit dogm128fb_remove(struct platform_device *device)
{
	struct fb_info *info = platform_get_drvdata(device);

	if (info) {
		unregister_framebuffer(info);
		framebuffer_release(info);
	}

	return 0;
}

static struct platform_driver dogm128fb_driver = {
	.probe	= dogm128fb_probe,
	.remove = __devexit_p(dogm128fb_remove),
	.driver = {
		.name	= dogm128fb_NAME,
	},
};

static struct platform_device *dogm128fb_device;

static int __init dogm128fb_init(void)
{
	int ret = -EINVAL;

	/* dogm128fb_init() must be called first */
#if 0
	if (!dogm128_isinited()) {
		printk(KERN_ERR dogm128fb_NAME ": ERROR: "
			"dogm128fb is not initialized\n");
		goto none;
	}
#endif

	if (dogm128_enable()) {
		printk(KERN_ERR dogm128fb_NAME ": ERROR: "
			"can't enable dogm128fb refreshing (being used)\n");
		return -ENODEV;
	}

	ret = platform_driver_register(&dogm128fb_driver);

	if (!ret) {
		dogm128fb_device =
			platform_device_alloc(dogm128fb_NAME, 0);

		if (dogm128fb_device)
			ret = platform_device_add(dogm128fb_device);
		else
			ret = -ENOMEM;

		if (ret) {
			platform_device_put(dogm128fb_device);
			platform_driver_unregister(&dogm128fb_driver);
		}
	}


	return ret;
}

static void __exit dogm128fb_exit(void)
{
	platform_device_unregister(dogm128fb_device);
	platform_driver_unregister(&dogm128fb_driver);
	dogm128_disable();
}

module_init(dogm128fb_init);
module_exit(dogm128fb_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Max Holtzberg");
MODULE_DESCRIPTION("dogm128fb LCD framebuffer driver");
