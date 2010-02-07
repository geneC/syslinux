SUBDIRS = lib gpllib libutil modules mboot menu samples rosh cmenu \
	  hdt gfxboot sysdump
all tidy dist clean spotless install:
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
