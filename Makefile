NASM	= nasm

all:	bootsect.bin ldlinux.sys

ldlinux.bin: ldlinux.asm
	$(NASM) -f bin -dHEX_TIME="`perl now.pl`" -l ldlinux.lst -o ldlinux.bin ldlinux.asm

bootsect.bin: ldlinux.bin
	dd if=ldlinux.bin of=bootsect.bin bs=512 count=1

ldlinux.sys:
	dd if=ldlinux.bin of=ldlinux.sys  bs=512 skip=1

clean:
	rm -f *.bin *.lst *.sys

distclean: clean
	rm -f *~ \#*
