/*
  dogm128.c

  Copyright Miguel Ojeda Sandonis, 2006
  Copyright Scott Ellis, 2010
  Copyright Max Holtzberg, 2012

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  dogm128 v0.1 loads and registers a spi driver for a AE DogM128 Display
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#include <mach/gpio.h>

#include "dogm128.h"

#define USER_BUFF_SIZE	(128)
#define DOGM_A0_PIN		(AT91_PIN_PB4)
#define DOGM_RESET_PIN	(AT91_PIN_PB5)

#define DOGM_CMD_SET_PAGE_ADDR	(0xb0)
#define DOGM_CMD_SET_COL_ADDR_H	(0x10)
#define DOGM_CMD_SET_COL_ADDR_L	(0x00)

const char this_driver_name[] = "dogm128";
const u8 dogm128_init_sequence[] = {0x40, 0xa1, 0xc0, 0xa6, 0xa2, 0x2f, 0xf8,
		0x00, 0x27, 0x81, 0x16, 0xac, 0x00, 0xaf/*, 0xa5*/};

struct dogm128_dev {
	struct semaphore spi_sem;
	struct spi_device *spi_device;
	char *user_buff;
	unsigned reset_pin;
	unsigned a0_pin;
};

static struct dogm128_dev dogm128_dev;

#define CONFIG_dogm128_RATE	(10)

static unsigned int dogm128_rate = CONFIG_dogm128_RATE;
module_param(dogm128_rate, uint, S_IRUGO);
MODULE_PARM_DESC(dogm128_rate,
	"Refresh rate (hertz)");

unsigned int dogm128_getrate(void)
{
	return dogm128_rate;
}


static inline void set_reset(int reset)
{
	at91_set_gpio_value(dogm128_dev.reset_pin, !reset);
}

static inline void set_a0(int a0)
{
	at91_set_gpio_value(dogm128_dev.a0_pin, a0);
}


/*
 * Update work
 */
unsigned char *dogm128_buffer;
static unsigned char *dogm128_cache;
static DEFINE_MUTEX(dogm128_mutex);
static unsigned char dogm128_updating;
static void dogm128_update(struct work_struct *delayed_work);
static struct workqueue_struct *dogm128_workqueue;
static DECLARE_DELAYED_WORK(dogm128_work, dogm128_update);

static void dogm128_queue(void)
{
	queue_delayed_work(dogm128_workqueue, &dogm128_work,
		HZ / dogm128_rate);
}

unsigned char dogm128_enable(void)
{
	unsigned char ret;

	mutex_lock(&dogm128_mutex);

	if (!dogm128_updating) {
		dogm128_updating = 1;
		dogm128_queue();
		ret = 0;
	} else
		ret = 1;

	mutex_unlock(&dogm128_mutex);

	return ret;
}

void dogm128_disable(void)
{
	mutex_lock(&dogm128_mutex);

	if (dogm128_updating) {
		dogm128_updating = 0;
		cancel_delayed_work(&dogm128_work);
		flush_workqueue(dogm128_workqueue);
	}

	mutex_unlock(&dogm128_mutex);
}

unsigned char dogm128_isenabled(void)
{
	return dogm128_updating;
}

static void dogm128_update(struct work_struct *work)
{
	unsigned i, j, k;
	struct spi_device *spi_device = dogm128_dev.spi_device;
	u8 *p = dogm128_buffer;
	u8 *ub = NULL;


	if (spi_device == NULL)
		return;
	if (memcmp(dogm128_cache, dogm128_buffer, DOGM_RAM_SIZE)) {
		for(i = 0; i < DOGM_RAM_SIZE; i += DOGM_RES_X) {
			set_a0(0);
			/* Set page address */
			dogm128_dev.user_buff[0] = DOGM_CMD_SET_PAGE_ADDR | (0x0f & (i / DOGM_RES_X));
			spi_write(spi_device, dogm128_dev.user_buff, 1);

			/* Set column address to 0 */
			dogm128_dev.user_buff[0] = DOGM_CMD_SET_COL_ADDR_L | 0;
			dogm128_dev.user_buff[1] = DOGM_CMD_SET_COL_ADDR_H | 0;
			spi_write(spi_device, dogm128_dev.user_buff, 2);

			memset(dogm128_dev.user_buff, 0x00, DOGM_RES_X);
			for(j = 0; j < 8; j++)
				for(k = 0, ub = dogm128_dev.user_buff; k < DOGM_RES_X; k++) {
					if (p[(j * DOGM_RES_X / 8 ) + (k / 8)]  & (1 << (k % 8)))
						*ub |= 1 << j;
					ub++;
				}

			/* Write page */
			set_a0(1);
			spi_write(dogm128_dev.spi_device, dogm128_dev.user_buff, DOGM_RES_X);

			/* Move to next page */
			p += DOGM_RES_X;
		}
		memcpy(dogm128_cache, dogm128_buffer, DOGM_RAM_SIZE);
	}
	if (dogm128_updating)
		dogm128_queue();
}

/*
 * dogm128 Exported Symbols
 */

EXPORT_SYMBOL_GPL(dogm128_buffer);
EXPORT_SYMBOL_GPL(dogm128_getrate);
EXPORT_SYMBOL_GPL(dogm128_enable);
EXPORT_SYMBOL_GPL(dogm128_disable);
EXPORT_SYMBOL_GPL(dogm128_isenabled);


static int dogm128_probe(struct spi_device *spi_device)
{
	int status = 0;
	size_t size = 0;

	if (down_interruptible(&dogm128_dev.spi_sem))
		return -EBUSY;

	dogm128_dev.spi_device = spi_device;

	/* Send initialization sequence which has to be in DMAable storage */
	size = ARRAY_SIZE(dogm128_init_sequence);
	memcpy(dogm128_dev.user_buff, dogm128_init_sequence, size);
	status = spi_write(spi_device, dogm128_dev.user_buff, size);

	up(&dogm128_dev.spi_sem);
	printk(KERN_INFO "Probed\n");
	return status;
}

static int dogm128_remove(struct spi_device *spi_device)
{
	if (down_interruptible(&dogm128_dev.spi_sem))
		return -EBUSY;

	dogm128_dev.spi_device = NULL;

	up(&dogm128_dev.spi_sem);

	return 0;
}


static struct spi_driver dogm128_driver = {
	.driver = {
		.name =	this_driver_name,
		.owner = THIS_MODULE,
	},
	.probe = dogm128_probe,
	.remove = __devexit_p(dogm128_remove),
};

static int __init dogm128_init_spi(void)
{
	int error;

	error = spi_register_driver(&dogm128_driver);
	if (error < 0) {
		printk(KERN_ALERT "spi_register_driver() failed %d\n", error);
		return error;
	}
	return 0;
}

static int __init dogm128_init(void)
{
	int status = 0;

	memset(&dogm128_dev, 0, sizeof(dogm128_dev));

	BUILD_BUG_ON(PAGE_SIZE < DOGM_RAM_SIZE);
	BUILD_BUG_ON(USER_BUFF_SIZE < DOGM_RES_X);

	sema_init(&dogm128_dev.spi_sem, 1);


	dogm128_dev.user_buff = kzalloc(USER_BUFF_SIZE, GFP_KERNEL);
	if (dogm128_dev.user_buff == NULL) {
		status = -ENOMEM;
		goto fail_1;
	}

	dogm128_buffer = (u8*)get_zeroed_page(GFP_KERNEL);
	if (dogm128_buffer == NULL) {
		status = -ENOMEM;
		goto fail_2;
	}

	dogm128_cache = (u8*)get_zeroed_page(GFP_KERNEL);
	if (dogm128_cache == NULL) {
		status = -ENOMEM;
		goto fail_3;
	}


	dogm128_dev.reset_pin = DOGM_RESET_PIN;
	dogm128_dev.a0_pin = DOGM_A0_PIN;

	at91_set_gpio_output(dogm128_dev.reset_pin, 1);
	at91_set_gpio_output(dogm128_dev.a0_pin, 1);

	set_reset(0);
	set_a0(0);

	dogm128_workqueue = create_singlethread_workqueue(this_driver_name);
	if (dogm128_workqueue == NULL)
		goto fail_4;

	if ((status = dogm128_init_spi()) < 0)
		goto fail_5;

	return status;

fail_5:
	destroy_workqueue(dogm128_workqueue);

fail_4:
	free_page((unsigned long)dogm128_cache);

fail_3:
	free_page((unsigned long)dogm128_buffer);

fail_2:
	kfree(dogm128_dev.user_buff);

fail_1:
	return -1;
}
module_init(dogm128_init);

static void __exit dogm128_exit(void)
{
	destroy_workqueue(dogm128_workqueue);
	spi_unregister_driver(&dogm128_driver);

	set_reset(1);
	set_a0(1);

	if (dogm128_dev.user_buff)
		kfree(dogm128_dev.user_buff);

	if (dogm128_buffer)
		free_page((unsigned long)dogm128_buffer);

	if (dogm128_cache)
		free_page((unsigned long)dogm128_cache);
}
module_exit(dogm128_exit);


MODULE_AUTHOR("Max Holtzberg");
MODULE_DESCRIPTION("dogm128 LCD driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
