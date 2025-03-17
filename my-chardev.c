#include "linux/cdev.h"
#include "linux/device/class.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/init.h"
#include "linux/kern_levels.h"
#include "linux/slab.h"
#include "linux/uaccess.h"
#include <linux/device.h>
#include <linux/module.h>

#define DEVICE_NUM	1
#define MEM_SIZE	0x1000

static struct my_chardev {
	dev_t devno;
	struct cdev cdev;
	u8 mem[MEM_SIZE];
}*my;

static char *my_chardev_class_devnode(const struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0644;
}
static const struct class my_chardev_class = {
	.name = "my_chardev",
	.devnode = my_chardev_class_devnode,
};

static ssize_t my_chardev_read(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos)
{
	size_t size;

	if (*ppos >= MEM_SIZE)
		return 0;

	size = *ppos + count > MEM_SIZE ? MEM_SIZE - *ppos : count;

	if (copy_to_user(buf, my->mem + *ppos, size))
		return -EFAULT;

	*ppos += size;

	return size;
}

static ssize_t my_chardev_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	size_t size;

	if (*ppos >= MEM_SIZE)
		return 0;

	size = *ppos + count > MEM_SIZE ? MEM_SIZE - *ppos : count;

	if (copy_from_user(my->mem + *ppos, buf, size))
		return -EFAULT;

	*ppos += size;

	return size;
}
	
static loff_t my_chardev_llseek(struct file *file, loff_t offset, int orig)
{
	printk(KERN_INFO"llseek is called\n");

	return 0;
}

static const struct file_operations fops = {
	.write = my_chardev_write,
	.read = my_chardev_read,
	.llseek = my_chardev_llseek,
};

static int __init my_chardev_init(void)
{
	int retval;
	struct device *dev;

	my = kzalloc(sizeof(*my), GFP_KERNEL);
	if (!my) {
		printk(KERN_ERR"allock memory for my_chardev structure failed\n");
		return -ENOMEM;
	}

	retval = alloc_chrdev_region(&my->devno, 0, DEVICE_NUM, "my_chardev");
	if (retval) {
		printk(KERN_ERR"alloc dev number failed\n");
		return retval;
	}

	cdev_init(&my->cdev, &fops);
	my->cdev.owner = THIS_MODULE;
	retval = cdev_add(&my->cdev, my->devno, DEVICE_NUM);
	if (retval) {
		printk(KERN_ERR"failed to add char device\n");
		return retval;
	}

	retval = class_register(&my_chardev_class);
	if (retval) {
		printk(KERN_ERR"class register failed\n");
		return retval;
	}

	dev = device_create(&my_chardev_class, NULL, my->devno, NULL, "my_chardev");
	if (!dev) {
		printk(KERN_ERR"device create failed\n");
		return PTR_ERR(dev);
	}
	return 0;
}

static void __exit my_chardev_exit(void)
{
	device_destroy(&my_chardev_class, my->devno);

	class_unregister(&my_chardev_class);

	cdev_del(&my->cdev);

	unregister_chrdev_region(my->devno, DEVICE_NUM);

	kfree(my);
}

module_init(my_chardev_init);
module_exit(my_chardev_exit);

MODULE_LICENSE("GPL");
