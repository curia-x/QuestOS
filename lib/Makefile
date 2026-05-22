# SPDX-License-Identifier: GPL-2.0
lib_makefile_this := $(lastword $(MAKEFILE_LIST))
lib-clean-path = $(patsubst ./%,%,$(subst /./,/,$1))
this-dir := $(call lib-clean-path,$(subst $(ABS_PROJECT_DIR),.,$(realpath $(dir $(lib_makefile_this)))))

# Add libraries here.
lib-y += libfdt

# The final paths of all generated library.
lib_outputs := $(addprefix $(this-dir)/, $(lib-y))
# All generated libraries must be named $(libname).lib
lib_outputs := $(patsubst %, %.lib, $(lib_outputs))

lib_makefile = $(strip $(dir $1))$(patsubst %.lib,Makefile.%, $(notdir $1))
# Get the paths of the library Makefiles.
sublib_makes := $(foreach m, $(lib_outputs), $(call lib_makefile, $m))

lib_outputs := $(call lib-clean-path,$(addprefix $(OUT_DIR)/,$(lib_outputs)))

# Use the custom Makefile to build the specify library.
# The naming format of the Makefile must be Makefile.$(LIB_NAME).
# The sub Makefile.$(LIB_NAME) must be responsible for generating
# corresponding $(LIB_NAME).lib which used to generate lib/built-in.a.
PHONY += $(sublib_makes)
$(sublib_makes):
	$(Q) mkdir -p $(OUT_DIR)/$(subst Makefile.,,$@)/
	$(Q) $(MAKE) -f $@
