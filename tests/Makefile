#
#   Copyright (C) 2013 Intel Corporation; author Matt Fleming
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Boston MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#

#
# See linux/Makefile for an example Makefile. When tests are invoked the
# location of the Syslinux files, as used by qemu, is in $(MOUNTDIR).
# Tests that copy files into $(MOUNTDIR) must be sure to delete them once
# the test has completed, otherwise the qemu harddisk image will become
# full.
#

ifeq ($(INITIAL),)

INITIAL := 1

HDD = $(OBJ)/hdd.img

export QEMU QEMU_FLAGS INITIAL HDD

SYSLINUX_output-str := $(shell sudo $(SRC)/build-syslinux start \
				$(objdir)/bios $(HDD) test.cfg)
SYSLINUX_MOUNT_DIR = $(firstword $(SYSLINUX_output-str))
SYSLINUX_INSTALL_DIR = $(SYSLINUX_MOUNT_DIR)/boot/syslinux
SYSLINUX_CONFIG_FILE = $(SYSLINUX_INSTALL_DIR)/syslinux.cfg
SYSLINUX_LOOPDEV =$(lastword $(SYSLINUX_output-str))

PXELINUX_output-str := $(shell $(SRC)/build-pxelinux start $(topdir) test.cfg)
PXELINUX_INSTALL_DIR = $(firstword $(PXELINUX_output-str))/tftpboot
PXELINUX_CONFIG_FILE = $(PXELINUX_INSTALL_DIR)/pxelinux.cfg/default

QEMU = qemu-system-i386

all: syslinux-tests pxelinux-tests
syslinux-tests:
	$(MAKE) SRC="$(SRC)" OBJ="$(OBJ)" objdir="$(objdir)" \
		INITIAL="$(INITIAL)" INSTALL_DIR="$(SYSLINUX_INSTALL_DIR)" \
		CONFIG_FILE="$(SYSLINUX_CONFIG_FILE)" \
		DERIVATIVE=SYSLINUX
	@sudo $(SRC)/build-syslinux stop $(SYSLINUX_MOUNT_DIR) \
		$(SYSLINUX_LOOPDEV) test.cfg

pxelinux-tests:
	$(MAKE) SRC="$(SRC)" OBJ="$(OBJ)" objdir="$(objdir)" \
		INITIAL="$(INITIAL)" INSTALL_DIR="$(PXELINUX_INSTALL_DIR)" \
		CONFIG_FILE="$(PXELINUX_CONFIG_FILE)" \
		DERIVATIVE=PXELINUX
	$(SRC)/build-pxelinux stop $(PXELINUX_INSTALL_DIR) test.cfg
else # ifeq ($(INITIAL),)

export DERIVATIVE
ifeq ($(DERIVATIVE),SYSLINUX)
QEMU_FLAGS += -hda $(HDD) -no-reboot
else
QEMU_FLAGS = -boot n -net nic,model=virtio -net user,tftp="$(INSTALL_DIR)",bootfile=pxelinux.0 -no-reboot
endif

all: banner subdirs

banner:
	printf "Executing regression tests for $(DERIVATIVE)\n"

SUBDIRS = linux com32
.PHONY: subdirs $(SUBDIRS) fini
subdirs: $(SUBDIRS)
$(SUBDIRS):
	mkdir -p $(OBJ)/$@
	$(MAKE) -C $(OBJ)/$@ SRC="$(SRC)"/$@ OBJ="$(OBJ)"/$@/ \
		-f $(SRC)/$@/Makefile objdir="$(objdir)/tests" \
		INSTALL_DIR="$(INSTALL_DIR)" CONFIG_FILE="$(CONFIG_FILE)" \
		DERIVATIVE="$(DERIVATIVE)"

endif # ifeq ($(INITIAL),)
