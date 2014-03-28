# 2012-07-20: File changed by Sony Corporation

GCOV_PROFILE := y

ifeq ($(CONFIG_MACH_TXS03),y)
	CFLAGS_hdmi.o += -DENABLE_DETECT_SETTING
endif

obj-y += dc.o
obj-y += rgb.o
obj-y += hdmi.o
obj-$(CONFIG_TEGRA_NVHDCP) += nvhdcp.o
obj-y += edid.o
obj-y += nvsd.o
obj-y += dsi.o
obj-y += dc_sysfs.o
obj-$(CONFIG_TEGRA_OVERLAY) += overlay.o
obj-$(CONFIG_TEGRA_DC_EXTENSIONS) += ext/
