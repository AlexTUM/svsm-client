#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/slab.h>

#include <asm/page.h>
#include <asm/sev.h>
#include <asm/errno.h>
#include <asm/io.h>

#include "address_helper.h"

static dev_t dev = 0;
static struct class *dev_cls;
static struct cdev cdev;


static int svsm_open(struct inode*, struct file *);
static int svsm_release(struct inode*, struct file *);
static ssize_t svsm_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t svsm_write(struct file *, const char __user *, size_t, loff_t *);

#define BUF_LEN 2048
#define PAGE_4K (1UL << 12)

const static u64 SVSM_CALL_BASE = 3UL << 32;
const static u64 SVSM_CALL_HASH_SINGLE = SVSM_CALL_BASE | 1; 

extern int do_svsm_protocol(struct svsm_call *call); /* in patched kernel */

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
    pr_warn("Requesting report from svsm\n");
    if (length > BUF_LEN) {
        pr_err("Too much to write\n");
        return -1;
    }

    for (int i = 0; i < BUF_LEN; i++) {
        svsm_buf[i] = '\0';
    }
    pr_warn("Buffer cleared");

    int read_bytes = copy_from_user(svsm_buf, buf, length);
    if (read_bytes < 0) {
        pr_err("Error reading from userspace\n");
        return -1;
    }
    pr_warn("Read %d bytes from user\n", read_bytes);

    /* convert buffer content into number */
    int req_pid = 0;
    int add_start = 0;
    for (add_start; add_start < 10; add_start++) {
        if (svsm_buf[add_start] == ' ') {
            svsm_buf[add_start] = '\0';
            break;
        }
    }
    if (kstrtoint(svsm_buf, 10, &req_pid)) {
        pr_err("Error interpreting pid as numeric value\n");
        return -1;
    }
    pr_warn("pid is %d\n", req_pid);
    pr_warn("add_start is %d\n", add_start);
    unsigned long addr_raw = 0;
    if (kstrtoul(svsm_buf + add_start + 1, 16, &addr_raw)) {
        pr_err("Error interpreting address as numeric value\n");
        return -1;
    }
    pr_warn("vaddr of page is %lx\n", addr_raw);
    /* convert into phyical address */
    void *p_addr_ptr = (void *)addr_raw;
    uint64_t p_addr_phys = pagewalki_for_pid(p_addr_ptr, req_pid);
    pr_warn("paddr of page is %llx\n", p_addr_phys);
    //unsigned long buf_addr_phys = virt_to_phys(svsm_buf);
    void *tmp_buf_virt = kmalloc(BUF_LEN, GFP_KERNEL | __GFP_ZERO);
    unsigned long tmp_buf_phys = virt_to_phys(tmp_buf_virt);

    pr_warn("paddr of buf is %lx\n", tmp_buf_phys);
    /* TODO: what if it is userspace? */
    /* make svsm call */
    struct svsm_call check_call;
    /* call id */
    check_call.rax = SVSM_CALL_HASH_SINGLE;
    /* physical address of page to check */
    check_call.rcx = (u64) p_addr_phys;
    /* page size used */
    u64 page_size_indicator = 0;
    if (PAGE_SIZE != PAGE_4K)
        page_size_indicator = 1;
    check_call.rdx = page_size_indicator;
    /* physical address of report buffer */
    check_call.r8 = (u64) tmp_buf_phys;
    /* size of report buffer */
    check_call.r9 = (u64) BUF_LEN;
    pr_warn("rax: %llx, rcx: %llx, rdx: %lld, r8: %llx, r9: %lld\n", check_call.rax, check_call.rcx, check_call.rdx, check_call.r8, check_call.r9);
    int ret = do_svsm_protocol(&check_call);
    pr_warn("Return code is %x", ret);
    return (ssize_t) length;
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
