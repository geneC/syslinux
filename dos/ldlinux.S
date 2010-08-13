/*
 * Wrap ldlinux.sys; this needs special handling for DOS.
 */

	.section ".payload","aw"
	.balign	16
	.globl	syslinux_ldlinux, syslinux_ldlinux_size
syslinux_ldlinux:
	.incbin "../core/ldlinux.sys"
	.space ((syslinux_ldlinux - .) & 511)
syslinux_ldlinux_size	= . - syslinux_ldlinux
	.size	syslinux_ldlinux, .-syslinux_ldlinux

	.section ".rodata","a"
	.balign	4
	.globl	syslinux_ldlinux_len
syslinux_ldlinux_len:
	.long	syslinux_ldlinux_size
	.size	syslinux_ldlinux_len, .-syslinux_ldlinux_len
