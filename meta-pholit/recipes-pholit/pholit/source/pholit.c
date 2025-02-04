#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init
pholit_start(void)
{
    printk(KERN_INFO "Loading pholit module\n");
    printk(KERN_INFO "Hello world\n");
    return 0;
}

static void __exit
pholit_end(void)
{
    printk(KERN_INFO "Goodbye\n");
}

module_init(pholit_start);
module_init(pholit_end);
