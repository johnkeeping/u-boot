/*
 * Copyright (C) 2015 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <libfdt.h>
#include <power/act8846_pmic.h>
#include <power/pmic.h>

DECLARE_GLOBAL_DATA_PTR;

struct act8846_platdata {
	bool system_power_controller;
};

static const struct pmic_child_info pmic_children_info[] = {
	{ .prefix = "REG", .driver = "act8846_reg"},
	{ },
};

static int act8846_reg_count(struct udevice *dev)
{
	return ACT8846_NUM_OF_REGS;
}

static int act8846_write(struct udevice *dev, uint reg, const uint8_t *buff,
			  int len)
{
	if (dm_i2c_write(dev, reg, buff, len)) {
		debug("write error to device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int act8846_read(struct udevice *dev, uint reg, uint8_t *buff, int len)
{
	if (dm_i2c_read(dev, reg, buff, len)) {
		debug("read error from device: %p register: %#x!", dev, reg);
		return -EIO;
	}

	return 0;
}

static int act8846_poweroff(struct udevice *dev)
{
	struct act8846_platdata *plat = dev_get_platdata(dev);
	int ret;

	if (!plat->system_power_controller)
		return -EINVAL;

	for (;;) {
		ret = pmic_reg_write(dev, ACT8846_GLB_OFF_CTRL,
				     ACT8846_OFFSYSCLR);
		if (ret)
			return ret;

		ret = pmic_reg_write(dev, ACT8846_GLB_OFF_CTRL,
				     ACT8846_OFFSYSCLR | ACT8846_OFFSYS);
		if (ret)
			return ret;

		mdelay(10);
		printf("%s: powerdown failed!\n", __func__);
	}
}

static int act8846_bind(struct udevice *dev)
{
	const void *blob = gd->fdt_blob;
	int regulators_node;
	int children;

	regulators_node = fdt_subnode_offset(blob, dev->of_offset,
					     "regulators");
	if (regulators_node <= 0) {
		debug("%s: %s regulators subnode not found!", __func__,
		      dev->name);
		return -ENXIO;
	}

	debug("%s: '%s' - found regulators subnode\n", __func__, dev->name);

	children = pmic_bind_children(dev, regulators_node, pmic_children_info);
	if (!children)
		debug("%s: %s - no child found\n", __func__, dev->name);

	/* Always return success for this device */
	return 0;
}

static int act8846_ofdata_to_platdata(struct udevice *dev)
{
	struct act8846_platdata *plat = dev_get_platdata(dev);
	const void *blob = gd->fdt_blob;
	int node = dev->of_offset;

	plat->system_power_controller =
		fdtdec_get_bool(blob, node, "system-power-controller");

	return 0;
}

static struct dm_pmic_ops act8846_ops = {
	.reg_count = act8846_reg_count,
	.read = act8846_read,
	.write = act8846_write,
	.poweroff = act8846_poweroff,
};

static const struct udevice_id act8846_ids[] = {
	{ .compatible = "active-semi,act8846" },
	{ }
};

U_BOOT_DRIVER(pmic_act8846) = {
	.name = "act8846 pmic",
	.id = UCLASS_PMIC,
	.of_match = act8846_ids,
	.bind = act8846_bind,
	.ofdata_to_platdata = act8846_ofdata_to_platdata,
	.ops = &act8846_ops,
	.platdata_auto_alloc_size = sizeof(struct act8846_platdata),
};
