/*
 * one.c -- Gadget One, for USB development
 *
 * Copyright (C) 2017 by Microsoft corporation
 *
 */

/*
 * Gadget one is an exploration into Gadget development for Linux
 * Using the Gadget APIS. 
 * 
 * It expands (contracts?) from Gadget zero, but removes the multi-function
 * aspect of it, so that the driver can be a simpler starting point
 * for developing other drivers.
 *
 * Use it with the Windows Host-side UDETEst from https://github.com/xxandy/USB_UDE_Sample,
 * picking the eduref_BULKOUT_plus_BULKIN branch, which does not have interruptes,
 * and changing the hardware ID's on the INF file to match this device.
 */

/*
 * driver assumes self-powered hardware, and
 * has no way for users to trigger remote wakeup.
 */

/* #define VERBOSE_DEBUG */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/usb/composite.h>

#include "g_one.h" // my stuff 

#include "f_one.h" // interface exposed by a function I need

/*-------------------------------------------------------------------------*/
USB_GADGET_COMPOSITE_OPTIONS();

#define DRIVER_VERSION		"Cinco de Mayo 2008"

static const char longname[] = "Gadget One";


static struct usb_zero_options gzero_options = {
	.bulk_buflen = GZERO_BULK_BUFLEN,
	.qlen = GZERO_QLEN,
};

/*-------------------------------------------------------------------------*/

/* Thanks to NetChip Technologies for donating this product ID.
 *
 * DO NOT REUSE THESE IDs with a protocol-incompatible driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures.
 */
#define DRIVER_VENDOR_NUM	0x1209 /* open source projects */
#define DRIVER_PRODUCT_NUM	0x0888 /* Lucky */
#define DEFAULT_AUTORESUME	0

/* If the optional "autoresume" mode is enabled, it provides good
 * functional coverage for the "USBCV" test harness from USB-IF.
 * It's always set if OTG mode is enabled.
 */
static unsigned autoresume = DEFAULT_AUTORESUME;
module_param(autoresume, uint, S_IRUGO);
MODULE_PARM_DESC(autoresume, "zero, or seconds before remote wakeup");

/* Maximum Autoresume time */
static unsigned max_autoresume;
module_param(max_autoresume, uint, S_IRUGO);
MODULE_PARM_DESC(max_autoresume, "maximum seconds before remote wakeup");

/* Interval between two remote wakeups */
static unsigned autoresume_interval_ms;
module_param(autoresume_interval_ms, uint, S_IRUGO);
MODULE_PARM_DESC(autoresume_interval_ms,
		"milliseconds to increase successive wakeup delays");

static unsigned autoresume_step_ms;
/*-------------------------------------------------------------------------*/

static struct usb_device_descriptor device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	/* .bcdUSB = DYNAMIC */
	.bDeviceClass =		USB_CLASS_VENDOR_SPEC,

	.idVendor =		cpu_to_le16(DRIVER_VENDOR_NUM),
	.idProduct =		cpu_to_le16(DRIVER_PRODUCT_NUM),
	.bNumConfigurations =	1,
};

static const struct usb_descriptor_header *otg_desc[2];

/* string IDs are assigned dynamically */
/* default serial number takes at least two packets */
static char serial[] = "0123456789.0123456789.0100020000";

#define USB_GZERO_LB_DESC	(USB_GADGET_FIRST_AVAIL_IDX + 0)

static struct usb_string strings_dev[] = {
	[USB_GADGET_MANUFACTURER_IDX].s = "XSample Mfg",
	[USB_GADGET_PRODUCT_IDX].s = longname,
	[USB_GADGET_SERIAL_IDX].s = serial,
	[USB_GZERO_LB_DESC].s	= "loop input to output",
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_dev = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_dev,
};

static struct usb_gadget_strings *dev_strings[] = {
	&stringtab_dev,
	NULL,
};

/*-------------------------------------------------------------------------*/

static struct timer_list	autoresume_timer;

static void zero_autoresume(unsigned long _c)
{
	struct usb_composite_dev	*cdev = (void *)_c;
	struct usb_gadget		*g = cdev->gadget;

	/* unconfigured devices can't issue wakeups */
	if (!cdev->config)
		return;

	/* Normally the host would be woken up for something
	 * more significant than just a timer firing; likely
	 * because of some direct user request.
	 */
	if (g->speed != USB_SPEED_UNKNOWN) {
		int status = usb_gadget_wakeup(g);
		INFO(cdev, "%s --> %d\n", __func__, status);
	}
}

static void zero_suspend(struct usb_composite_dev *cdev)
{
	if (cdev->gadget->speed == USB_SPEED_UNKNOWN)
		return;

	if (autoresume) {
		if (max_autoresume &&
			(autoresume_step_ms > max_autoresume * 1000))
				autoresume_step_ms = autoresume * 1000;

		mod_timer(&autoresume_timer, jiffies +
			msecs_to_jiffies(autoresume_step_ms));
		DBG(cdev, "suspend, wakeup in %d milliseconds\n",
			autoresume_step_ms);

		autoresume_step_ms += autoresume_interval_ms;
	} else
		DBG(cdev, "%s\n", __func__);
}

static void zero_resume(struct usb_composite_dev *cdev)
{
	DBG(cdev, "%s\n", __func__);
	del_timer(&autoresume_timer);
}

/*-------------------------------------------------------------------------*/

static struct usb_function *func_lb;
static struct usb_function_instance *func_inst_lb;


static int lb_config_setup(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl)
{
	switch (ctrl->bRequest) {
	case 0x5b:
	case 0x5c:
		return func_lb->setup(func_lb, ctrl);  // TODO: do we really need this?
	default:
		return -EOPNOTSUPP;
	}
}


static struct usb_configuration loopback_driver = {
	.label          = "loopback",
	.setup                  = lb_config_setup,
	.bConfigurationValue = 2,
	.bmAttributes   = USB_CONFIG_ATT_SELFPOWER,
	/* .iConfiguration = DYNAMIC */
};



module_param_named(buflen, gzero_options.bulk_buflen, uint, 0);






module_param_named(qlen, gzero_options.qlen, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(qlen, "depth of loopback queue");



static int zero_bind(struct usb_composite_dev *cdev)
{
	struct f_lb_opts	*lb_opts;
	int			status;

        printk("Zerobind enter\n");

	/* Allocate string descriptor numbers ... note that string
	 * contents can be overridden by the composite_dev glue.
	 */
	status = usb_string_ids_tab(cdev, strings_dev);
	if (status < 0)
		return status;

	printk("Zerobind id strings ok\n");

	device_desc.iManufacturer = strings_dev[USB_GADGET_MANUFACTURER_IDX].id;
	device_desc.iProduct = strings_dev[USB_GADGET_PRODUCT_IDX].id;
	device_desc.iSerialNumber = strings_dev[USB_GADGET_SERIAL_IDX].id;

	setup_timer(&autoresume_timer, zero_autoresume, (unsigned long) cdev);






	func_inst_lb = usb_get_function_instance("OTLoopBck");
	if (IS_ERR(func_inst_lb)) {
        printk("unable to load function\n");
		return PTR_ERR(func_inst_lb); // nothing to undo in this case
	}

	printk("Zerobind got OTLoopBck\n");

	lb_opts = container_of(func_inst_lb, struct f_lb_opts, func_inst);
	lb_opts->bulk_buflen = gzero_options.bulk_buflen;
	lb_opts->qlen = gzero_options.qlen;

	func_lb = usb_get_function(func_inst_lb);
	if (IS_ERR(func_lb)) {
		status = PTR_ERR(func_lb);
		goto err_put_func_inst_lb;
	}

	printk("Zerobind translated OTLoopBck\n");

	loopback_driver.iConfiguration = strings_dev[USB_GZERO_LB_DESC].id;

	/* support autoresume for remote wakeup testing */
	loopback_driver.bmAttributes &= ~USB_CONFIG_ATT_WAKEUP;
	loopback_driver.descriptors = NULL;
	if (autoresume) {
		loopback_driver.bmAttributes |= USB_CONFIG_ATT_WAKEUP;
		autoresume_step_ms = autoresume * 1000;
	}

	/* support OTG systems */
	if (gadget_is_otg(cdev->gadget)) {
		if (!otg_desc[0]) {
			struct usb_descriptor_header *usb_desc;

			usb_desc = usb_otg_descriptor_alloc(cdev->gadget);
			if (!usb_desc) {
				status = -ENOMEM;
				goto err_conf_flb;
			}
			usb_otg_descriptor_init(cdev->gadget, usb_desc);
			otg_desc[0] = usb_desc;
			otg_desc[1] = NULL;
		}
		loopback_driver.descriptors = otg_desc;
		loopback_driver.bmAttributes |= USB_CONFIG_ATT_WAKEUP;
	}

	/* Register primary, then secondary configuration.  Note that
	 * SH3 only allows one config...
	 */
	usb_add_config_only(cdev, &loopback_driver);

	status = usb_add_function(&loopback_driver, func_lb);
	if (status)
		goto err_free_otg_desc;

	usb_ep_autoconfig_reset(cdev->gadget);
	usb_composite_overwrite_options(cdev, &coverwrite);

	INFO(cdev, "%s, version: " DRIVER_VERSION "\n", longname);

	return 0;

err_free_otg_desc:
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;
err_conf_flb:
	usb_put_function(func_lb);
	func_lb = NULL;
err_put_func_inst_lb:
	usb_put_function_instance(func_inst_lb);
	func_inst_lb = NULL;
	return status;
}

static int zero_unbind(struct usb_composite_dev *cdev)
{
	del_timer_sync(&autoresume_timer);
	if (!IS_ERR_OR_NULL(func_lb))
		usb_put_function(func_lb);
	usb_put_function_instance(func_inst_lb);
	kfree(otg_desc[0]);
	otg_desc[0] = NULL;

	return 0;
}

static struct usb_composite_driver zero_driver = {
	.name		= "zero",
	.dev		= &device_desc,
	.strings	= dev_strings,
	.max_speed	= USB_SPEED_SUPER,
	.bind		= zero_bind,
	.unbind		= zero_unbind,
	.suspend	= zero_suspend,
	.resume		= zero_resume,
};

module_usb_composite_driver(zero_driver);

MODULE_AUTHOR("David Brownell");
MODULE_LICENSE("GPL");
