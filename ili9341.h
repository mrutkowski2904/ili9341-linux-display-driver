#ifndef _ILI9341_H
#define _ILI9341_H

#include <linux/types.h>
#include <linux/kthread.h>

struct device_data
{
    struct task_struct *display_thread;
};

#endif /* _ILI9341_H */