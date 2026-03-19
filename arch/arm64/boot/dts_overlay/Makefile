dtbo-$(CONFIG_ARCH_MT5891) += mt5891_smp_overlay.dtbo
dtbo-$(CONFIG_ARCH_MT5886) += mt5886_smp_overlay.dtbo
dtbo-$(CONFIG_ARCH_MT5863) += mt5863_smp_overlay.dtbo
dtbo-$(CONFIG_ARCH_MT5893) += mt5893_smp_overlay.dtbo

DTBO_LIST := $(dtbo-y)

DTBO_OBJS := $(addprefix $(obj)/dts_overlay/,$(DTBO_LIST))

dtbos: $(addprefix $(obj)/, $(DTBO_LIST))

clean-files := dts_overlay/*.dtbo *.dtbo

