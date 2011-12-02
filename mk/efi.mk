include $(MAKEDIR)/syslinux.mk

com32 = $(topdir)/com32
core = $(topdir)/core

ARCH=ia32
LIBDIR=/usr/lib
FORMAT=efi-app-$(ARCH)

CFLAGS = -I/usr/include/efi -I/usr/include/efi/$(ARCH) \
		-DEFI_FUNCTION_WRAPPER -fPIC -fshort-wchar -ffreestanding \
		-Wall -I$(com32)/include -I$(core)/include -m32 -march=i686 \
		-I$(com32)/lib/ -std=gnu99

# gnuefi sometimes installs these under a gnuefi/ directory, and sometimes not
CRT0 := $(shell find $(LIBDIR) -name crt0-efi-$(ARCH).o 2>/dev/null | tail -n1)
LDSCRIPT := $(shell find $(LIBDIR) -name elf_$(ARCH)_efi.lds 2>/dev/null | tail -n1)

LDFLAGS = -T $(LDSCRIPT) -Bsymbolic -shared -nostdlib -znocombreloc \
		-L$(LIBDIR) $(CRT0)

SFLAGS     = $(GCCOPT) $(GCCWARN) -march=i386 \
	     -fomit-frame-pointer -D__COM32__ \
	     -nostdinc -iwithprefix include \
	     -I$(com32)/libutil/include -I$(com32)/include $(GPLINCLUDE)

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.efi: %.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
		-j .rela -j .reloc --target=$(FORMAT) $*.so $@
