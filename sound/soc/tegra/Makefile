# 2011-06-10: File changed by Sony Corporation
ccflags-y                       += -DNV_DEBUG=0
ifeq ($(CONFIG_AUDIO_WORKAROUND),y)
obj-$(CONFIG_TEGRA_JACK)        += tegra_wired_jack_nbx.o
obj-${CONFIG_SND_SOC_WM8903}    += tegra_soc_wm8903_nbx.o
obj-$(CONFIG_TEGRA_ALSA)        += tegra_soc_controls_nbx.o
else
obj-$(CONFIG_TEGRA_JACK)        += tegra_wired_jack.o
obj-${CONFIG_SND_SOC_WM8903}    += tegra_soc_wm8903.o
obj-$(CONFIG_TEGRA_ALSA)        += tegra_soc_controls.o
endif
obj-$(CONFIG_TEGRA_PCM)         += tegra_pcm.o
obj-$(CONFIG_TEGRA_IEC)         += tegra_spdif.o
obj-$(CONFIG_TEGRA_I2S)         += tegra_i2s.o
obj-$(CONFIG_TEGRA_GENERIC_CODEC)+= tegra_generic_codec.o
obj-${CONFIG_SND_SOC_WM8903}    += tegra_hookswitch.o
obj-${CONFIG_SND_SOC_WM8903}    += tegra_limit_volume.o
obj-${CONFIG_SND_SOC_WM8753}    += tegra_soc_wm8753.o
obj-${CONFIG_SND_SOC_WM8903_CHSW}    += tegra_ext_audio.o
