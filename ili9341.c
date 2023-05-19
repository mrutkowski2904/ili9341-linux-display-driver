#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/fb.h>

#include "ili9341.h"
#include "display.h"

static int display_thread(void *data);
static int ili9341_probe(struct spi_device *client);
static void ili9341_remove(struct spi_device *client);

static struct of_device_id ili9341_driver_of_ids[] = {
    {
        .compatible = "mr,custom_ili9341",
    },
    {/* NULL */},
};
MODULE_DEVICE_TABLE(of, ili9341_driver_of_ids);

static struct spi_device_id ili9341_ids[] = {
    {"custom_ili9341", 0},
    {/* NULL */},
};
MODULE_DEVICE_TABLE(spi, ili9341_ids);

static struct spi_driver ili9341_spi_driver = {
    .probe = ili9341_probe,
    .remove = ili9341_remove,
    .driver = {
        .name = "custom_ili9341",
        .of_match_table = of_match_ptr(ili9341_driver_of_ids),
    },
};

static struct fb_ops ili9341_fb_ops = {
    .owner = THIS_MODULE,
    .fb_fillrect = cfb_fillrect,
    .fb_imageblit = cfb_imageblit,
    .fb_copyarea = cfb_copyarea,
};

static int display_thread(void *data)
{
    struct device_data *dev_data;
    dev_data = data;

    while (!kthread_should_stop())
    {
        msleep(700);
        if (ili9341_send_display_buff(dev_data))
        {
            dev_err(&dev_data->client->dev, "error occured while sending data to the display");
            break;
        }
    }
    return 0;
}

static int ili9341_probe(struct spi_device *client)
{
    int status;
    struct device_data *dev_data;

    if (!device_property_present(&client->dev, "dc-gpios"))
    {
        dev_err(&client->dev, "device tree property dc-gpios does not exist\n");
        return -EINVAL;
    }

    dev_data = devm_kzalloc(&client->dev, sizeof(struct device_data), GFP_KERNEL);
    if (!dev_data)
        return -ENOMEM;

    dev_data->display_buff = devm_kzalloc(&client->dev, ILI9341_BUFFER_SIZE * sizeof(u8), GFP_KERNEL);
    if (!dev_data->display_buff)
        return -ENOMEM;

    spi_set_drvdata(client, dev_data);
    dev_data->client = client;

    dev_data->dc_gpio = gpiod_get(&client->dev, "dc", GPIOD_OUT_HIGH);
    if (IS_ERR(dev_data->dc_gpio))
    {
        dev_err(&client->dev, "could not setup dc gpio\n");
        return PTR_ERR(dev_data->dc_gpio);
    }

    status = ili9341_init(dev_data);
    if (status)
    {
        gpiod_put(dev_data->dc_gpio);
        dev_err(&client->dev, "error while initialising display hardware\n");
        return status;
    }

    dev_data->display_thread = kthread_create(display_thread, dev_data, "ili9341_kthread");
    if (!dev_data->display_thread)
    {
        gpiod_put(dev_data->dc_gpio);
        return -ECHILD;
    }

    wake_up_process(dev_data->display_thread);
    dev_info(&client->dev, "probe successful\n");
    return 0;
}

static void ili9341_remove(struct spi_device *client)
{
    struct device_data *dev_data;
    dev_data = spi_get_drvdata(client);

    kthread_stop(dev_data->display_thread);
    gpiod_put(dev_data->dc_gpio);
    dev_info(&client->dev, "device removed\n");
}

module_spi_driver(ili9341_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maciej Rutkowski");
MODULE_DESCRIPTION("SPI driver for ILI9341 display");