/*
 * clock scaling for the MT53xx
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 *	Maintained by GUAN Xue-tao <gxt@mprc.pku.edu.cn>
 *	Copyright (C) 2001-2010 Guan Xuetao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/pm_opp.h>
#include <linux/of_device.h>

static struct mt53xx_dvfs_data {
	struct device *dev;
	struct cpufreq_frequency_table *freq_table;
	struct mutex lock;
	unsigned int transition_latency;
};

static struct mt53xx_dvfs_data *dvfs_info;

static inline unsigned int mt53xx_get_hw_cpupll(void) {
	struct cpufreq_frequency_table *freq_table = dvfs_info->freq_table;
	return freq_table[0].frequency;
}

static inline int mt53xx_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	return 0;
}

static unsigned int mt53xx_cpufreq_getspeed(unsigned int cpu)
{
	return mt53xx_get_hw_cpupll();
}

static int mt53xx_cpufreq_target_index(struct cpufreq_policy *policy,
                unsigned int index)
{
	struct cpufreq_frequency_table *freq_table = dvfs_info->freq_table;
	long target_freq;

	target_freq = freq_table[index].frequency;
	printk("mt53xx_cpufreq_target_index: %d : %ld\n", index, target_freq);

	mutex_lock(&dvfs_info->lock);
	policy->cur = target_freq;
	mutex_unlock(&dvfs_info->lock);

	return 0;
}

static int __init mt53xx_cpufreq_init(struct cpufreq_policy *policy)
{
	printk(KERN_ALERT"mt53xx_cpufreq_init\n");
	cpufreq_generic_init(policy, dvfs_info->freq_table,
            dvfs_info->transition_latency);
	return 0;
}

static int mt53xx_cpufreq_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static struct cpufreq_driver mt53xx_cpufreq_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= mt53xx_cpufreq_verify_speed,
	.target_index		= mt53xx_cpufreq_target_index,
	.get		= mt53xx_cpufreq_getspeed,
	.init		= mt53xx_cpufreq_init,
	.exit		= mt53xx_cpufreq_exit,
	.name		= "mediatek-cpufreq",
};


static const struct of_device_id mt53xx_cpufreq_match[] = {
	{
		.compatible = "mediatek,mt53xx-cpufreq",
	},
	{},
};
MODULE_DEVICE_TABLE(of, mt53xx_cpufreq_match);

static int mt53xx_cpufreq_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np;

	dvfs_info = devm_kzalloc(&pdev->dev, sizeof(*dvfs_info), GFP_KERNEL);
	if (!dvfs_info) {
		ret = -ENOMEM;
		goto err_put_node;
	}

	dvfs_info->dev = &pdev->dev;
	mutex_init(&dvfs_info->lock);

	// get clock transition
	np = of_cpu_device_node_get(0);
	if (!np) {
		dev_err(dvfs_info->dev, "No cpu node found");
		ret = -ENODEV;
		goto err_put_node;
	}

	if (of_property_read_u32(np, "clock-latency",
			&dvfs_info->transition_latency))
		dvfs_info->transition_latency = CPUFREQ_ETERNAL;

	of_node_put(np);

	ret = dev_pm_opp_of_add_table(dvfs_info->dev);
	if (ret) {
		dev_err(dvfs_info->dev, "failed to init OPP table: %d\n", ret);
		goto err_put_node;
	}

	ret = dev_pm_opp_init_cpufreq_table(dvfs_info->dev, &dvfs_info->freq_table);
	if (ret) {
		dev_err(dvfs_info->dev,
				"failed to init cpufreq table: %d\n", ret);
		goto err_put_node;
	}

	ret = cpufreq_register_driver(&mt53xx_cpufreq_driver);
	if (ret) {
		dev_err(dvfs_info->dev,
			"%s: failed to register cpufreq driver\n", __func__);
		goto err_free_table;
	}
	return 0;

err_free_table:
	dev_pm_opp_free_cpufreq_table(dvfs_info->dev, &dvfs_info->freq_table);
err_put_node:
	return ret;
}

static struct platform_driver mt53xx_cpufreq_platdrv = {
	.driver = {
		.name = "mt53xx-cpufreq",
		.owner = THIS_MODULE,
		.of_match_table = mt53xx_cpufreq_match,
	},
	.probe = mt53xx_cpufreq_probe,
};
//module_platform_driver(mt53xx_cpufreq_platdrv); // use this function, cpufreq is loading earlier than clk driver

static int __init mt53xx_cpufreq_driver_init(void)
{
	int err;
	printk("mt53xx_cpufreq_driver_init\n");

	err = platform_driver_register(&mt53xx_cpufreq_platdrv);
	if (err)
		return err;

	return 0;
}

late_initcall(mt53xx_cpufreq_driver_init);

