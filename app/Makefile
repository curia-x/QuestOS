# SPDX-License-Identifier: GPL-2.0
BUILD_DIR := $(PROJECT_DIR)/build
APP_OUT_DIR := $(BUILD_DIR)/app
APP_SRC_DIR := $(PROJECT_DIR)/app/src
APP_LD_SCRIPTS := $(APP_OUT_DIR)/linker.lds
APP_INCLUDE_DIR := $(PROJECT_DIR)/app/include
APP_CONFIG := $(APP_DIR)/config/app-config.toml

include scripts/Makefile.include

NOLIBC_DIR := $(THIRDPARTY_DIR)/nolibc
UAPI_DIR := $(THIRDPARTY_DIR)/linux-uapi-arm64

LIBRARY_DIR := library
DEMO_DIR := demo

CFLAGS := $(filter-out -fpie -ffreestanding, $(CFLAGS))
CFLAGS := $(filter-out -include linux/kconfig.h, $(CFLAGS))
LDFLAGS := $(filter-out -pie, $(LDFLAGS))

CFLAGS += -D__aarch64__

INCLUDE := \
	-I. \
	-I$(APP_INCLUDE_DIR) \
	-I$(UAPI_DIR)/include \
	-I$(NOLIBC_DIR)

$(APP_OUT_DIR)/$(PROJECT_NAME)_app_pack.elf:

$(APP_OUT_DIR):
	$(Q) mkdir -p $@

$(BUILD_DIR)/%.o: %.c
	$(Q) $(ECHO) "CC	$@"
	$(Q) mkdir -p $(dir $@)
	$(Q) $(CC) $(c_flags) $(INCLUDE) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	$(Q) $(ECHO) "AS	$@"
	$(Q) mkdir -p $(dir $@)
	$(Q) $(CC) $(a_flags) $(INCLUDE) -c -o $@ $<

$(APP_LD_SCRIPTS): $(APP_SRC_DIR)/linker.ld.S | $(APP_OUT_DIR)
	$(Q) $(ECHO) "LDS	$@"
	$(Q) $(CPP) $(INCLUDE) $(depflags) -P -Uarm64 -D__ASSEMBLY__ -DLINKER_SCRIPT -o $@ $<


# Common startup/runtime objects linked into every app
LIBRARY_SRCS := $(wildcard $(APP_SRC_DIR)/$(LIBRARY_DIR)/*.c)
LIBRARY_SRCS += $(wildcard $(APP_SRC_DIR)/$(LIBRARY_DIR)/*.S)

LIBRARY_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(LIBRARY_SRCS)))
LIBRARY_OBJS += $(patsubst %.S,$(BUILD_DIR)/%.o,$(filter %.S,$(LIBRARY_SRCS)))


# One app per directory under app/src/demo/
APP_LIST := $(notdir $(patsubst %/,%,$(wildcard $(APP_SRC_DIR)/$(DEMO_DIR)/*/)))

APP_ELF_LIST := $(addprefix $(APP_OUT_DIR)/,$(addsuffix .elf,$(APP_LIST)))


define APP_TEMPLATE

$(1)_SRCS := $$(wildcard $(APP_SRC_DIR)/$(DEMO_DIR)/$(1)/*.c)
$(1)_SRCS += $$(wildcard $(APP_SRC_DIR)/$(DEMO_DIR)/$(1)/*.S)

$(1)_OBJS := $$(patsubst %.c,$(BUILD_DIR)/%.o,$$(filter %.c,$$($(1)_SRCS)))
$(1)_OBJS += $$(patsubst %.S,$(BUILD_DIR)/%.o,$$(filter %.S,$$($(1)_SRCS)))

$(APP_OUT_DIR)/$(1).elf: $(LIBRARY_OBJS) $$($(1)_OBJS) $(APP_LD_SCRIPTS) | $(APP_OUT_DIR)
	$$(Q) $$(ECHO) "LD	$$@"
	$$(Q) $$(LD) $$(LDFLAGS) -o $$@ -T $(APP_LD_SCRIPTS) $(LIBRARY_OBJS) $$($(1)_OBJS)

endef

$(foreach app,$(APP_LIST),$(eval $(call APP_TEMPLATE,$(app))))

APP_OBJS := $(LIBRARY_OBJS)
APP_OBJS += $(foreach app,$(APP_LIST),$($(app)_OBJS))
APP_DEPS := $(patsubst %.o,%.d,$(APP_OBJS))
APP_DEPS += $(APP_LD_SCRIPTS:.lds=.d)

-include $(APP_DEPS)

$(APP_OUT_DIR)/$(PROJECT_NAME)_app_pack: $(APP_ELF_LIST) app/tools/proc_pack.py $(APP_CONFIG) | $(APP_OUT_DIR)
	$(Q) $(ECHO) "PACK	$@"
	$(Q) python3 app/tools/proc_pack.py -o $@ --config-file $(APP_CONFIG)


$(APP_OUT_DIR)/$(PROJECT_NAME)_app_pack.elf: $(APP_OUT_DIR)/$(PROJECT_NAME)_app_pack
	$(Q) $(ECHO) "OBJCOPY	$@"
	$(Q) $(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 \
		--rename-section .data=.process_blob,alloc,load,readonly,data,contents \
		$< $@
