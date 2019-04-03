#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of.h>       // access device tree file
#include <linux/delay.h>
#include <linux/slab.h>      // kmalloc, kcallloc, ....
#include <linux/mutex.h>
#include <linux/reset-controller.h>     // reset controller
#include <linux/kref.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/reset.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("THOMASTHONG");
MODULE_VERSION("1.0.0");

#define PDEBUG(fmt,args...) printk(KERN_DEBUG"%s: "fmt,DRIVER_NAME, ##args)
#define PERR(fmt,args...) printk(KERN_ERR"%s: "fmt,DRIVER_NAME,##args)
#define PINFO(fmt,args...) printk(KERN_INFO"%s: "fmt,DRIVER_NAME, ##args)

#define DRIVER_NAME "reset_lpc11u68"
#define FIRST_MINOR 0
#define BUFF_SIZE 100

dev_t device_num ;
struct device * device;
struct gpio_desc *reset, *isp;
typedef struct privatedata {
    bool isp_mode ;
    bool asserted_value;
	uint32_t duration_ms;
    struct class device_class;
    struct reset_controller_dev	rcdev;
} private_data_t;


static int driver_probe (struct platform_device *pdev);
static int driver_remove(struct platform_device *pdev);

static const struct of_device_id reset_dst[]={
    //{ .compatible = "acme,foo", },
    { .compatible = "isp-reset", },
    {}
};

MODULE_DEVICE_TABLE(of, reset_dst);	

static struct platform_driver lpc11u68_reset_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,   
        .of_match_table = of_match_ptr (reset_dst),
    },
    .probe = driver_probe,
    .remove = driver_remove,
};

// reset function
void gpio_reset_assert(private_data_t *data)
{
    //gpiod_set_value(reset, data->asserted_value);
    gpiod_set_value(reset, 1);
}

void gpio_reset_deassert(private_data_t *data)
{
    //gpiod_set_value(reset, !data->asserted_value);
    gpiod_set_value(reset, 0);
}

int gpio_reset_status(private_data_t *data)
{
    return gpiod_get_value(reset);
} 

void gpio_reset_reset(private_data_t *data)
{
    gpio_reset_assert(data);
    msleep(data->duration_ms);
    gpio_reset_deassert(data);
}


// class attribute
static ssize_t isp_store(struct class *cls, struct class_attribute *attr, const char *buff, size_t len)
{
    private_data_t *priv = container_of(cls, private_data_t, device_class);
    PINFO ("Inside isp_store %d\n", priv);
    PINFO("sleep %d s, asserted %d, data name %s\n", priv->duration_ms, priv->asserted_value, priv->rcdev.owner);
    if (buff[0] == '1' && (len == 2))
    {
        gpiod_set_value(isp, 0);

        gpio_reset_reset(priv);

        gpiod_set_value(isp, 1);
        priv->isp_mode = true;
    } 
    return len;
}

static ssize_t isp_show(struct class *cls, struct class_attribute *attr, char *buf)
{ 
    private_data_t *priv = container_of(cls, private_data_t, device_class);
    int retval;
    PINFO ("Inside isp_show\n");

    retval = scnprintf(buf, PAGE_SIZE, "%s\n", priv->isp_mode ? "isp mode" : "normal mode");

    return retval;
}

static CLASS_ATTR_RW(isp);

static ssize_t reset_store(struct class *cls, struct class_attribute *attr, const char *buff, size_t len)
{
    private_data_t *priv = container_of(cls, private_data_t, device_class);
    PINFO("inside reset_store\n");

    gpio_reset_reset(priv);
    priv->isp_mode = false;

    return len;
} 

static CLASS_ATTR(reset, 00200, NULL, reset_store);

/***************************/
/*****module init + exit****/
/***************************/
int lpc11u68_reset_assert (struct reset_controller_dev *rcdev, unsigned long id)
{
    private_data_t *priv = container_of(rcdev, private_data_t, rcdev);
    gpio_reset_assert(priv);

    return 0;
}

int lpc11u68_reset_deassert (struct reset_controller_dev *rcdev, unsigned long id)
{
    private_data_t *priv = container_of(rcdev, private_data_t, rcdev);
    gpio_reset_deassert(priv);

    return 0;
}

int lpc11u68_reset_status (struct reset_controller_dev *rcdev, unsigned long id)
{
    private_data_t *priv = container_of(rcdev, private_data_t, rcdev);
    return gpio_reset_status(priv);
}

int lpc11u68_reset_reset (struct reset_controller_dev *rcdev, unsigned long id)
{
    private_data_t *priv = container_of(rcdev, private_data_t, rcdev);
    gpio_reset_reset(priv);

    return 0;
}

static struct reset_control_ops lpc11u68_reset_ops = {
    .reset      = lpc11u68_reset_assert,
	.assert		= lpc11u68_reset_assert,
	.deassert	= lpc11u68_reset_deassert,
    .status     = lpc11u68_reset_status,
};

static int driver_probe (struct platform_device *pdev)
{
    int retval;
    private_data_t *data;

    PINFO ("driver module init\n");
    
    PINFO ("node name %s\n",pdev->dev.of_node->name );
    // create private data
    data = (private_data_t*)kcalloc(1, sizeof(private_data_t), GFP_KERNEL);
    data->rcdev.owner = THIS_MODULE;
    data->rcdev.nr_resets = 1;
    data->rcdev.ops = &lpc11u68_reset_ops;
    data->rcdev.of_node = pdev->dev.of_node;
    data->isp_mode = false;


    reset =  gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(reset))
    {
        PINFO ("can't get reset gpio, error code: %d\n", reset);

        goto error_reset_gpio;
    }

    isp =  gpiod_get(&pdev->dev, "isp", GPIOD_OUT_HIGH);
    if (IS_ERR(isp))
    {
        PINFO ("can't get isp gpio, error code: %d\n", isp);

        goto error_isp_gpio;
    }

    //init gpio
    retval = gpiod_direction_output(reset, 0);
    if (retval)
    {
        PERR ("reset gpio can't set as output, error code: %d", retval); 

        goto error_gpio;   
    }

    retval = gpiod_direction_output(isp, 1);
    if (retval)
    {
        PERR ("isp gpio can't set as output, error code: %d", retval); 

        goto error_gpio;   
    }
    gpiod_set_value(reset, 0);
    gpiod_set_value(isp, 1);


    // class create
    /*
    data->device_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(&data->device_class))
    {
        PERR("Class create faili, error code: %d\n", PTR_ERR(&data->device_class));

        goto error_class_create;
    }
    */
    data->device_class.name = DRIVER_NAME;
    data->device_class.owner = THIS_MODULE;
    if (class_register(&data->device_class))
    {
        PERR("Class register fail\n");

        goto error_class_register;
    }

    retval = class_create_file(&data->device_class, &class_attr_reset);
    if (IS_ERR(retval))
    {
        PERR("Can't add class reset attribute\n");

        goto error_attr_isp;
    }
    retval = class_create_file(&data->device_class, &class_attr_isp);
    if (IS_ERR(retval))
    {
        PERR("Can't add class isp attribute\n");

        goto error_attr_reset;
    }

    // get device tree property
    retval = of_property_read_u32(pdev->dev.of_node, "asserted-state", &data->asserted_value);
    if (IS_ERR(retval))
        data->asserted_value = true;

	retval = of_property_read_u32(pdev->dev.of_node, "duration-ms", &data->duration_ms);
    PINFO("RETVAL %d\n", IS_ERR(retval));
    if (IS_ERR(retval))    
        data->duration_ms = 2000;
    PINFO("duration %d\n", data->duration_ms);
    if (of_property_read_bool(pdev->dev.of_node, "auto"))
		gpio_reset_reset(data);

    platform_set_drvdata(pdev, data);
    reset_controller_register(&data->rcdev);

    return 0;

    //error handle
error_attr_isp:
    class_remove_file (&data->device_class, &class_attr_isp);
error_attr_reset:
    class_unregister(&data->device_class);
error_class_register:
error_gpio:
    gpiod_put(isp);
error_isp_gpio:
    gpiod_put(reset);
error_reset_gpio:
    kfree(data);
    return -1;

}

static int driver_remove(struct platform_device *pdev)
{
    PINFO("driver module remove from kernel\n");
    
    private_data_t *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);

    class_remove_file(&data->device_class, &class_attr_isp);
    class_remove_file(&data->device_class, &class_attr_reset);
    class_unregister(&data->device_class);

    kfree(data);
    gpiod_put(reset);
    gpiod_put(isp);
    return 0;
}

static int __init gpio_reset_init(void)
{
	return platform_driver_register(&lpc11u68_reset_driver);
}
subsys_initcall(gpio_reset_init);

static void __exit gpio_reset_exit(void)
{
	platform_driver_unregister(&lpc11u68_reset_driver);
}
module_exit(gpio_reset_exit);
