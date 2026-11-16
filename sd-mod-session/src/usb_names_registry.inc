/* SPDX-License-Identifier: GPL-2.0
 * Included after the pinned inMusic f_uac2 implementation. The stock driver
 * stays in kernel memory. Its registration is restored when this RAM module
 * is unloaded, after the host has removed all named function instances.
 */
static struct usb_function_driver *am_stock_uac2;
DECLARE_USB_FUNCTION(uac2, afunc_alloc_inst, afunc_alloc);

static int __init am_usb_names_init(void)
{
	struct usb_function_instance *stock;
	int result;
	stock = usb_get_function_instance("uac2");
	if (IS_ERR(stock))
		return PTR_ERR(stock);
	am_stock_uac2 = stock->fd;
	/* This build targets the firmware's built-in function, not another LKM. */
	if (!am_stock_uac2 || am_stock_uac2->mod) {
		usb_put_function_instance(stock);
		return -EOPNOTSUPP;
	}
	usb_put_function_instance(stock);
	usb_function_unregister(am_stock_uac2);
	result = usb_function_register(&uac2usb_func);
	if (result) {
		int restored = usb_function_register(am_stock_uac2);
		if (restored)
			pr_err("AM_USB_NAMES stock registration restore failed: %d\n", restored);
		return result;
	}
	pr_info("AM_USB_NAMES registered 18 named capture channels\n");
	return 0;
}

static void __exit am_usb_names_exit(void)
{
	int result;
	usb_function_unregister(&uac2usb_func);
	result = usb_function_register(am_stock_uac2);
	if (result)
		pr_err("AM_USB_NAMES stock registration restore failed: %d\n", result);
	else
		pr_info("AM_USB_NAMES restored stock UAC2 registration\n");
}
module_init(am_usb_names_init);
module_exit(am_usb_names_exit);
