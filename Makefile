# SPDX-License-Identifier: GPL-2.0
Q = @
export Q

PROJECT_NAME := quest_os
export PROJECT_NAME

CROSS_COMPILE ?= aarch64-linux-gnu-
export CROSS_COMPILE

makefile_this := $(lastword $(MAKEFILE_LIST))

include scripts/Makefile.include

ABS_PROJECT_DIR := $(realpath $(dir $(makefile_this)))
PROJECT_DIR := .
THIRDPARTY_DIR := $(PROJECT_DIR)/thirdparty
UBOOT_DIR := $(THIRDPARTY_DIR)/u-boot
LINUX_DIR := $(THIRDPARTY_DIR)/linux
TF_A_DIR := $(THIRDPARTY_DIR)/tf-a
BUILD_DIR := $(PROJECT_DIR)/build
SRC_DIR := $(PROJECT_DIR)/src
LIB_DIR := $(SRC_DIR)/uboot_modyfied/lib
OUT_DIR ?= $(PROJECT_DIR)/build
LD_SCRIPTS := $(OUT_DIR)/linker.lds
export ABS_PROJECT_DIR PROJECT_DIR THIRDPARTY_DIR UBOOT_DIR LINUX_DIR TF_A_DIR BUILD_DIR SRC_DIR OUT_DIR LD_SCRIPTS

APP_DIR := app
export APP_DIR

srcroot := $(PROJECT_DIR)/src
export srcroot

CFLAGS :=
AFLAGS :=
LDFLAGS :=
INCLUDE :=
export CFLAGS AFLAGS LDFLAGS INCLUDE

ECHO	= echo
MAKE	= make --no-print-directory -s
CPP		= $(CC) -E
CC		= $(CROSS_COMPILE)gcc
LD		= $(CROSS_COMPILE)ld
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
READELF		= $(CROSS_COMPILE)readelf
STRIP		= $(CROSS_COMPILE)strip
export ECHO MAKE CC LD AR NM OBJCOPY OBJDUMP READELF STRIP

include scripts/Makefile.flags

INCLUDE +=		-I. \
				-I$(PROJECT_DIR)/include \
				-I$(PROJECT_DIR)/include/linux_porting\
				-I$(PROJECT_DIR)/include/linux_modify \
				-I$(PROJECT_DIR)/include/uboot_porting \
				-I$(PROJECT_DIR)/include/uboot_modify \
				-I$(LINUX_DIR)/include \
				-I$(LINUX_DIR)/include/uapi \
				-I$(LINUX_DIR)/arch/arm64/include \
				-I$(LINUX_DIR)/arch/arm64/include/generated \
				-I$(LINUX_DIR)/arch/arm64/include/uapi \
				-I$(LINUX_DIR)/arch/arm64/include/generated/uapi \
				-I$(UBOOT_DIR)/include \
				-I$(UBOOT_DIR)/arch/arm/include \

CFLAGS += -include linux/kconfig.h

CONFIG_PSCI_METHOD ?= smc
ifneq ($(filter $(CONFIG_PSCI_METHOD),smc hvc),$(CONFIG_PSCI_METHOD))
$(error CONFIG_PSCI_METHOD must be smc or hvc)
endif
CFLAGS += -DCONFIG_PSCI_METHOD=\"$(CONFIG_PSCI_METHOD)\"

INCLUDE += -isystem $(shell $(CC) -print-file-name=include)
export INCLUDE

DIRECT_KERNEL_OUT_DIR ?= $(OUT_DIR)/direct_kernel

# For ATF build.
ATF_PLAT ?= qemu
ATF_DEBUG ?= 1
ATF_EXTRA_ARGS ?=
ATF_BUILD_TYPE := $(if $(filter 1,$(ATF_DEBUG)),debug,release)
ATF_BUILD_DIR := $(TF_A_DIR)/build/$(ATF_PLAT)/$(ATF_BUILD_TYPE)
ATF_QEMU_FW_BIN := $(ATF_BUILD_DIR)/qemu_fw.bios
ATF_IMAGES := bl1.bin bl2.bin bl31.bin fip.bin
ATF_TARGETS := $(addprefix $(OUT_DIR)/,$(ATF_IMAGES))
QEMU_FW_BIN := $(OUT_DIR)/qemu_fw.bios
ATF_CLEAN_ENV := CFLAGS AFLAGS LDFLAGS INCLUDE
ATF_CLEAN_ENV += CC CPP LD AR NM OBJCOPY OBJDUMP READELF STRIP
ATF_ENV := env $(addprefix -u ,$(ATF_CLEAN_ENV))
ATF_GIC_DRIVER ?= QEMU_GICV3
ATF_SECURE_BOOT ?= 1

ifeq ($(strip $(ATF_SECURE_BOOT)),1)
ATF_EXTRA_ARGS += TRUSTED_BOARD_BOOT=1
ATF_EXTRA_ARGS += GENERATE_COT=1
ATF_EXTRA_ARGS += ARM_ROTPK_LOCATION=devel_rsa
ATF_EXTRA_ARGS += ROT_KEY=plat/arm/board/common/rotpk/arm_rotprivk_rsa.pem
endif

PHONY := all
# Default target.
all:

-include $(LD_SCRIPTS:.lds=.d)

PHONY += FORCE
FORCE:

PHONY += $(OUT_DIR)/src/built-in.a
$(OUT_DIR)/src/built-in.a:
	$(Q) $(MAKE) -f scripts/Makefile.build obj=src

PHONY += $(OUT_DIR)/lib/built-in.a
$(OUT_DIR)/lib/built-in.a:
	$(Q) $(MAKE) -f scripts/Makefile.build lib=lib

$(LD_SCRIPTS): $(SRC_DIR)/linker.lds.S
	$(Q) $(ECHO) "LDS     $@"
	$(Q) mkdir -p $(dir $@)
	$(Q) $(CPP) $(INCLUDE) $(c_flags) -P -Uarm64 -D__ASSEMBLY__ -DLINKER_SCRIPT -o $@ $<

$(OUT_DIR)/$(PROJECT_NAME)_kernel: $(OUT_DIR)/src/built-in.a $(OUT_DIR)/lib/built-in.a $(LD_SCRIPTS)
	$(Q) $(ECHO) "LD	$@"
	$(Q) mkdir -p $(dir $@)
	$(Q) $(LD) $(LDFLAGS) -o $@ -T $(LD_SCRIPTS) --whole-archive $(OUT_DIR)/src/built-in.a $(OUT_DIR)/lib/built-in.a

PHONY += $(OUT_DIR)/app/$(PROJECT_NAME)_app_pack.elf
$(OUT_DIR)/app/$(PROJECT_NAME)_app_pack.elf: FORCE
	$(Q) $(ECHO) "MAKE	APPS"
	$(Q) $(MAKE) -f $(APP_DIR)/Makefile

$(OUT_DIR)/kernel_app_pack.elf: $(OUT_DIR)/$(PROJECT_NAME)_kernel
$(OUT_DIR)/kernel_app_pack.elf: $(OUT_DIR)/app/$(PROJECT_NAME)_app_pack.elf
$(OUT_DIR)/kernel_app_pack.elf: $(LD_SCRIPTS)
	$(Q) $(LD) $(LDFLAGS) -o $@ -T $(LD_SCRIPTS) \
		--whole-archive $(OUT_DIR)/src/built-in.a \
		$(OUT_DIR)/lib/built-in.a \
		$(OUT_DIR)/app/$(PROJECT_NAME)_app_pack.elf \

$(OUT_DIR)/$(PROJECT_NAME)_pack_all.bin: $(OUT_DIR)/kernel_app_pack.elf
	$(Q) $(ECHO) "OBJCOPY	$@"
	$(Q) $(OBJCOPY) $< -O binary $@

$(OUT_DIR)/$(PROJECT_NAME)_kernel.bin: $(OUT_DIR)/$(PROJECT_NAME)_kernel
	$(Q) $(ECHO) "OBJCOPY	$@"
	$(Q) $(OBJCOPY) $< -O binary $@

PHONY += kernel
kernel: $(OUT_DIR)/$(PROJECT_NAME)_kernel.bin

PHONY += direct_kernel
direct_kernel:
	$(Q) $(MAKE) OUT_DIR=$(DIRECT_KERNEL_OUT_DIR) CONFIG_PSCI_METHOD=hvc kernel

PHONY += app
app: $(OUT_DIR)/app/$(PROJECT_NAME)_app_pack.elf

$(ATF_QEMU_FW_BIN): $(OUT_DIR)/$(PROJECT_NAME)_pack_all.bin FORCE
	$(Q) $(ECHO) "ATF	$@"
	$(Q) test -d $(TF_A_DIR)/.git || git submodule update --init --recursive thirdparty/tf-a
	$(Q) test -d $(TF_A_DIR)/contrib/mbed-tls/.git || \
		(cd $(TF_A_DIR) && git submodule update --init --recursive --depth=1 contrib/mbed-tls)
	$(Q) $(ATF_ENV) $(MAKE) -C $(TF_A_DIR) PLAT=$(ATF_PLAT) DEBUG=$(ATF_DEBUG) \
		CROSS_COMPILE=$(CROSS_COMPILE) BL33=$(abspath $<) \
		QEMU_USE_GIC_DRIVER=$(ATF_GIC_DRIVER) \
		$(ATF_EXTRA_ARGS) all fip

$(ATF_TARGETS): $(OUT_DIR)/%.bin: $(ATF_QEMU_FW_BIN)
	$(Q) mkdir -p $(dir $@)
	$(Q) cp $(ATF_BUILD_DIR)/$*.bin $@

$(QEMU_FW_BIN): $(ATF_QEMU_FW_BIN)
	$(Q) mkdir -p $(dir $@)
	$(Q) cp $(ATF_QEMU_FW_BIN) $@

PHONY += atf
atf: $(ATF_TARGETS) $(QEMU_FW_BIN)

PHONY += qemu_fw
qemu_fw: $(QEMU_FW_BIN)

all: $(OUT_DIR)/$(PROJECT_NAME)_kernel.bin $(ATF_TARGETS) $(QEMU_FW_BIN)
	$(Q) $(ECHO) "\nOutput files: $(OUT_DIR)/$(PROJECT_NAME)_kernel.bin $(QEMU_FW_BIN)"
	$(Q) $(ECHO) "==================== Build complete. ====================\n"

PHONY += clean
clean:
	$(Q) $(ECHO) "Cleaning *.o *.a *.d *.lib *.bin *.bios *.elf *.lds"
	$(Q) [ ! -d $(OUT_DIR) ] || find $(OUT_DIR) -type f \( -name '*.o' -o \
									-name '*.a' -o \
									-name '*.d' -o \
									-name '*.lib' -o \
									-name '*.bin' -o \
									-name '*.bios' -o \
									-name '*.elf' -o \
									-name '*.lds' \
									\) -delete

	$(Q) $(ECHO) "Cleaning $(PROJECT_NAME)_kernel $(PROJECT_NAME)_app_pack"
	$(Q) [ ! -d $(OUT_DIR) ] || find $(OUT_DIR) -type f \( -name '$(PROJECT_NAME)_kernel' -o \
									-name '$(PROJECT_NAME)_app_pack' \
									\) -delete

	$(Q) $(ECHO) "Cleaning generated header files"
	$(Q) rm -rf $(PROJECT_DIR)/include/asm/generated/

	$(Q) $(ECHO) "Cleaning ATF build outputs"
	$(Q) rm -rf $(ATF_BUILD_DIR)

.PHONY: $(PHONY)
