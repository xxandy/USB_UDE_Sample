/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This header defines structures and constants used by "Gadget One",
 * a simple USB gadget made for education/sample purposes.
 */

#ifndef __G_ONE_H
#define __G_ONE_H

#define GZERO_BULK_BUFLEN	4096
#define GZERO_QLEN		32
#define GZERO_ISOC_INTERVAL	4
#define GZERO_ISOC_MAXPACKET	1024
#define GZERO_SS_BULK_QLEN	1
#define GZERO_SS_ISO_QLEN	8

struct usb_zero_options {
	unsigned pattern;
	unsigned isoc_interval;
	unsigned isoc_maxpacket;
	unsigned isoc_mult;
	unsigned isoc_maxburst;
	unsigned bulk_buflen;
	unsigned qlen;
	unsigned ss_bulk_qlen;
	unsigned ss_iso_qlen;
};



#endif /* __G_ONE_H */
