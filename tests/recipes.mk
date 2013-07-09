#
#   Copyright (C) 2013 Intel Corporation; author Matt Fleming
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
#   Boston MA 02111-1307, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# Canned recipes

#
# copy-files - copy a config to the mounted filesystem
# 
# Copies $(cfg) to the default Syslinux config location.
# Usually this wants pairing with a use of $(remove-config).
# 
# To use, your config filename must be the same as your target
#
define copy-files =
    for f in $($@_files); do \
	sudo cp $$f $(INSTALL_DIR) ;\
    done
    sync
endef

#
# install-config
#
define install-config =
    sudo sh -c 'echo INCLUDE $($@_cfg) >> $(CONFIG_FILE)'
    sync
endef

#
# remove-files - deletes files from the mounted filesystem
#
# Deletes $(mytest_files)
#
define remove-files =
    for f in $($@_files); do \
	sudo rm $(INSTALL_DIR)/$$f ;\
    done
endef

#
# delete-config - remove a test's config file from the master config
#
define delete-config =
    sudo sed -i -e '/INCLUDE $($@_cfg)/d' $(CONFIG_FILE)
endef

#
# run-test - begin executing the tests
#
define run-test =
    $(copy-files)
    $(install-config)

    sudo $(QEMU) $(QEMU_FLAGS) -serial file:$@.log

    $(delete-config)
    $(remove-files)

    sudo sort $@.log -o $@.log
    if [ `comm -1 -3 $@.log $($@_results) | wc -l` -ne 0 ]; then \
        printf "      [!] $@ failed\n" ;\
    else \
        printf "      [+] $@ passed\n" ;\
    fi
endef
