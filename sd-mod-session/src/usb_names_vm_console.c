/* SPDX-License-Identifier: GPL-2.0
 * Test-only console for booting the unmodified MPC kernel on QEMU virt.
 * Never distributed to the MPC. Refuses every board except dummy-virt.
 */
#include <linux/module.h>
#include <linux/console.h>
#include <linux/io.h>
#include <linux/of.h>
static void __iomem *am_uart;
static void am_vm_write(struct console *console, const char *text, unsigned count)
{
	unsigned i;
	(void)console;
	for (i = 0; i < count; ++i) {
		unsigned wait = 100000;
		while ((readl(am_uart + 0x18) & 0x20) && --wait)
			cpu_relax();
		writel(text[i], am_uart);
	}
}
static struct console am_vm_console = {
	.name = "amvm", .write = am_vm_write, .flags = CON_ENABLED, .index = -1,
};
static int __init am_vm_init(void)
{
	if (!of_machine_is_compatible("linux,dummy-virt"))
		return -ENODEV;
	am_uart = ioremap(0x09000000, 4096);
	if (!am_uart)
		return -ENOMEM;
	register_console(&am_vm_console);
	pr_info("AM_VM_CONSOLE_READY\n");
	return 0;
}
static void __exit am_vm_exit(void)
{
	unregister_console(&am_vm_console);
	iounmap(am_uart);
}
module_init(am_vm_init);
module_exit(am_vm_exit);
MODULE_LICENSE("GPL");
