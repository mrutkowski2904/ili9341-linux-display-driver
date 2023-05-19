#ifndef _ILI9341_DISPLAY_H
#define _ILI9341_DISPLAY_H

#include <linux/types.h>

#include "ili9341.h"

#define ILI9341_DC_DATA 1
#define ILI9341_DC_COMMAND 0

int ili9341_init(struct device_data *dev_data);

#endif /* _ILI9341_DISPLAY_H */