#
#   Copyright (C) 2013 Intel Corporation; author Matt Fleming
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Boston MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# Regression tests for the Linux kernel loader code paths
#

include $(objdir)/recipes.mk

ifeq ($(DERIVATIVE),PXELINUX)
derivative-tests = pxetest

pxetest_cfg = pxetest.cfg
pxetest_files = $(pxetest_cfg) kernelhello-vmlinuz
pxetest_results = pxetest.results

endif

all: tests

empty_cfg = empty.cfg
empty_files = $(empty_cfg) empty-vmlinuz
empty_results = empty.results

kernelhello_cfg = kernelhello.cfg
kernelhello_files = $(kernelhello_cfg) kernelhello-vmlinuz
kernelhello_results = kernelhello.results

cmdline_cfg = cmdline.cfg
cmdline_files = $(cmdline_cfg) kernelhello-vmlinuz
cmdline_results = cmdline.results

STANDARD_TESTS = kernelhello pxetest cmdline
$(STANDARD_TESTS):
	$(run-test)

empty:
	touch empty-vmlinuz
	$(run-test)

tests: banner empty kernelhello cmdline $(derivative-tests)

banner:
	printf "    Running Linux kernel regression tests...\n"
