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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("THOMASTHONG");
MODULE_VERSION("1.0.0");

#define PDEBUG(fmt,args...) printk(KERN_DEBUG"%s: "fmt,DRIVER_NAME, ##args)
#define PERR(fmt,args...) printk(KERN_ERR"%s: "fmt,DRIVER_NAME,##args)
#define PINFO(fmt,args...) printk(KERN_INFO"%s: "fmt,DRIVER_NAME, ##args)

#define DRIVER_NAME "LPC11U68_reset"
#define FIRST_MINOR 0
#define BUFF_SIZE 100

dev_t device_num ;
struct class * device_class;
struct device * device;
struct gpio_desc *reset, *isp;
typedef struct privatedata {
    //some orther data

    struct reset_controller_dev	rcdev;
} private_data_t;

private_data_t *data;



static int driver_probe (struct platform_device *pdev);
static int driver_remove(struct platform_device *pdev);

static const struct of_device_id reset_dst[]={
    //{ .compatible = "acme,foo", },
    { .compatible = "isp-reset", },
    {}
};

MODULE_DEVICE_TABLE(of, reset_dst);	

static struct platform_driver reset_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,   
        .of_match_table = of_match_ptr (reset_dst),
    },
    .probe = driver_probe,
    .remove = driver_remove,
};

/***************************/
/*****module init + exit****/
/***************************/
int LPC11U68_reset_assert (struct reset_controller_dev *rcdev, unsigned long id)
{
    gpiod_set_value(reset, 1);

    return 0;
}

int LPC11U68_reset_deassert (struct reset_controller_dev *rcdev, unsigned long id)
{
    gpiod_set_value(reset, 0);

    return 0;
}

int LPC11U68_reset_status (struct reset_controller_dev *rcdev, unsigned long id)
{
    return gpiod_get_value(reset);
}

int LPC11U68_reset_reset (struct reset_controller_dev *rcdev, unsigned long id)
{
    LPC11U68_reset_assert(rcdev, id);
	msleep(1000);
	LPC11U68_reset_deassert(rcdev, id);

    return 0;
}

static struct reset_control_ops LPC11U68_reset_ops = {
    .reset      = LPC11U68_reset_assert,
	.assert		= LPC11U68_reset_assert,
	.deassert	= LPC11U68_reset_deassert,
    .status     = LPC11U68_reset_status,
};

static int driver_probe (struct platform_device *pdev)
{
    int res;

    PINFO ("driver module init\n");
    
    PINFO ("node name %s\n",pdev->dev.of_node->name );
    // create private data
    data = (private_data_t*)kcalloc(1, sizeof(private_data_t), GFP_KERNEL);
    data->rcdev.owner = THIS_MODULE;
    data->rcdev.nr_resets = 1;
    data->rcdev.ops = &LPC11U68_reset_ops;
    data->rcdev.of_node = pdev->dev.of_node;

    platform_set_drvdata(pdev, data);

    reset =  gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(reset))
    {
        PINFO ("can't get reset gpio, error code: %d\n", reset);

        goto error_gpio;
    }

    //isp = gpiod_get_index(&pdev->dev, "led", 1, GPIOD_OUT_HIGH);
    isp =  gpiod_get(&pdev->dev, "isp", GPIOD_OUT_HIGH);
    if (IS_ERR(isp))
    {
        PINFO ("can't get isp gpio, error code: %d\n", isp);

        goto error_isp_gpio;
    }

    //init gpio
    res = gpiod_direction_output(reset, 0);
    if (res)
    {
        PERR ("reset gpio can't set as output, error code: %d", res); 

        goto error_gpio;   
    }

    res = gpiod_direction_output(isp, 1);
    if (res)
    {
        PERR ("isp gpio can't set as output, error code: %d", res); 

        goto error_gpio;   
    }
    return  reset_controller_register(&data->rcdev);

    //error handle
error_gpio:
    gpiod_put(isp);
error_isp_gpio:
    gpiod_put(reset);
error:
    return -1;

}

static int driver_remove(struct platform_device *pdev)
{
    PINFO("driver module remove from kernel\n");
    
    private_data_t *data = platform_get_drvdata(pdev);

	reset_controller_unregister(&data->rcdev);

    kfree(data);
    gpiod_put(reset);
    gpiod_put(isp);
    return 0;
}

module_platform_driver(reset_driver);
