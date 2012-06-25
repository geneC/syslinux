include $(MAKEDIR)/syslinux.mk

com32 = $(topdir)/com32
core = $(topdir)/core

# Support IA32 and x86_64 platforms with one build
# Set up architecture specifics; for cross compilation, set ARCH as apt
# gnuefi sets up architecture specifics in ia32 or x86_64 sub directories
# set up the LIBDIR and EFIINC for building for the appropriate architecture
# For now, the following assumptions are made:
# 1. gnu-efi lib for IA32 is installed in /usr/local/lib
# and the include files in /usr/local/include/efi.
# 2. gnu-efi lib for x86_64 is installed in /usr/lib
# and the include files in /usr/include/efi.
ifeq ($(ARCH),i386)
	SARCHOPT = -march=i386
	CARCHOPT = -m32 -march=i386
	EFI_SUBARCH = ia32
	LIBDIR = /usr/local/lib
	EFIINC = /usr/local/include/efi
endif
ifeq ($(ARCH),x86_64)
	SARCHOPT = -march=x86-64
	CARCHOPT = -m64 -march=x86-64
	EFI_SUBARCH = $(ARCH)
	EFIINC = /usr/include/efi
	LIBDIR=/usr/lib64
endif
#LIBDIR=/usr/lib
FORMAT=efi-app-$(EFI_SUBARCH)

CFLAGS = -I$(EFIINC) -I$(EFIINC)/$(EFI_SUBARCH) \
		-DEFI_FUNCTION_WRAPPER -fPIC -fshort-wchar -ffreestanding \
		-Wall -I$(com32)/include -I$(com32)/include/sys \
		-I$(core)/include $(CARCHOPT) \
		-I$(com32)/lib/ -std=gnu99 -DELF_DEBUG -DSYSLINUX_EFI

# gnuefi sometimes installs these under a gnuefi/ directory, and sometimes not
CRT0 := $(shell find $(LIBDIR) -name crt0-efi-$(EFI_SUBARCH).o 2>/dev/null | tail -n1)
LDSCRIPT := $(shell find $(LIBDIR) -name elf_$(EFI_SUBARCH)_efi.lds 2>/dev/null | tail -n1)

LDFLAGS = -T $(ARCH)/syslinux.ld -Bsymbolic -pie -nostdlib -znocombreloc \
		-L$(LIBDIR) --hash-style=gnu -m elf_$(ARCH) $(CRT0) -E

SFLAGS     = $(GCCOPT) $(GCCWARN) $(SARCHOPT) \
	     -fomit-frame-pointer -D__COM32__ \
	     -nostdinc -iwithprefix include \
	     -I$(com32)/libutil/include -I$(com32)/include -I$(com32)/include/sys $(GPLINCLUDE)

.PRECIOUS: %.o
%.o: %.S
	$(CC) $(SFLAGS) -c -o $@ $<

.PRECIOUS: %.o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

#%.efi: %.so
#	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel \
#		-j .rela -j .reloc --target=$(FORMAT) $*.so $@
