SUBDIRS = libupload tools lib elflink/ldlinux gpllib libutil modules mboot \
	  menu samples elflink rosh cmenu hdt gfxboot sysdump lua/src chain

.PHONY: subdirs $(SUBDIRS)
subdirs: $(SUBDIRS)
$(SUBDIRS):
	@mkdir -p $(OBJ)/$@
	$(MAKE) -C $(OBJ)/$@ SRC="$(SRC)"/$@ OBJ="$(OBJ)"/$@/ \
		-f $(SRC)/$@/Makefile $(MAKECMDGOALS)

all tidy dist clean spotless install: subdirs

# Parallel dependencies
chain mboot menu: lib libutil gpllib
cmenu: lib libutil
elflink/ldlinux gpllib libupload libutil: lib
gfxboot: lib libutil gpllib
hdt: lib libupload cmenu gpllib libutil
lua/src: cmenu modules
modules: lib libutil gpllib
rosh: lib libutil gpllib
samples: libutil gpllib elflink/ldlinux
sysdump: lib libutil libupload gpllib
