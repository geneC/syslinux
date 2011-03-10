SUBDIRS = mbr geodsp

all tidy dist clean spotless install:
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
