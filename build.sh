make -j 8 || exit 1
cp extlinux/extlinux /home/feng/test/syslinux/
cp com32/elflink/modules/*.c32 /home/feng/test/syslinux/cd/
sudo extlinux/extlinux -i /home/feng/test/syslinux/cd/
