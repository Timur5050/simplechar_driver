#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple char device driver with single device");
MODULE_VERSION("1.0");

static int simplechar_major = 0;  // 0 для динамічного виділення
module_param(simplechar_major, int, S_IRUGO);

// Структура для пристрою
struct simplechar_dev {
    char *data;            
    unsigned long size;    
    struct cdev cdev;      
};

// Глобальні змінні
static struct simplechar_dev simplechar_device;
static dev_t simplechar_devno;  // major + minor
static struct class *simplechar_class;  // Для створення /dev/simplechar
#define BUFFER_SIZE 1024 

static int simplechar_open(struct inode *inode, struct file *filp)
{
    // Зберігаємо вказівник на пристрій у private_data
    filp->private_data = &simplechar_device;
    printk(KERN_INFO "simplechar: Opened device, major=%d, minor=%d\n",
           MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
    return 0;
}

static int simplechar_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "simplechar: Released device, major=%d, minor=%d\n",
           MAJOR(inode->i_rdev), MINOR(inode->i_rdev));
    return 0;
}
 
static ssize_t simplechar_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct simplechar_dev *dev = filp->private_data;
    ssize_t retval = 0;

    if (*f_pos >= dev->size)
        return 0;  

    // reduce count
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    if (copy_to_user(buf, dev->data + *f_pos, count)) {
        printk(KERN_ERR "simplechar: Failed to copy data to user\n");
        return -EFAULT;
    }

    *f_pos += count;
    retval = count;
    printk(KERN_INFO "simplechar: Read %zd bytes from pos %lld\n", count, *f_pos);
    return retval;
}

static ssize_t simplechar_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct simplechar_dev *dev = filp->private_data;
    ssize_t retval = 0;

    if (*f_pos + count > BUFFER_SIZE) {
        count = BUFFER_SIZE - *f_pos;
        if (count == 0) {
            printk(KERN_ERR "simplechar: Buffer full\n");
            return -ENOSPC;
        }
    }

    if (copy_from_user(dev->data + *f_pos, buf, count)) {
        printk(KERN_ERR "simplechar: Failed to copy data from user\n");
        return -EFAULT;
    }

    *f_pos += count;
    if (dev->size < *f_pos)
        dev->size = *f_pos;
    retval = count;
    printk(KERN_INFO "simplechar: Wrote %zd bytes to pos %lld\n", count, *f_pos);
    return retval;
}

static struct file_operations simplechar_fops = {
    .owner = THIS_MODULE,
    .open = simplechar_open,
    .release = simplechar_release,
    .read = simplechar_read,
    .write = simplechar_write,
};

static int __init simplechar_init(void)
{
    int err;

    printk(KERN_INFO "simplechar: Initializing module\n");

    // Виділення номера пристрою
    if (simplechar_major) {
        simplechar_devno = MKDEV(simplechar_major, 0);
        err = register_chrdev_region(simplechar_devno, 1, "simplechar");
    } else {
        err = alloc_chrdev_region(&simplechar_devno, 0, 1, "simplechar");
        simplechar_major = MAJOR(simplechar_devno);
    }
    if (err < 0) {
        printk(KERN_ERR "simplechar: Failed to allocate device number\n");
        return err;
    }

    // Ініціалізація структури пристрою
    simplechar_device.data = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!simplechar_device.data) {
        printk(KERN_ERR "simplechar: Failed to allocate buffer\n");
        err = -ENOMEM;
        goto fail_alloc;
    }
    simplechar_device.size = 0;
    memset(simplechar_device.data, 0, BUFFER_SIZE);

    // Ініціалізація cdev
    cdev_init(&simplechar_device.cdev, &simplechar_fops);
    simplechar_device.cdev.owner = THIS_MODULE;
    err = cdev_add(&simplechar_device.cdev, simplechar_devno, 1);
    if (err) {
        printk(KERN_ERR "simplechar: Failed to add cdev\n");
        goto fail_cdev;
    }

    // Створення файлу в /dev
    simplechar_class = class_create("simplechar");
    if (IS_ERR(simplechar_class)) {
        err = PTR_ERR(simplechar_class);
        printk(KERN_ERR "simplechar: Failed to create class\n");
        goto fail_class;
    }
    device_create(simplechar_class, NULL, simplechar_devno, NULL, "simplechar");

    printk(KERN_INFO "simplechar: Module loaded, major=%d\n", simplechar_major);
    return 0;

fail_class:
    cdev_del(&simplechar_device.cdev);
fail_cdev:
    kfree(simplechar_device.data);
fail_alloc:
    unregister_chrdev_region(simplechar_devno, 1);
    return err;
}

static void __exit simplechar_exit(void)
{
    // Видалення файлу в /dev
    device_destroy(simplechar_class, simplechar_devno);
    class_destroy(simplechar_class);

    // Видалення cdev
    cdev_del(&simplechar_device.cdev);

    // Звільнення пам’яті
    kfree(simplechar_device.data);

    // Звільнення номера пристрою
    unregister_chrdev_region(simplechar_devno, 1);

    printk(KERN_INFO "simplechar: Module unloaded\n");
}

module_init(simplechar_init);
module_exit(simplechar_exit);