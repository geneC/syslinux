SUBDIRS = libupload tools lib gpllib libutil modules mboot menu samples rosh cmenu \
	  hdt gfxboot sysdump lua/src

all tidy dist clean spotless install:
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
