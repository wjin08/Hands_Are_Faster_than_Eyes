#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#define LEDKEY_DEV_NAME "ledkey"
#define LEDKEY_DEV_MAJOR 230
#define LED_COUNT 8

static int gpioLed[LED_COUNT] = {528, 529, 530, 531, 532, 533, 534, 535};

static int openFlag = 0;

static int ledkey_open(struct inode *inode, struct file *filp)
{
    if (openFlag)
        return -EBUSY;
    openFlag = 1;
    
    if (!try_module_get(THIS_MODULE))
        return -ENODEV;
    
    printk("ledkey device opened\n");
    return 0;
}

static ssize_t ledkey_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    unsigned char kbuf;
    int i;
    
    get_user(kbuf, buf);
    
    for (i = 0; i < LED_COUNT; i++)
    {
        gpio_set_value(gpioLed[i], (kbuf & (0x01 << i)) ? 1 : 0);
    }
    
    printk("LED write: 0x%02x\n", kbuf);
    return count;
}

static ssize_t ledkey_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    char kbuf = 0;
    put_user(kbuf, buf);
    return count;
}

static int ledkey_release(struct inode *inode, struct file *filp)
{
    module_put(THIS_MODULE);
    openFlag = 0;
    printk("ledkey device closed\n");
    return 0;
}

static struct file_operations ledkey_fops = {
    .owner = THIS_MODULE,
    .open = ledkey_open,
    .write = ledkey_write,
    .read = ledkey_read,
    .release = ledkey_release,
};

static int __init ledkey_init(void)
{
    int result;
    int i;
    char gpioName[10];
    
    printk("LED Driver Init\n");
    
    for (i = 0; i < LED_COUNT; i++)
    {
        sprintf(gpioName, "led%d", i);
        result = gpio_request(gpioLed[i], gpioName);
        if (result < 0)
        {
            printk("Failed to request gpio%d\n", gpioLed[i]);
            goto error_gpio;
        }
        
        result = gpio_direction_output(gpioLed[i], 0);
        if (result < 0)
        {
            printk("Failed to set gpio%d as output\n", gpioLed[i]);
            goto error_gpio;
        }
    }
    
    result = register_chrdev(LEDKEY_DEV_MAJOR, LEDKEY_DEV_NAME, &ledkey_fops);
    if (result < 0)
    {
        printk("Failed to register device\n");
        goto error_gpio;
    }
    
    return 0;

error_gpio:
    for (i--; i >= 0; i--)
    {
        gpio_free(gpioLed[i]);
    }
    return result;
}

static void __exit ledkey_exit(void)
{
    int i;
    
    unregister_chrdev(LEDKEY_DEV_MAJOR, LEDKEY_DEV_NAME);
    
    for (i = 0; i < LED_COUNT; i++)
    {
        gpio_set_value(gpioLed[i], 0);
        gpio_free(gpioLed[i]);
    }
    
    printk("LED Driver Exit\n");
}

module_init(ledkey_init);
module_exit(ledkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BJH");
MODULE_DESCRIPTION("Simple LED Control Driver");
