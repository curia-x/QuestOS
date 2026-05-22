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
BUILD_DIR := $(PROJECT_DIR)/build
SRC_DIR := $(PROJECT_DIR)/src
LIB_DIR := $(SRC_DIR)/uboot_modyfied/lib
OUT_DIR ?= $(PROJECT_DIR)/build
LD_SCRIPTS := $(OUT_DIR)/linker.lds
export ABS_PROJECT_DIR PROJECT_DIR THIRDPARTY_DIR UBOOT_DIR LINUX_DIR BUILD_DIR SRC_DIR OUT_DIR LD_SCRIPTS

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
INCLUDE += -isystem $(shell $(CC) -print-file-name=include)
export INCLUDE

PHONY := all
# 默认目标
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

PHONY += app
app: $(OUT_DIR)/app/$(PROJECT_NAME)_app_pack.elf

PHONY += all
all: $(OUT_DIR)/$(PROJECT_NAME)_kernel.bin $(OUT_DIR)/$(PROJECT_NAME)_pack_all.bin

PHONY += clean
clean:
	$(Q) $(ECHO) "Deleting *.o *.a *.d *.lib *.bin *.elf *.lds $(PROJECT_NAME)_kernel  $(PROJECT_NAME)_app_pack"
	$(Q) find $(OUT_DIR) -type f \( -name '*.o' -o \
									-name '*.a' -o \
									-name '*.d' -o \
									-name '*.lib' -o \
									-name '*.bin' -o \
									-name '*.elf' -o \
									-name '*.lds' -o \
									-name '$(PROJECT_NAME)_kernel' -o \
									-name '$(PROJECT_NAME)_app_pack' \
									\) \
						-delete  2>/dev/null || true

.PHONY: $(PHONY)
