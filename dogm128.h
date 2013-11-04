/*
 *    Filename: dogm128.h
 *     Version: 0.1.0
 * Description: dogm128 LCD driver header
 *     License: GPLv2
 *
 *      Author: Copyright (C) Miguel Ojeda Sandonis
 *        Date: 2006-10-12
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

#ifndef _DOGM128_H_
#define _DOGM128_H_

#define DOGM_RES_X	(128)
#define DOGM_RES_Y	(64)
#define DOGM_RAM_SIZE	((DOGM_RES_X*DOGM_RES_Y) / 8)

/*
 * The driver will blit this buffer to the LCD
 *
 * Its size is dogm128_SIZE.
 */
extern unsigned char * dogm128_buffer;

/*
 * Get the refresh rate of the LCD
 *
 * Returns the refresh rate (hertz).
 */
extern unsigned int dogm128_getrate(void);

/*
 * Enable refreshing
 *
 * Returns 0 if successful (anyone was using it),
 * or != 0 if failed (someone is using it).
 */
extern unsigned char dogm128_enable(void);

/*
 * Disable refreshing
 *
 * You should call this only when you finish using the LCD.
 */
extern void dogm128_disable(void);

/*
 * Is enabled refreshing? (is anyone using the module?)
 *
 * Returns 0 if refreshing is not enabled (anyone is using it),
 * or != 0 if refreshing is enabled (someone is using it).
 *
 * Useful for buffer read-only modules.
 */
extern unsigned char dogm128_isenabled(void);

/*
 * Is the module inited?
 */
extern unsigned char dogm128_isinited(void);

#endif /* _DOGM128_H_ */
