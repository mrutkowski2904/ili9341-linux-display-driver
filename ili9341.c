#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/fb.h>

#include "ili9341.h"
#include "display.h"

static int display_thread(void *data);
static int ili9341_probe(struct spi_device *client);
static int ili9341_remove(struct spi_device *client);
int ili9341_setcolreg(unsigned regno, unsigned red, unsigned green,
                      unsigned blue, unsigned transp, struct fb_info *info);
int ili9341_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
static void ili9341_configure_fb(struct device_data *dev_data);

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
    .fb_check_var = ili9341_check_var,
    .fb_setcolreg = ili9341_setcolreg,
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

    dev_data->framebuffer_info = framebuffer_alloc(0, &client->dev);
    if (dev_data->framebuffer_info == NULL)
    {
        gpiod_put(dev_data->dc_gpio);
        return -ENOMEM;
    }
    if (fb_alloc_cmap(&dev_data->framebuffer_info->cmap, ILI9341_BITS_PER_PIXEL, 0))
    {
        gpiod_put(dev_data->dc_gpio);
        framebuffer_release(dev_data->framebuffer_info);
        return -ENOMEM;
    }

    ili9341_configure_fb(dev_data);

    if (register_framebuffer(dev_data->framebuffer_info))
    {
        fb_dealloc_cmap(&dev_data->framebuffer_info->cmap);
        framebuffer_release(dev_data->framebuffer_info);
        gpiod_put(dev_data->dc_gpio);
        dev_err(&client->dev, "error while registering framebuffer\n");
        return -EINVAL;
    }

    dev_data->display_thread = kthread_create(display_thread, dev_data, "ili9341_kthread");
    if (!dev_data->display_thread)
    {
        fb_dealloc_cmap(&dev_data->framebuffer_info->cmap);
        framebuffer_release(dev_data->framebuffer_info);
        gpiod_put(dev_data->dc_gpio);
        return -ECHILD;
    }

    wake_up_process(dev_data->display_thread);
    dev_info(&client->dev, "probe successful\n");
    return 0;
}

static int ili9341_remove(struct spi_device *client)
{
    struct device_data *dev_data;
    dev_data = spi_get_drvdata(client);

    kthread_stop(dev_data->display_thread);
    gpiod_put(dev_data->dc_gpio);
    unregister_framebuffer(dev_data->framebuffer_info);
    fb_dealloc_cmap(&dev_data->framebuffer_info->cmap);
    framebuffer_release(dev_data->framebuffer_info);
    dev_info(&client->dev, "device removed\n");
    return 0;
}

int ili9341_setcolreg(unsigned regno, unsigned red, unsigned green,
                      unsigned blue, unsigned transp, struct fb_info *info)
{
    if (info->fix.visual == FB_VISUAL_TRUECOLOR && regno < ILI9341_PSEUDO_PALETTE_SIZE)
    {
        ((u32 *)(info->pseudo_palette))[regno] =
            ((red & 0x3F) << ILI9341_RED_OFFSET) 
            | ((green & 0x3F) << ILI9341_GREEN_OFFSET) 
            | ((blue & 0x3F) << ILI9341_BLUE_OFFSET);
        return 0;
    }
    return -EINVAL;
}

int ili9341_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
    var->xres = ILI9341_WIDTH;
    var->yres = ILI9341_HEIGHT;
    var->xres_virtual = ILI9341_WIDTH;
    var->yres_virtual = ILI9341_HEIGHT;
    var->bits_per_pixel = ILI9341_BITS_PER_PIXEL;
    var->grayscale = 0;

    var->red.offset = ILI9341_RED_OFFSET;
    var->red.length = ILI9341_COLOR_LENGTH;
    var->red.msb_right = 0;

    var->green.offset = ILI9341_GREEN_OFFSET;
    var->green.length = ILI9341_COLOR_LENGTH;
    var->green.msb_right = 0;

    var->blue.offset = ILI9341_BLUE_OFFSET;
    var->blue.length = ILI9341_COLOR_LENGTH;
    var->blue.msb_right = 0;

    var->transp.offset = 0;
    var->transp.length = 0;
    var->transp.msb_right = 0;
    return 0;
}

static void ili9341_configure_fb(struct device_data *dev_data)
{
    struct fb_info *info;

    info = dev_data->framebuffer_info;
    info->screen_base = dev_data->display_buff;
    info->screen_size = ILI9341_BUFFER_SIZE;
    info->fbops = &ili9341_fb_ops;
    info->pseudo_palette = dev_data->pseudo_palette;

    info->fix.visual = FB_VISUAL_TRUECOLOR;
    info->fix.type = FB_TYPE_PACKED_PIXELS;
    info->fix.line_length = ILI9341_LINE_LENGTH;
    info->fix.accel = FB_ACCEL_NONE;
    sprintf(info->fix.id, "CUST_ILI9341");

    info->var.xres = ILI9341_WIDTH;
    info->var.yres = ILI9341_HEIGHT;
    info->var.xres_virtual = ILI9341_WIDTH;
    info->var.yres_virtual = ILI9341_HEIGHT;
    info->var.bits_per_pixel = ILI9341_BITS_PER_PIXEL;
    info->var.grayscale = 0;
    info->var.activate = FB_ACTIVATE_NOW;

    info->var.red.offset = ILI9341_RED_OFFSET;
    info->var.red.length = ILI9341_COLOR_LENGTH;
    info->var.red.msb_right = 0;

    info->var.green.offset = ILI9341_GREEN_OFFSET;
    info->var.green.length = ILI9341_COLOR_LENGTH;
    info->var.green.msb_right = 0;

    info->var.blue.offset = ILI9341_BLUE_OFFSET;
    info->var.blue.length = ILI9341_COLOR_LENGTH;
    info->var.blue.msb_right = 0;

    info->var.transp.offset = 0;
    info->var.transp.length = 0;
    info->var.transp.msb_right = 0;
}

module_spi_driver(ili9341_spi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maciej Rutkowski");
MODULE_DESCRIPTION("SPI driver for ILI9341 display");