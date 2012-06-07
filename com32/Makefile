SUBDIRS = libupload tools lib elflink/ldlinux gpllib libutil modules mboot \
	  menu samples elflink rosh cmenu hdt gfxboot sysdump lua/src chain

all tidy dist clean spotless install:
	set -e; for d in $(SUBDIRS); do $(MAKE) -C $$d $@; done
