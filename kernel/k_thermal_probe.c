// SPDX-License-Identifier: GPL-2.0
/*
 * Experimental placeholder for deep thermal probing.
 *
 * This file is intentionally conservative. The original draft had ambitious
 * ideas around per-CPU MSR reads, PCI probing, thermal-zone iteration, and
 * possible BMC/IPMI integration, but parts of that draft were not yet ready
 * to ship as buildable kernel code.
 *
 * For this repository revision, the kernel path stays isolated and explicit:
 * it marks the intended privileged extension point without pretending to be a
 * production-ready module.
 */

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("thermal-observatory");
MODULE_DESCRIPTION("Experimental kernel thermal probe scaffold");
MODULE_VERSION("0.1.0");

static ssize_t kthermal_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
    static const char payload[] =
        "{"
        "\"status\":\"experimental\","
        "\"message\":\"kernel collector scaffold only; implement MSR and thermal-zone hooks incrementally\""
        "}\n";

    return simple_read_from_buffer(buf, len, ppos, payload, sizeof(payload) - 1);
}

static const struct file_operations kthermal_fops = {
    .owner = THIS_MODULE,
    .read = kthermal_read,
};

static int __init kthermal_init(void)
{
    pr_info("k_thermal_probe: experimental scaffold loaded\n");
    pr_info("k_thermal_probe: next steps are safe per-CPU MSR reads and audited IOCTL design\n");
    return 0;
}

static void __exit kthermal_exit(void)
{
    pr_info("k_thermal_probe: unloaded\n");
}

module_init(kthermal_init);
module_exit(kthermal_exit);

