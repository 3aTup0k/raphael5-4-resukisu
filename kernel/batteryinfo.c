#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/math64.h>

#define SRC_CHARGE_FULL     "/sys/class/power_supply/bms/charge_full"
#define SRC_CHARGE_FULL_DESIGN "/sys/class/power_supply/bms/charge_full_design"
#define SRC_CYCLE           "/sys/class/power_supply/bms/cycle_count"

static struct class *fuel_class;

static int read_int_from_file(const char *path, int *value)
{
    struct file *file;
    ssize_t ret;
    loff_t pos = 0;
    char *tmp_buf;
    mm_segment_t old_fs;
    int result = -EINVAL;

    file = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(file))
        return PTR_ERR(file);

    tmp_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!tmp_buf) {
        filp_close(file, NULL);
        return -ENOMEM;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    ret = file->f_op->read(file, tmp_buf, PAGE_SIZE - 1, &pos);
    set_fs(old_fs);

    if (ret > 0) {
        tmp_buf[ret] = '\0';
        result = kstrtoint(tmp_buf, 10, value);
    }

    kfree(tmp_buf);
    filp_close(file, NULL);
    return result == 0 ? 0 : -EIO;
}

static ssize_t read_source_file(const char *path, char *buf, size_t count)
{
    struct file *file;
    ssize_t ret;
    loff_t pos = 0;
    char *tmp_buf;
    mm_segment_t old_fs;

    file = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(file))
        return PTR_ERR(file);

    tmp_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!tmp_buf) {
        filp_close(file, NULL);
        return -ENOMEM;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    ret = file->f_op->read(file, tmp_buf, PAGE_SIZE - 1, &pos);
    set_fs(old_fs);

    if (ret >= 0) {
        tmp_buf[ret] = '\0';
        snprintf(buf, count, "%s", tmp_buf);
    }

    kfree(tmp_buf);
    filp_close(file, NULL);
    return ret;
}

static ssize_t soh_show(struct class *cls, struct class_attribute *attr, char *buf)
{
    int charge_full, charge_full_design;
    int ret;
    int soh_value;

    ret = read_int_from_file(SRC_CHARGE_FULL, &charge_full);
    if (ret < 0)
        return ret;

    ret = read_int_from_file(SRC_CHARGE_FULL_DESIGN, &charge_full_design);
    if (ret < 0)
        return ret;

    if (charge_full_design == 0)
        return -EINVAL;

    soh_value = (int)div64_s64((s64)charge_full * 100, charge_full_design);

    return snprintf(buf, PAGE_SIZE, "%d\n", soh_value);
}

static ssize_t cycle_show(struct class *cls, struct class_attribute *attr, char *buf)
{
    return read_source_file(SRC_CYCLE, buf, PAGE_SIZE);
}

static CLASS_ATTR_RO(soh);
static CLASS_ATTR_RO(cycle);

static int __init fuelsummary_init(void)
{
    int ret;

    fuel_class = class_create(THIS_MODULE, "fuelsummary");
    if (IS_ERR(fuel_class))
        return PTR_ERR(fuel_class);

    ret = class_create_file(fuel_class, &class_attr_soh);
    if (ret)
        goto err_soh;

    ret = class_create_file(fuel_class, &class_attr_cycle);
    if (ret)
        goto err_cycle;

    return 0;

err_cycle:
    class_remove_file(fuel_class, &class_attr_soh);
err_soh:
    class_destroy(fuel_class);
    return ret;
}

static void __exit fuelsummary_exit(void)
{
    class_remove_file(fuel_class, &class_attr_cycle);
    class_remove_file(fuel_class, &class_attr_soh);
    class_destroy(fuel_class);
}

module_init(fuelsummary_init);
module_exit(fuelsummary_exit);

MODULE_LICENSE("GPL");