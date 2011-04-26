# -*- makefile -*-

include $(MAKEDIR)/syslinux.mk

GCCOPT := $(call gcc_ok,-std=gnu99,)
GCCOPT += $(call gcc_ok,-m32,)
GCCOPT += $(call gcc_ok,-fno-stack-protector,)
GCCOPT += $(call gcc_ok,-fwrapv,)
GCCOPT += $(call gcc_ok,-freg-struct-return,)
GCCOPT += $(call gcc_ok,-fPIE,-fPIC)
GCCOPT += $(call gcc_ok,-fno-exceptions,)
GCCOPT += $(call gcc_ok,-fno-asynchronous-unwind-tables,)
GCCOPT += $(call gcc_ok,-fno-strict-aliasing,)
GCCOPT += $(call gcc_ok,-falign-functions=0,-malign-functions=0)
GCCOPT += $(call gcc_ok,-falign-jumps=0,-malign-jumps=0)
GCCOPT += $(call gcc_ok,-falign-labels=0,-malign-labels=0)
GCCOPT += $(call gcc_ok,-falign-loops=0,-malign-loops=0)
GCCOPT += $(call gcc_ok,-mpreferred-stack-boundary=2,)

INCLUDE	= -I.
STRIP	= strip --strip-all -R .comment -R .note

# zlib and libpng configuration flags
LIBFLAGS = -DDYNAMIC_CRC_TABLE -DPNG_NO_CONSOLE_IO \
	   -DPNG_NO_WRITE_SUPPORTED \
	   -DPNG_NO_MNG_FEATURES \
	   -DPNG_NO_READ_tIME -DPNG_NO_WRITE_tIME

# We need some features in libpng which apparently aren't available in the
# fixed-point versions.  It's OK, because we have to have a non-graphical
# fallback anyway, just use that on old machines...
# LIBFLAGS += -DPNG_NO_FLOATING_POINT_SUPPORTED

REQFLAGS  = $(GCCOPT) -g -mregparm=3 -DREGPARM=3 -D__COM32__ \
	    -nostdinc -iwithprefix include -I. -I./sys -I../include
OPTFLAGS  = -Os -march=i386 -falign-functions=0 -falign-jumps=0 \
	    -falign-labels=0 -ffast-math -fomit-frame-pointer
WARNFLAGS = $(GCCWARN) -Wpointer-arith -Wwrite-strings -Wstrict-prototypes -Winline

CFLAGS  = $(OPTFLAGS) $(REQFLAGS) $(WARNFLAGS) $(LIBFLAGS)
LDFLAGS	= -m elf32_i386

.SUFFIXES: .c .o .a .so .lo .i .S .s .ls .ss .lss

% : %.c # Cancel default rule

% : %.S

.c.o:
	$(CC) $(MAKEDEPS) $(CFLAGS) -c -o $@ $<

.c.i:
	$(CC) $(MAKEDEPS) $(CFLAGS) -E -o $@ $<

.c.s:
	$(CC) $(MAKEDEPS) $(CFLAGS) -S -o $@ $<

.S.o:
	$(CC) $(MAKEDEPS) $(CFLAGS) -D__ASSEMBLY__ -c -o $@ $<

.S.s:
	$(CC) $(MAKEDEPS) $(CFLAGS) -D__ASSEMBLY__ -E -o $@ $<

.S.lo:
	$(CC) $(MAKEDEPS) $(CFLAGS) $(SOFLAGS) -D__ASSEMBLY__ -c -o $@ $<

.S.ls:
	$(CC) $(MAKEDEPS) $(CFLAGS) $(SOFLAGS) -D__ASSEMBLY__ -E -o $@ $<

.s.o:
	$(CC) $(MAKEDEPS) $(CFLAGS) -x assembler -c -o $@ $<

.ls.lo:
	$(CC) $(MAKEDEPS) $(CFLAGS) $(SOFLAGS) -x assembler -c -o $@ $<

.c.lo:
	$(CC) $(MAKEDEPS) $(CFLAGS) $(SOFLAGS) -c -o $@ $<

.c.ls:
	$(CC) $(MAKEDEPS) $(CFLAGS) $(SOFLAGS) -S -o $@ $<
