STACK_SIZE	= 8192
HEAP_SIZE	= 16384

		.section ".header","a"
		.balign	512
__header_start:
		.short	0x5a4d
		.short	_exe_edata_low
		.short	_exe_edata_blocks
		.short	0		/* Relocation count */
		.short	(__header_end - __header_start) >> 4
		.short	_exe_bss_paras
		.short	_exe_bss_paras
		.short	_exe_text_seg	/* SP */
		.short	__stack_end
		.short	0		/* Checksum */
		.short	_start
		.short	_exe_text_seg	/* CS */
		.short	__reloc
		.short	0		/* Overlay number */
		.short	0		/* Unknown/pad? */
		.short	0		/* Unknown/pad? */
__reloc:
		.balign	512
__header_end:

		.section ".heap","aw"
		.space	HEAP_SIZE

		.section ".stack","aw"
		.space	STACK_SIZE
