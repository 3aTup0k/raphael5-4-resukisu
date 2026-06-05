#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "dsi_display.h"
#include "dsi_panel.h"

#define HBM_CLASS_NAME "hbm"
#define HBM_NODE_NAME   "hbm"

static struct class *hbm_class;
static struct device *hbm_device;
static DEFINE_MUTEX(hbm_lock);
static bool hbm_enabled;

static struct dsi_display *get_main_display(void)
{
    void *display_array[MAX_DSI_ACTIVE_DISPLAY];
    int num_displays;
    
    num_displays = dsi_display_get_active_displays(display_array, 
                                                     MAX_DSI_ACTIVE_DISPLAY);
    if (num_displays > 0)
        return (struct dsi_display *)display_array[0];
    
    return NULL;
}

static ssize_t hbm_store(struct device *dev,
                         struct device_attribute *attr,
                         const char *buf, size_t count)
{
    unsigned int val;
    int ret;
    struct dsi_display *display;
    struct dsi_panel *panel;

    ret = kstrtouint(buf, 10, &val);
    if (ret)
        return ret;

    display = get_main_display();
    if (!display || !display->panel)
        return -ENODEV;

    panel = display->panel;

    mutex_lock(&display->display_lock);

    if (val)
        ret = dsi_panel_set_fod_hbm(panel, true);
    else
        ret = dsi_panel_set_fod_hbm(panel, false);

    mutex_unlock(&display->display_lock);

    if (ret)
        return ret;

    hbm_enabled = (val != 0);
    return count;
}

static DEVICE_ATTR_WO(hbm);

static struct attribute *hbm_attrs[] = {
    &dev_attr_hbm.attr,
    NULL,
};

static struct attribute_group hbm_attr_group = {
    .attrs = hbm_attrs,
};

static int __init nyako_hbm_init(void)
{
    int ret;

    pr_info("nyako_hbm: initializing HBM driver\n");

    hbm_class = class_create(THIS_MODULE, HBM_CLASS_NAME);
    if (IS_ERR(hbm_class)) {
        pr_err("nyako_hbm: failed to create class\n");
        return PTR_ERR(hbm_class);
    }

    hbm_device = device_create(hbm_class, NULL, 0, NULL, HBM_NODE_NAME);
    if (IS_ERR(hbm_device)) {
        pr_err("nyako_hbm: failed to create device\n");
        ret = PTR_ERR(hbm_device);
        goto destroy_class;
    }

    ret = sysfs_create_group(&hbm_device->kobj, &hbm_attr_group);
    if (ret) {
        pr_err("nyako_hbm: failed to create sysfs group\n");
        goto destroy_device;
    }

    pr_info("nyako_hbm: initialized successfully at /sys/class/hbm/hbm\n");
    return 0;

destroy_device:
    device_destroy(hbm_class, 0);
destroy_class:
    class_destroy(hbm_class);
    return ret;
}

static void __exit nyako_hbm_exit(void)
{
    pr_info("nyako_hbm: exiting HBM driver\n");

    sysfs_remove_group(&hbm_device->kobj, &hbm_attr_group);
    device_destroy(hbm_class, 0);
    class_destroy(hbm_class);

    pr_info("nyako_hbm: exited\n");
}

module_init(nyako_hbm_init);
module_exit(nyako_hbm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mochizuki_Nyako");
MODULE_DESCRIPTION("K20Pro Local FOD HBM Driver");
MODULE_VERSION("1.0");
