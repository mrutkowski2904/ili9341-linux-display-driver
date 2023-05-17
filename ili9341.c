#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>

#include "ili9341.h"
#include "display.h"

static int ili9341_probe(struct spi_device *client);
static void ili9341_remove(struct spi_device *client);

static struct of_device_id ili9341_driver_of_ids[] = {
    {
        .compatible = "mr,custom_ili9341",
    },
    {/* NULL */},
};
MODULE_DEVICE_TABLE(of, ili9341_driver_of_ids);

static struct spi_driver ili9341_spi_driver = {
    .probe = ili9341_probe,
    .remove = ili9341_remove,
    .driver = {
        .name = "ili9341",
        .of_match_table = of_match_ptr(ili9341_driver_of_ids),
    },
};

static int ili9341_probe(struct spi_device *client)
{
    struct device_data *dev_data;
    dev_data = devm_kzalloc(&client->dev, sizeof(struct device_data), GFP_KERNEL);
    if (!dev_data)
        return -ENOMEM;

    dev_info(&client->dev, "ili9341 probe called\n");
    return 0;
}

static void ili9341_remove(struct spi_device *client)
{
    dev_info(&client->dev, "ili9341 remove\n");
}

module_spi_driver(ili9341_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maciej Rutkowski");
MODULE_DESCRIPTION("SPI driver for ILI9341 display");