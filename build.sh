make -j 4 || exit 1
cp extlinux/extlinux /home/feng/test/syslinux/
cp com32/elflink/modules/*.c32 /home/feng/test/syslinux/cd/
cp memdisk/memdisk /home/feng/test/syslinux/cd/
sudo extlinux/extlinux -i /home/feng/test/syslinux/cd/
qemu -hda /home/feng/test/syslinux/test.img
