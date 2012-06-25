/* Must be included first of all */
#if __SIZEOF_POINTER__ == 4
#ifdef __ASSEMBLY__
	.code16
#else
__asm__ (".code16gcc");
#endif
#endif
