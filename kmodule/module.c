#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <asm/errno.h>

static dev_t dev = 0;
static struct class *dev_cls;
static struct cdev cdev;

static int svsm_open(struct inode*, struct file *);
static int svsm_release(struct inode*, struct file *);
static ssize_t svsm_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t svsm_write(struct file *, const char __user *, size_t, loff_t *);

#define BUF_LEN 2048

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = svsm_open,
    .release = svsm_release,
    .read = svsm_read,
    .write = svsm_write,
};

static char svsm_buf[BUF_LEN + 1];

static int svsm_open(struct inode *inode, struct file *file) {
    pr_info("SVSM client connected\n");
    return 0;
}

static int svsm_release(struct inode *inode, struct file *file) {
    pr_info("SVSM client disconnected\n");
    return 0;
}

static ssize_t svsm_read(struct file *file, char __user *buf, size_t length, loff_t *offset) {
    int bytes_read = 0;
    const char *svsm_ptr = svsm_buf;

    if (!*(svsm_ptr + *offset)) {
        *offset = 0;
        return 0;
    }

    svsm_ptr += *offset;

    while (length && *svsm_ptr) {
        put_user(*(svsm_ptr++), buf++);
        length--;
        bytes_read++;
    }

    *offset += bytes_read;
    return bytes_read;
}

static ssize_t svsm_write(struct file *file, const char __user *buf, size_t length, loff_t *offset) {
    if (length > BUF_LEN) {
        pr_err("Too much to write\n");
        return -1;
    }

    int read_bytes = copy_from_user(svsm_buf, buf, length);
    if (read_bytes < 0) {
        pr_err("Error reading from userspace\n");
        return -1;
    }

    return 0;
}

static int __init client_start(void) {
    pr_info("Loading svsm client kernel module\n");

    int ret = alloc_chrdev_region(&dev, 0, 1, "svsm_client");
    if (ret < 0) {
        pr_err("Couldn't register device: %d\n", ret);
        return ret;
    }

    cdev_init(&cdev, &fops);
    ret = cdev_add(&cdev, dev, 1);
    if (ret < 0) {
        pr_err("Couldn't add device: %d\n", ret);
        return ret;
    }

    if (IS_ERR(dev_cls = class_create("svsm_client_dev"))) {
        pr_err("Couldn't create class");
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    if (IS_ERR(device_create(dev_cls,NULL,dev,NULL,"svsm_client_dev"))) {
        pr_err("Couldn't create device");
        class_destroy(dev_cls);
        unregister_chrdev_region(dev, 1);
        return -1;
    }

    pr_info("Initialized SVSM client");
    return 0;
}

static void __exit client_exit(void) {
    pr_info("Unloading svsm client kernel module\n");
    device_destroy(dev_cls, dev);
    class_destroy(dev_cls);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
    return;
}

module_init(client_start);
module_exit(client_exit);
MODULE_LICENSE("GPL");
