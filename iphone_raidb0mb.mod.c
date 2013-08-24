#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x9a31bb74, "module_layout" },
	{ 0x754c57a0, "usb_deregister" },
	{ 0x84e528ea, "usb_register_driver" },
	{ 0xfb578fc5, "memset" },
	{ 0x1e047854, "warn_slowpath_fmt" },
	{ 0x16305289, "warn_slowpath_null" },
	{ 0xd4711236, "cancel_delayed_work_sync" },
	{ 0xb3be75f6, "dev_set_drvdata" },
	{ 0xa8231eda, "usb_kill_urb" },
	{ 0xd9d35cd9, "unregister_netdev" },
	{ 0x10519fe3, "dev_get_drvdata" },
	{ 0xfacf2d1b, "netif_rx" },
	{ 0x87ce1f99, "eth_type_trans" },
	{ 0x69acdf38, "memcpy" },
	{ 0x11f2fca, "skb_put" },
	{ 0x7a00547, "__netdev_alloc_skb" },
	{ 0xf57a521b, "usb_free_urb" },
	{ 0x2e1da436, "usb_free_coherent" },
	{ 0x68bc3b04, "usb_alloc_coherent" },
	{ 0xbb4d6341, "usb_alloc_urb" },
	{ 0x6b06fdce, "delayed_work_timer_fn" },
	{ 0x593a99b, "init_timer_key" },
	{ 0xb529515a, "_dev_info" },
	{ 0x27e1a049, "printk" },
	{ 0x37a0cba, "kfree" },
	{ 0xd61adcbd, "kmem_cache_alloc_trace" },
	{ 0x5cd9dbb5, "kmalloc_caches" },
	{ 0xc3fb7d5d, "free_netdev" },
	{ 0x533d2f64, "usb_altnum_to_altsetting" },
	{ 0xdcfe4431, "alloc_etherdev_mqs" },
	{ 0x1f822217, "schedule_delayed_work" },
	{ 0x1eb9516e, "round_jiffies_relative" },
	{ 0xe2eb05fe, "usb_set_interface" },
	{ 0x68b3dcbc, "netif_carrier_on" },
	{ 0xd54f2103, "netif_carrier_off" },
	{ 0x9dc10903, "usb_control_msg" },
	{ 0xd20f7b8c, "__netif_schedule" },
	{ 0xe67bab80, "dev_kfree_skb_irq" },
	{ 0x8da8689f, "usb_submit_urb" },
	{ 0xaea99d2f, "usb_unlink_urb" },
	{ 0xfee0571a, "dev_err" },
	{ 0xbdfb6dbb, "__fentry__" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("usb:v05ACp12A8d*dc*dsc*dp*icFFiscFDip01in*");

MODULE_INFO(srcversion, "F69B1A65DAD1A62C2F2E707");
