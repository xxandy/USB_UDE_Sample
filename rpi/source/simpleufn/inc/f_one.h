/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header defines structures and constants used by "Function One",
 * a simple USB function made for education purposes.
 *
 * This header file defines the public initialization interface used
 * by gagdet drivers that use this function.
 * 
 * This USB Function demonstrates building an out-of-tree USB function.
 */

#ifndef __F_ONE_H
#define __F_ONE_H


#define F_ONE_DEFAULT_BULK_BUFLEN 4096
#define F_ONE_DEFAULT_QLEN         32

struct f_lb_opts {
	struct usb_function_instance func_inst;
	unsigned bulk_buflen;
	unsigned qlen;

	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex			lock;
	int				refcnt;
};


#endif /* __F_ONE_H */
