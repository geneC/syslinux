SUBDIRS = lib libutil modules

all tidy clean spotless:
	for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
