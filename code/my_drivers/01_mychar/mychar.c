// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DEV_NAME  "mychar"
#define BUF_SIZE  256

static dev_t          devno;
static struct cdev    my_cdev;
static struct class  *my_class;
static struct device *my_device;

static char   my_buf[BUF_SIZE];
static size_t my_len;
static DEFINE_MUTEX(my_lock);          /* 保护 my_buf / my_len */

static int my_open(struct inode *inode, struct file *file)
{
	pr_info("mychar: open\n");
	return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
	pr_info("mychar: release\n");
	return 0;
}

static ssize_t my_read(struct file *file, char __user *ubuf,
		       size_t count, loff_t *ppos)
{
	ssize_t ret;

	mutex_lock(&my_lock);
	if (*ppos >= my_len) {          /* 已读完 */
		ret = 0;
		goto out;
	}
	if (count > my_len - *ppos)
		count = my_len - *ppos;
	if (copy_to_user(ubuf, my_buf + *ppos, count)) {
		ret = -EFAULT;
		goto out;
	}
	*ppos += count;
	ret = count;
out:
	mutex_unlock(&my_lock);
	return ret;
}

static ssize_t my_write(struct file *file, const char __user *ubuf,
			size_t count, loff_t *ppos)
{
	if (count > BUF_SIZE)
		count = BUF_SIZE;

	mutex_lock(&my_lock);
	if (copy_from_user(my_buf, ubuf, count)) {
		mutex_unlock(&my_lock);
		return -EFAULT;
	}
	my_len = count;
	mutex_unlock(&my_lock);

	pr_info("mychar: write %zu bytes\n", count);
	return count;
}

static const struct file_operations my_fops = {
	.owner   = THIS_MODULE,
	.open    = my_open,
	.release = my_release,
	.read    = my_read,
	.write   = my_write,
};

static int __init mychar_init(void)
{
	int ret;

	/* 1. 动态申请设备号 */
	ret = alloc_chrdev_region(&devno, 0, 1, DEV_NAME);
	if (ret) {
		pr_err("mychar: alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}

	/* 2. 注册 cdev */
	cdev_init(&my_cdev, &my_fops);
	my_cdev.owner = THIS_MODULE;
	ret = cdev_add(&my_cdev, devno, 1);
	if (ret) {
		pr_err("mychar: cdev_add failed: %d\n", ret);
		goto err_cdev;
	}

	/* 3. 自动创建设备节点 /dev/mychar（依赖 udev） */
	my_class = class_create(THIS_MODULE, DEV_NAME);   /* 4.19：两个参数 */
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		goto err_class;
	}
	my_device = device_create(my_class, NULL, devno, NULL, DEV_NAME);
	if (IS_ERR(my_device)) {
		ret = PTR_ERR(my_device);
		goto err_device;
	}

	pr_info("mychar: major=%d minor=%d ready\n",
		MAJOR(devno), MINOR(devno));
	return 0;

err_device:
	class_destroy(my_class);
err_class:
	cdev_del(&my_cdev);
err_cdev:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static void __exit mychar_exit(void)
{
	device_destroy(my_class, devno);
	class_destroy(my_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(devno, 1);
	pr_info("mychar: exit\n");
}

module_init(mychar_init);
module_exit(mychar_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cat");
MODULE_DESCRIPTION("LubanCat first char device");