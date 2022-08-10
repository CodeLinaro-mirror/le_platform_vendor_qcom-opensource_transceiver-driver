LASSEN_ROOT=$(ROOTDIR)/transceiver-driver
CONFIG_LASSEN_LOADER=$(MODULE_LASSEN_LOADER)
KBUILD_OPTIONS := LASSEN_ROOT=$(LASSEN_ROOT) \
			CONFIG_LASSEN_LOADER=$(CONFIG_LASSEN_LOADER) \

obj-m += drivers/

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

%:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $@ $(KBUILD_OPTIONS)

clean:
	rm -f *.o *.ko *.mod.c *.mod.o *~ .*.cmd Module.symvers
	rm -rf .tmp_versions
