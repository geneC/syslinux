SUBDIRS = lib libutil modules samples

all tidy clean spotless:
	for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
