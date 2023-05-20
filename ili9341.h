#ifndef _ILI9341_H
#define _ILI9341_H

#include <linux/types.h>
#include <linux/kthread.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/fb.h>

#define ILI9341_PSEUDO_PALETTE_SIZE 16

struct device_data
{
    struct spi_device *client;
    struct task_struct *display_thread;
    struct gpio_desc *dc_gpio;
    u32 pseudo_palette[ILI9341_PSEUDO_PALETTE_SIZE];
    u8 *display_buff;
    struct fb_info *framebuffer_info;
};

#endif /* _ILI9341_H */