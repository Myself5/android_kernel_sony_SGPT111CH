# 2012-07-20: File changed by Sony Corporation

#
# OTG infrastructure and transceiver drivers
#
GCOV_PROFILE_tegra-otg.o := y

ccflags-$(CONFIG_USB_DEBUG)		:= -DDEBUG
ccflags-$(CONFIG_USB_GADGET_DEBUG)	+= -DDEBUG

# infrastructure
obj-$(CONFIG_USB_OTG_UTILS)	+= otg.o

# transceiver drivers
obj-$(CONFIG_USB_GPIO_VBUS)	+= gpio_vbus.o
obj-$(CONFIG_USB_TEGRA_OTG)	+= tegra-otg.o
obj-$(CONFIG_ISP1301_OMAP)	+= isp1301_omap.o
obj-$(CONFIG_TWL4030_USB)	+= twl4030-usb.o
obj-$(CONFIG_TWL6030_USB)	+= twl6030-usb.o
obj-$(CONFIG_USB_LANGWELL_OTG)	+= langwell_otg.o
obj-$(CONFIG_NOP_USB_XCEIV)	+= nop-usb-xceiv.o
obj-$(CONFIG_USB_ULPI)		+= ulpi.o
obj-$(CONFIG_USB_ULPI_VIEWPORT)	+= ulpi_viewport.o
obj-$(CONFIG_USB_MSM_OTG)	+= msm_otg.o
obj-$(CONFIG_AB8500_USB)	+= ab8500-usb.o
obj-$(CONFIG_USB_CHARGE_NBX_EC)	+= nbx_usb_charge.o

ifeq ($(CONFIG_MACH_NBX03),y)
CFLAGS_tegra-otg.o += -DNO_TEGRA_OTG_ENABLE_IRQ_WAKE
endif
