SUBDIRS = tools lib gpllib libutil modules mboot menu samples elflink elflink/modules rosh cmenu \
	  hdt gfxboot sysdump lua/src
all tidy dist clean spotless install:
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
