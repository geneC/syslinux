	.section ".dynlink","a"
_dynamic_symbols:
	.globl _dynamic_symbols
	.incbin SYMFILE
	.size _dynamic_symbols, .-_dynamic_symbols
