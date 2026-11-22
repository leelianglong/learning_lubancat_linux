// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/ioctl.h>

#define DEV_NAME      "sys_led"
#define DEFAULT_MS    1000
#define MIN_MS        20          /* 太小会把 CPU 打满 */

/* ioctl：_IO(魔数, 序号)。用户态用同一组宏生成命令字 */
#define SYS_LED_MAGIC        'L'
#define SYS_LED_SET_PERIOD   _IOW(SYS_LED_MAGIC, 1, int)   /* 参数：周期 ms */

static dev_t          devno;
static struct cdev    cdev;
static struct class  *cls;
static struct device *dev;

/* 三样共享状态：都会被"进程上下文(ioctl/read/write)"和"软中断(timer)"同时访问 */
static bool           led_on;
static unsigned long  tick;
static unsigned long  period_ms = DEFAULT_MS;

/* ⚠️ 这里必须是 spinlock，不能是 mutex：
 *    timer 回调运行在软中断上下文，mutex 会睡眠 → BUG: scheduling while atomic */
static DEFINE_SPINLOCK(s_lock);
static struct timer_list my_timer;

static void sys_led_timer_fn(struct timer_list *t)
{
	unsigned long flags, period;

	/* TODO 1: spin_lock_irqsave(&s_lock, flags);
	 *         led_on = !led_on;
	 *         tick++;
	 *         spin_unlock_irqrestore(&s_lock, flags);
	 */
    spin_lock_irqsave(&s_lock, flags);
    led_on = !led_on;
    tick++;
    period = period_ms;
    spin_unlock_irqrestore(&s_lock, flags);
	/* TODO 2: 续期（定时器是一次性的，不续就不再来）
	 *         mod_timer(&my_timer, jiffies + msecs_to_jiffies(period_ms));
	 */
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(period));
}

static ssize_t sys_led_write(struct file *f, const char __user *ub,
			     size_t n, loff_t *ppos)
{
	bool val;
	int ret;
	unsigned long flags;

	/* TODO 3: kstrtobool_from_user(ub, n, &val) 解析 "1"/"0"/>
	 *         拿锁、led_on = val、放锁、返回 n
	 */
    ret = kstrtobool_from_user(ub, n, &val);
    if (ret) /* 成功返回0，失败返回错误码 */
        return ret;

    spin_lock_irqsave(&s_lock, flags);
    led_on = val;
    spin_unlock_irqrestore(&s_lock, flags);
	return n;
}

static ssize_t sys_led_read(struct file *f, char __user *ub,
			    size_t n, loff_t *ppos)
{
	/* TODO 4: 读 led_on，格式化 "1\n" 或 "0\n"，copy_to_user；
	 *         注意 *ppos >= 2 时返回 0（读完了）
	 */
    unsigned long flags;
    char buf[3] = {0};
    bool on = false;

    if (*ppos >= 2)
        return 0;
    if (n < 2)
        return -EINVAL;

    spin_lock_irqsave(&s_lock, flags);
    on = led_on;
    spin_unlock_irqrestore(&s_lock, flags);

    buf[0] = on ? '1' : '0';
    buf[1] = '\n';
    buf[2] = '\0';
    if (copy_to_user(ub, buf, 2))
        return -EFAULT; 

    *ppos += 2;
	return 2;
}

static long sys_led_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ms;
    unsigned long flags, period;
	switch (cmd) {
	case SYS_LED_SET_PERIOD:
		/* TODO 5: copy_from_user(&ms, (int __user *)arg, sizeof(ms))
		 *         校验 ms == 0（停止）或 ms >= MIN_MS，否则 -EINVAL
		 *         ms == 0 → del_timer_sync(&my_timer)
		 *         ms > 0  → 拿锁改 period_ms，放锁，mod_timer 重启
		 */
            if(copy_from_user(&ms, (int __user *)arg, sizeof(ms)))
                return -EFAULT;

            if (ms != 0 && ms < MIN_MS)
                return -EINVAL;
            if (ms == 0) {
                del_timer_sync(&my_timer);
                return 0;
            } 
            /* ms > 0：更新周期并重新武装 */
            spin_lock_irqsave(&s_lock, flags);
            period_ms = ms;
            period = period_ms;
            spin_unlock_irqrestore(&s_lock, flags);
            mod_timer(&my_timer, jiffies + msecs_to_jiffies(period));
		return 0;
	default:
		return -ENOTTY;
	}
}

static ssize_t tick_show(struct device *d, struct device_attribute *a, char *buf)
{
	/* TODO 6: 拿锁读 tick，sprintf 返回 */
    unsigned long flags;
    unsigned long t;
    spin_lock_irqsave(&s_lock, flags);
    t = tick;
    spin_unlock_irqrestore(&s_lock, flags);
	return sprintf(buf, "%lu\n", t);
}
static DEVICE_ATTR_RO(tick);

/* 1) 函数：放在 tick_show 附近 */
static ssize_t period_show(struct device *d, struct device_attribute *a, char *buf)
{
	unsigned long flags, p;

	spin_lock_irqsave(&s_lock, flags);
	p = period_ms;
	spin_unlock_irqrestore(&s_lock, flags);

	return sprintf(buf, "%lu\n", p);
}
static DEVICE_ATTR_RO(period);

static const struct file_operations sys_led_fops = {
	.owner          = THIS_MODULE,
	.write          = sys_led_write,
	.read           = sys_led_read,
	.unlocked_ioctl = sys_led_ioctl,
};

static int __init sys_led_init(void)
{
	/* 与 mychar 相同的注册流程（alloc_chrdev_region → cdev_add →
	 * class_create → device_create → device_create_file）
	 *
	 * TODO 7: timer_setup(&my_timer, sys_led_timer_fn, 0);
	 *         mod_timer(&my_timer, jiffies + msecs_to_jiffies(period_ms));
	 */
    int ret = alloc_chrdev_region(&devno, 0, 1, DEV_NAME);
    if (ret) {
        pr_err("sys_led: alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }
    cdev_init(&cdev, &sys_led_fops);
    cdev.owner = THIS_MODULE;
    ret = cdev_add(&cdev, devno, 1);
    if (ret) {
        pr_err("sys_led: cdev_add failed: %d\n", ret);
        unregister_chrdev_region(devno, 1);
        return ret;
    }
    cls = class_create(THIS_MODULE, DEV_NAME);
    if (IS_ERR(cls)) {
        ret = PTR_ERR(cls);
        cdev_del(&cdev);
        unregister_chrdev_region(devno, 1);
        return ret;
    }

    dev = device_create(cls, NULL, devno, NULL, DEV_NAME);
    if (IS_ERR(dev)) {
        ret = PTR_ERR(dev);
        class_destroy(cls);
        cdev_del(&cdev);
        unregister_chrdev_region(devno, 1);
        return ret;
    }

    ret = device_create_file(dev, &dev_attr_tick);
    if (ret) {
        device_destroy(cls, devno);
        class_destroy(cls);
        cdev_del(&cdev);        
        unregister_chrdev_region(devno, 1);
        return ret;
    }

    ret = device_create_file(dev, &dev_attr_period);
    if (ret) {
        device_destroy(cls, devno);
        class_destroy(cls);
        cdev_del(&cdev);
        unregister_chrdev_region(devno, 1);
        return ret;
    }

    timer_setup(&my_timer, sys_led_timer_fn, 0);
    mod_timer(&my_timer, jiffies + msecs_to_jiffies(period_ms));
    pr_info("%s: ready v2, major=%d, period=%lums\n", DEV_NAME, MAJOR(devno), period_ms);
	return 0;
}

static void __exit sys_led_exit(void)
{
	/* TODO 8: del_timer_sync(&my_timer);   ← 必须是第一行！
	 *         然后 device_remove_file → device_destroy → class_destroy
	 *         → cdev_del → unregister_chrdev_region
	 */
    del_timer_sync(&my_timer); // 注释掉 exit 里的 del_timer_sync,会导致整个设备死机
    pr_info("%s: exit\n", DEV_NAME);
    device_remove_file(dev, &dev_attr_tick);
    device_remove_file(dev, &dev_attr_period);
    device_destroy(cls, devno);
    class_destroy(cls);
    cdev_del(&cdev);
    unregister_chrdev_region(devno, 1);
}

module_init(sys_led_init);
module_exit(sys_led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lubancat");
MODULE_DESCRIPTION("sysfs LED driver with timer and ioctl");