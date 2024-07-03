#include <linux/init.h>
#include <linux/module.h>

static int __init client_start(void) {

    return 0;
}

static void __exit client_exit(void) {

    return;
}

module_init(client_start);
module_exit(client_exit);
MODULE_LICENSE("GPL");
