SUBDIRS = lib libutil modules samples

all tidy clean spotless install:
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
