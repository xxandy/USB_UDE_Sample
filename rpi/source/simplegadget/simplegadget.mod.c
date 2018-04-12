#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x6681337c, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x2e5810c6, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr1) },
	{ 0x5c2e3421, __VMLINUX_SYMBOL_STR(del_timer) },
	{ 0xb1ad28e0, __VMLINUX_SYMBOL_STR(__gnu_mcount_nc) },
	{ 0x423b3e95, __VMLINUX_SYMBOL_STR(param_ops_bool) },
	{ 0x5ee52022, __VMLINUX_SYMBOL_STR(init_timer_key) },
	{ 0xda02d67, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x80ed10b2, __VMLINUX_SYMBOL_STR(usb_ep_autoconfig_reset) },
	{ 0x5ceb3818, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0x4fb99ea4, __VMLINUX_SYMBOL_STR(usb_put_function_instance) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xa38caae0, __VMLINUX_SYMBOL_STR(mod_timer) },
	{ 0x383c731d, __VMLINUX_SYMBOL_STR(usb_composite_overwrite_options) },
	{ 0x1fdc915d, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0xd5fb406, __VMLINUX_SYMBOL_STR(usb_composite_probe) },
	{ 0xe27a9437, __VMLINUX_SYMBOL_STR(usb_add_function) },
	{ 0xe65c22d4, __VMLINUX_SYMBOL_STR(usb_gadget_wakeup) },
	{ 0x4449aa67, __VMLINUX_SYMBOL_STR(usb_put_function) },
	{ 0xf924d2fe, __VMLINUX_SYMBOL_STR(usb_composite_unregister) },
	{ 0x8a9440f5, __VMLINUX_SYMBOL_STR(usb_get_function) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x3a1e0250, __VMLINUX_SYMBOL_STR(usb_string_ids_tab) },
	{ 0xb657b5df, __VMLINUX_SYMBOL_STR(usb_add_config_only) },
	{ 0x5502bb28, __VMLINUX_SYMBOL_STR(usb_get_function_instance) },
	{ 0xe573468a, __VMLINUX_SYMBOL_STR(param_ops_ushort) },
	{ 0x7f02188f, __VMLINUX_SYMBOL_STR(__msecs_to_jiffies) },
	{ 0x3649d8b5, __VMLINUX_SYMBOL_STR(param_ops_uint) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=libcomposite,udc-core";


MODULE_INFO(srcversion, "07700BFC34F926E83A8B58A");
