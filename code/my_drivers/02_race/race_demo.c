#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/delay.h>


#define DEV_NAME  "race_dev"
#define WIN_US  50       /* 人为放大观察窗口，否则不变观测 */

static dev_t          devno;
static struct cdev    cdev;
static struct class   *cls;
static struct device  *dev;

/* 四个计数器，分别演示四种"保护"方式 */
static unsigned long cnt_plain;                       /* 无保护 */
static atomic_t      cnt_atomic = ATOMIC_INIT(0);     /* 原子变量 */
static unsigned long cnt_mutex;                       /* 受 m_lock 保护 */
static unsigned long cnt_spin;                        /* 受 s_lock 保护 */

static DEFINE_MUTEX(m_lock);
static DEFINE_SPINLOCK(s_lock);

static ssize_t demo_write(struct file *f, const char __user *ub,
			  size_t n, loff_t *ppos)
{
	unsigned long tmp;

	/* ① 无保护：读-改-写 三步不是原子的，中间可被抢走 */
	tmp = cnt_plain;
	udelay(WIN_US);                 /* 放大窗口：模拟"耗时的计算" */
	cnt_plain = tmp + 1;

	/* ② 原子变量：一条指令完成，不会被拆开 */
	atomic_inc(&cnt_atomic);

	/* ③ 互斥锁：临界区可睡眠（这里用 udelay 只为对比，实际可 msleep） */
	mutex_lock(&m_lock);
	tmp = cnt_mutex;
	udelay(WIN_US);
	cnt_mutex = tmp + 1;
	mutex_unlock(&m_lock);

	/* ④ 自旋锁：临界区不可睡眠，只适合极短操作 */
	spin_lock(&s_lock);
	tmp = cnt_spin;
	udelay(WIN_US);                 /* 自旋锁内只能 udelay，绝不能 msleep */
	cnt_spin = tmp + 1;
	spin_unlock(&s_lock);

	return n;
}

static const struct file_operations demo_fops = {
	.owner = THIS_MODULE,
	.write = demo_write,
};

/* /sys/class/racedev/racedev/stats —— 读四个计数 */
static ssize_t stats_show(struct device *d, struct device_attribute *a, char *buf)
{
	return sprintf(buf, "plain=%lu\natomic=%d\nmutex=%lu\nspin=%lu\n",
		       cnt_plain, atomic_read(&cnt_atomic), cnt_mutex, cnt_spin);
}
static DEVICE_ATTR_RO(stats);

/* /sys/class/racedev/racedev/reset —— 清零（写任意内容即可） */
static ssize_t reset_store(struct device *d, struct device_attribute *a,
			   const char *buf, size_t n)
{
	cnt_plain = 0;
	atomic_set(&cnt_atomic, 0);
	mutex_lock(&m_lock); cnt_mutex = 0; mutex_unlock(&m_lock);
	spin_lock(&s_lock);  cnt_spin  = 0; spin_unlock(&s_lock);
	return n;
}
static DEVICE_ATTR_WO(reset);

static int __init racedev_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&devno, 0, 1, DEV_NAME);
	if (ret)
		return ret;

	cdev_init(&cdev, &demo_fops);
	cdev.owner = THIS_MODULE;
	ret = cdev_add(&cdev, devno, 1);
	if (ret)
		goto err_cdev;

	cls = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(cls)) {
		ret = PTR_ERR(cls);
		goto err_class;
	}

	dev = device_create(cls, NULL, devno, NULL, DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto err_dev;
	}

	device_create_file(dev, &dev_attr_stats);
	device_create_file(dev, &dev_attr_reset);

	pr_info("racedev: ready, major=%d\n", MAJOR(devno));
	return 0;

err_dev:
	class_destroy(cls);
err_class:
	cdev_del(&cdev);
err_cdev:
	unregister_chrdev_region(devno, 1);
	return ret;
}

static void __exit racedev_exit(void)
{
	device_remove_file(dev, &dev_attr_reset);
	device_remove_file(dev, &dev_attr_stats);
	device_destroy(cls, devno);
	class_destroy(cls);
	cdev_del(&cdev);
	unregister_chrdev_region(devno, 1);
	pr_info("racedev: exit\n");
}

module_init(racedev_init);
module_exit(racedev_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LubanCat concurrency demo: plain/atomic/mutex/spinlock");