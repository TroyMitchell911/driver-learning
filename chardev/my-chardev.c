#include "linux/cdev.h"
#include "linux/container_of.h"
#include "linux/device/class.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/init.h"
#include "linux/kdev_t.h"
#include "linux/kern_levels.h"
#include "linux/mutex.h"
#include "linux/slab.h"
#include "linux/uaccess.h"
#include <linux/device.h>
#include <linux/module.h>

#define DEVICE_NUM	3
#define MEM_SIZE	0x1000

static dev_t devno;

struct my_chardev {
	struct cdev cdev;
	struct mutex mutex;
	u8 mem[MEM_SIZE];
}*my;

static int my_chardev_open(struct inode *inode, struct file *file)
{
	struct my_chardev *dev;

	dev = container_of(inode->i_cdev, struct my_chardev, cdev);

	file->private_data = dev;
	return 0;
}

static ssize_t my_chardev_read(struct file *file, char __user *buf,
			       size_t count, loff_t *ppos)
{
	size_t size;
	unsigned long pos = *ppos;
	struct my_chardev *my_dev = file->private_data;

	if (pos >= MEM_SIZE)
		return 0;

	size = pos + count > MEM_SIZE ? MEM_SIZE - pos : count;

	mutex_lock(&my->mutex);

	if (copy_to_user(buf, my_dev->mem + pos, size))
		return -EFAULT;

	*ppos += size;

	mutex_unlock(&my->mutex);

	return size;
}

static ssize_t my_chardev_write(struct file *file, const char __user *buf,
				size_t count, loff_t *ppos)
{
	size_t size;
	unsigned long pos = *ppos;
	struct my_chardev *my_dev = file->private_data;

	if (pos >= MEM_SIZE)
		return 0;

	size = pos + count > MEM_SIZE ? MEM_SIZE - pos : count;

	mutex_lock(&my->mutex);

	if (copy_from_user(my_dev->mem + pos, buf, size))
		return -EFAULT;

	*ppos += size;

	mutex_unlock(&my->mutex);

	return size;
}
	
static loff_t my_chardev_llseek(struct file *file, loff_t offset, int orig)
{
	printk(KERN_INFO"llseek is called\n");

	return 0;
}

static const struct file_operations fops = {
	.open = my_chardev_open,
	.write = my_chardev_write,
	.read = my_chardev_read,
	.llseek = my_chardev_llseek,
};

static char *my_chardev_class_devnode(const struct device *dev, umode_t *mode)
{
	if (mode)
		*mode = 0444;

	return NULL;
}

static const struct class my_chardev_class = {
	.name = "my_chardev",
	.devnode = my_chardev_class_devnode,
};

static int my_chardev_setup(struct my_chardev *my_chardev)
{
	int retval;
	int major = MAJOR(devno);
	int cdev_num = 0, device_num = 0;
	struct device *dev;
	struct my_chardev *my = my_chardev;

	for (int i = 0; i < DEVICE_NUM; i++) {
		mutex_init(&my->mutex);

		cdev_init(&my->cdev, &fops);
		my->cdev.owner = THIS_MODULE;

		retval = cdev_add(&my->cdev, devno, DEVICE_NUM);
		if (retval) {
			printk(KERN_ERR"failed to add char device\n");
			goto failed;
		}
		cdev_num ++;

		dev = device_create(&my_chardev_class, NULL, MKDEV(major, i), NULL, "my_chardev%d", i);
		if (!dev) {
			printk(KERN_ERR"device %d create failed\n", i);
			retval = PTR_ERR(dev);
			goto failed;
		}
		device_num++;

		my ++;
	}

	return 0;

failed:
	my = my_chardev;
	for (int i = 0; i < cdev_num; i++) {
		cdev_del(&my->cdev);
		my ++;
	}

	my = my_chardev;
	for (int i = 0; i < cdev_num; i++) {
		device_destroy(&my_chardev_class, MKDEV(major, i));
		my ++;
	}

	return -EFAULT;
}

static int __init my_chardev_init(void)
{
	int retval;

	retval = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "my_chardev");
	if (retval) {
		printk(KERN_ERR"alloc dev number failed\n");
		return retval;
	}
	
	retval = class_register(&my_chardev_class);
	if (retval) {
		printk(KERN_ERR"class register failed\n");
		goto failed1;
	}

	my = kzalloc(sizeof(*my) * DEVICE_NUM, GFP_KERNEL);
	if (!my) {
		printk(KERN_ERR"allock memory for my_chardev structure failed\n");
		goto failed2;
	}

	return my_chardev_setup(my);

failed2:
	class_unregister(&my_chardev_class);
failed1:
	unregister_chrdev_region(devno, DEVICE_NUM);
	return retval;
}

static void __exit my_chardev_exit(void)
{
	int major = MAJOR(devno);

	for (int i = 0; i < DEVICE_NUM; i++) {
		device_destroy(&my_chardev_class, MKDEV(major, i));
	}

	class_unregister(&my_chardev_class);

	for (int i = 0; i < DEVICE_NUM; i++) {
		cdev_del(&(my+i)->cdev);
	}

	unregister_chrdev_region(devno, DEVICE_NUM);

	kfree(my);
}

module_init(my_chardev_init);
module_exit(my_chardev_exit);

MODULE_LICENSE("GPL");
