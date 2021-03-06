/* 2012-07-20: File changed by Sony Corporation */
/*
 * tegra_asoc_utils.c - Harmony machine ASoC driver
 *
 * Author: Stephen Warren <swarren@nvidia.com>
 * Copyright (C) 2010 - NVIDIA, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>

#include "tegra_asoc_utils.h"

int tegra_asoc_utils_set_rate(struct tegra_asoc_utils_data *data, int srate,
			      int mclk)
{
	int new_baseclock;
	bool clk_change;
	int err;

	switch (srate) {
	case 11025:
	case 22050:
	case 44100:
	case 88200:
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		new_baseclock = 56448000;
#else
		new_baseclock = 564480000;
#endif
		break;
	case 8000:
	case 16000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
		new_baseclock = 73728000;
#else
		new_baseclock = 552960000;
#endif
		break;
	default:
		return -EINVAL;
	}

	clk_change = ((new_baseclock != data->set_baseclock) ||
			(mclk != data->set_mclk));
	if (!clk_change)
		return 0;

	/* Don't change rate if already one dai-link is using it */
	if (data->lock_count)
		return -EINVAL;

	data->set_baseclock = 0;
	data->set_mclk = 0;

	clk_disable(data->clk_cdev1);
	clk_disable(data->clk_pll_a_out0);
	clk_disable(data->clk_pll_a);

	err = clk_set_rate(data->clk_pll_a, new_baseclock);
	if (err) {
		dev_err(data->dev, "Can't set pll_a rate: %d\n", err);
		return err;
	}

	err = clk_set_rate(data->clk_pll_a_out0, mclk);
	if (err) {
		dev_err(data->dev, "Can't set pll_a_out0 rate: %d\n", err);
		return err;
	}

	/* Don't set cdev1/extern1 rate; it's locked to pll_a_out0 */

	err = clk_enable(data->clk_pll_a);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a: %d\n", err);
		return err;
	}

	err = clk_enable(data->clk_pll_a_out0);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a_out0: %d\n", err);
		return err;
	}

	err = clk_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}

	data->set_baseclock = new_baseclock;
	data->set_mclk = mclk;

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_set_rate);

void tegra_asoc_utils_lock_clk_rate(struct tegra_asoc_utils_data *data,
				    int lock)
{
	if (lock)
		data->lock_count++;
	else if (data->lock_count)
		data->lock_count--;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_lock_clk_rate);

int tegra_asoc_utils_clk_enable(struct tegra_asoc_utils_data *data)
{
	int err;

	err = clk_enable(data->clk_pll_a);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a: %d\n", err);
		return err;
	}

	err = clk_enable(data->clk_pll_a_out0);
	if (err) {
		dev_err(data->dev, "Can't enable pll_a_out0: %d\n", err);
		return err;
	}

	err = clk_enable(data->clk_cdev1);
	if (err) {
		dev_err(data->dev, "Can't enable cdev1: %d\n", err);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_enable);

int tegra_asoc_utils_clk_disable(struct tegra_asoc_utils_data *data)
{
	clk_disable(data->clk_cdev1);
	clk_disable(data->clk_pll_a_out0);
	clk_disable(data->clk_pll_a);
	return 0;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_clk_disable);

int tegra_asoc_utils_init(struct tegra_asoc_utils_data *data,
			  struct device *dev)
{
	int ret;

	data->dev = dev;

	data->clk_pll_a = clk_get_sys(NULL, "pll_a");
	if (IS_ERR(data->clk_pll_a)) {
		dev_err(data->dev, "Can't retrieve clk pll_a\n");
		ret = PTR_ERR(data->clk_pll_a);
		goto err;
	}

	data->clk_pll_a_out0 = clk_get_sys(NULL, "pll_a_out0");
	if (IS_ERR(data->clk_pll_a_out0)) {
		dev_err(data->dev, "Can't retrieve clk pll_a_out0\n");
		ret = PTR_ERR(data->clk_pll_a_out0);
		goto err_put_pll_a;
	}

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	data->clk_cdev1 = clk_get_sys(NULL, "cdev1");
#else
	data->clk_cdev1 = clk_get_sys("extern1", NULL);
#endif
	if (IS_ERR(data->clk_cdev1)) {
		dev_err(data->dev, "Can't retrieve clk cdev1\n");
		ret = PTR_ERR(data->clk_cdev1);
		goto err_put_pll_a_out0;
	}

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
	data->clk_out1 = ERR_PTR(-ENOENT);
#else
	data->clk_out1 = clk_get_sys("clk_out_1", "extern1");
	if (IS_ERR(data->clk_out1)) {
		dev_err(data->dev, "Can't retrieve clk out1\n");
		ret = PTR_ERR(data->clk_out1);
		goto err_put_cdev1;
	}
#endif

	ret = clk_enable(data->clk_pll_a);
	if (ret) {
		dev_err(data->dev, "Can't enable clk pll_a");
		goto err_put_out1;
	}

	ret = clk_enable(data->clk_pll_a_out0);
	if (ret) {
		dev_err(data->dev, "Can't enable clk pll_a_out0");
		goto err_put_out1;
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
	ret = clk_set_parent(data->clk_cdev1, data->clk_pll_a_out0);
	if (ret) {
		dev_err(data->dev, "Can't set clk cdev1/extern1 parent");
		goto err_put_out1;
	}
#endif

	ret = clk_enable(data->clk_cdev1);
	if (ret) {
		dev_err(data->dev, "Can't enable clk cdev1/extern1");
		goto err_put_out1;
	}

	if (!IS_ERR(data->clk_out1)) {
		ret = clk_enable(data->clk_out1);
		if (ret) {
			dev_err(data->dev, "Can't enable clk out1");
			goto err_put_out1;
		}
	}

	ret = tegra_asoc_utils_set_rate(data, 48000, 256 * 48000);
	if (ret)
		goto err_put_out1;

	return 0;

err_put_out1:
	if (!IS_ERR(data->clk_out1))
		clk_put(data->clk_out1);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
err_put_cdev1:
#endif
	clk_put(data->clk_cdev1);
err_put_pll_a_out0:
	clk_put(data->clk_pll_a_out0);
err_put_pll_a:
	clk_put(data->clk_pll_a);
err:
	return ret;
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_init);

void tegra_asoc_utils_fini(struct tegra_asoc_utils_data *data)
{
	if (!IS_ERR(data->clk_out1))
		clk_put(data->clk_out1);
	clk_put(data->clk_cdev1);
	clk_put(data->clk_pll_a_out0);
	clk_put(data->clk_pll_a);
}
EXPORT_SYMBOL_GPL(tegra_asoc_utils_fini);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_DESCRIPTION("Tegra ASoC utility code");
MODULE_LICENSE("GPL");
