#
#   Copyright (C) 2013 Intel Corporation; author Matt Fleming
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Boston MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#

include $(objdir)/recipes.mk

all: tests

ifeq ($(DERIVATIVE),SYSLINUX)
chaindisk_cfg = chaindisk.cfg
chaindisk_files = $(chaindisk_cfg)
chaindisk_results = chaindisk.results
chaindisk:
	$(run-test)

tests: banner chaindisk
else

tests:
endif # ifeq ($(DERIVATIVE),SYSLINUX)

banner:
	printf "    Running COM32 module regression tests...\n"
