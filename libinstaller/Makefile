# _bin.c files required by both BTARGET and ITARGET installers
BINFILES = bootsect_bin.c ldlinux_bin.c \
	   extlinux_bss_bin.c extlinux_sys_bin.c \
	   mbr_bin.c gptmbr_bin.c

PERL	 = perl

all: $(BINFILES)

bootsect_bin.c: ../core/ldlinux.bss bin2c.pl
	$(PERL) bin2c.pl syslinux_bootsect < $< > $@

ldlinux_bin.c: ../core/ldlinux.sys bin2c.pl
	$(PERL) bin2c.pl syslinux_ldlinux < $< > $@

extlinux_bss_bin.c: ../core/extlinux.bss bin2c.pl
	$(PERL) bin2c.pl extlinux_bootsect < $< > $@

extlinux_sys_bin.c: ../core/extlinux.sys bin2c.pl
	$(PERL) bin2c.pl extlinux_image 512 < $< > $@

mbr_bin.c: ../mbr/mbr.bin bin2c.pl
	$(PERL) bin2c.pl syslinux_mbr < $< > $@

gptmbr_bin.c: ../mbr/gptmbr.bin bin2c.pl
	$(PERL) bin2c.pl syslinux_gptmbr < $< > $@

tidy:
	rm -f $(BINFILES)

clean: tidy

dist: tidy

spotless: clean
