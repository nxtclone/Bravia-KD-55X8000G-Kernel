dtb-$(CONFIG_ARCH_MT5891) += mt5891_smp.dtb
dtb-$(CONFIG_ARCH_MT5886) += mt5886_smp.dtb
dtb-$(CONFIG_ARCH_MT5863) += mt5863_smp.dtb
ifeq "$(AB_UPDATE_ENABLE)" "true"
dtb-$(CONFIG_ARCH_MT5893) += mt5893_smp_avb_abupdate.dtb
else
ifeq ($(CONFIG_DM_VERITY_FEC),y)
dtb-$(CONFIG_ARCH_MT5893) += mt5893_smp_dmverity.dtb
else
dtb-$(CONFIG_ARCH_MT5893) += mt5893_smp.dtb
endif
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
