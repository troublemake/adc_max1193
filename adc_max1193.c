/*
 * adc_max1193.c
 *
 *  Created on: 22.07.2011
 *      Author: entwickler
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>

#define USER_BUFF_SIZE 128

struct adc_max1193_dev {
	dev_t devt;
	struct cdev cdev;
	struct semaphore sem;
	struct class *class;
	char *user_buff;
};

static struct adc_max1193_dev adc_max1193_dev;

static ssize_t adc_max1193_write(struct file *filp, const char __user *buff,
		size_t count, loff_t *f_pos) {

	ssize_t status;
	size_t len = USER_BUFF_SIZE - 1;

	if (count == 0)
		return 0;

	if (down_interruptible(&adc_max1193_dev.sem))
		return -ERESTARTSYS;

	if (len > count)
		len = count;

	memset(adc_max1193_dev.user_buff, 0, USER_BUFF_SIZE);

	if (copy_from_user(adc_max1193_dev.user_buff, buff, len)) {
		status = -EFAULT;
		goto adc_max1193_write_done;
	}

	/* do something with the user data */

	status = len;
	*f_pos += len;

adc_max1193_write_done:
	up(&adc_max1193_dev.sem);
	return status;
}

static ssize_t adc_max1193_read(struct file *filp, char __user *buff,
		size_t count, loff_t *offp) {

	ssize_t status;
	size_t len;
	/* 	Generic user progs like cat will continue calling until we
	 *  return zero. So if *offp != 0, we know this is at least the
	 *  second call.
	 */

	if (*offp > 0)
		return 0;

	if (down_interruptible(&adc_max1193_dev.sem))
	return -ERESTARTSYS;

	strcpy(adc_max1193_dev.user_buff, "adc_max1193 driver data\n");
	len = strlen(adc_max1193_dev.user_buff);

	if (len > count)
		len = count;

	if (copy_to_user(buff, adc_max1193_dev.user_buff, len)) {
		status = -EFAULT;
		goto adc_max1193_read_done;
	}

	*offp += len;
	status = len;

adc_max1193_read_done:
	up(&adc_max1193_dev.sem);
	return status;
}

static int adc_max1193_open(struct inode *inode, struct file *filp){

	int status = 0;

	if (down_interruptible(&adc_max1193_dev.sem))
		return -ERESTARTSYS;

	if (!adc_max1193_dev.user_buff) {
		adc_max1193_dev.user_buff = kmalloc(USER_BUFF_SIZE, GFP_KERNEL);

		if (!adc_max1193_dev.user_buff) {
			printk(KERN_ALERT "adc_max1193_open: user_buff alloc failed\n");

			status = -ENOMEM;
		}
	}
	up(&adc_max1193_dev.sem);
	return status;
}

static const struct file_operations	adc_max1193_fops = {
		.owner = THIS_MODULE,
		.open = adc_max1193_open,
		.read = adc_max1193_read,
		.write = adc_max1193_write,
};

static int __init adc_max1193_init_cdev(void) {
	int error;
	adc_max1193_dev.devt = MKDEV(0, 0);
	error = alloc_chrdev_region(&adc_max1193_dev.devt, 0, 1, "adc_max1193");
	if (error < 0) {
		printk(KERN_ALERT "alloc_chrdev_region() failed: error = %d \n", error);
		return -1;
	}

	cdev_init(&adc_max1193_dev.cdev, &adc_max1193_fops);
	adc_max1193_dev.cdev.owner = THIS_MODULE;

	error = cdev_add(&adc_max1193_dev.cdev, adc_max1193_dev.devt, 1);
	if (error) {printk(KERN_ALERT "cdev_add() failed: error = %d\n", error);
		unregister_chrdev_region(adc_max1193_dev.devt, 1);
		return -1;
	}
	return 0;
}

static int __init adc_max1193_init_class(void) {
	adc_max1193_dev.class = class_create(THIS_MODULE, "adc_max1193");

	if (!adc_max1193_dev.class) {
		printk(KERN_ALERT "class_create(adc_max1193) failed\n");
		return -1;
	}

	if (!device_create(adc_max1193_dev.class, NULL, adc_max1193_dev.devt, NULL, "adc_max1193")) {
		class_destroy(adc_max1193_dev.class);
		return -1;
	}
	return 0;
}

static int __init adc_max1193_init(void) {
	printk(KERN_INFO "adc_max1193_init()\n");
	memset(&adc_max1193_dev, 0, sizeof(struct adc_max1193_dev));
	sema_init(&adc_max1193_dev.sem, 1);
	if (adc_max1193_init_cdev())
		goto init_fail_1;
	if (adc_max1193_init_class())
		goto init_fail_2;

	return 0;

init_fail_2:
	cdev_del(&adc_max1193_dev.cdev);
	unregister_chrdev_region(adc_max1193_dev.devt, 1);

init_fail_1:
	return -1;
}
module_init( adc_max1193_init);
static void __exit adc_max1193_exit(void) {
	printk(KERN_INFO "adc_max1193_exit()\n");

	device_destroy(adc_max1193_dev.class, adc_max1193_dev.devt);
	class_destroy(adc_max1193_dev.class);

	cdev_del(&adc_max1193_dev.cdev);
	unregister_chrdev_region(adc_max1193_dev.devt, 1);

	if (adc_max1193_dev.user_buff)
		kfree(adc_max1193_dev.user_buff);
}
module_exit( adc_max1193_exit);

MODULE_AUTHOR("entwickler");
MODULE_DESCRIPTION("adc_max1193 driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.1");
