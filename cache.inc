; -*- fundamental -*- ---------------------------------------------------
;
;   Copyright 2004-2007 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

		section .text

		struc cptr
.sector:	resd 1		; Sector number
.prev:		resw 1		; LRU pointer to previous (less recent)
.next:		resw 1		; LRU pointer to next (more recent)
		endstruc
cptr_size_lg2	equ 3

NCacheEntries	equ 65536/SECTOR_SIZE

;
; initcache: Initialize the cache data structures
;
initcache:
		xor eax,eax			; We don't care about sector 0
		mov di,CachePtrs
		mov cx,NCacheEntries+1
		mov bx,CachePtrs+NCacheEntries*cptr_size	; "prev" pointer
.loop:
		mov [di+cptr.sector],eax	; Zero sector number
		mov [di+cptr.prev],bx		; Previous pointer
		mov [bx+cptr.next],di		; Previous entry's next pointer
		mov bx,di
		add di,cptr_size
		loop .loop
		ret

;
; cache_remove_lru:
;	Remove a cache block from the expiry list.  Can be used to
;	lock down a cache block for future use.
;
;	SI = cache block offset (any offset inside the block is OK)
;	Assumes CS == DS
;
cache_remove_lru:
		pusha

		; Get metadata pointer
		shr si,SECTOR_SHIFT-cptr_size_lg2
		add si,CachePtrs+cptr_size

		mov bx,[si+cptr.prev]
		mov di,[si+cptr.next]
		mov [bx+cptr.next],di
		mov [di+cptr.prev],bx

		popa
		ret

;
; cache_add_lru:
;	Add back a cache block to the expiry chain that was removed.
;
;	SI = cache block offset (any offset inside the block is OK)
;	Assumes CS == DS
;
cache_add_lru:
		pusha

		; Get metadata pointer
		shr si,SECTOR_SHIFT-cptr_size_lg2
		add si,CachePtrs+cptr_size

		mov bx,[CachePtrs+cptr.prev]
		mov [si+cptr.prev],bx
		mov [bx+cptr.next],si
		mov [CachePtrs+cptr.prev],si
		mov word [si+cptr.next],CachePtrs

		popa
		ret

;
; getcachesector: Check for a particular sector (EAX) in the sector cache,
;		  and if it is already there, return a pointer in GS:SI
;		  otherwise load it and return said pointer.
;
;		Assumes CS == DS.
;
getcachesector:
		push cx
		push bx
		push di
		mov si,cache_seg
		mov gs,si
		mov si,CachePtrs+cptr_size	; Real sector cache pointers
		mov cx,NCacheEntries
.search:
		cmp eax,[si]
		jz .hit
		add si,cptr_size
		loop .search

.miss:
		TRACER 'M'
		; Need to load it.
		push es
		push gs
		pop es
		mov bx,[CachePtrs+cptr.next]	; "Next most recent than head node"
		mov [bx+cptr.sector],eax
		mov si,bx
		sub bx,CachePtrs+cptr_size
		shl bx,SECTOR_SHIFT-cptr_size_lg2	; Buffer address
		pushad
%if IS_EXTLINUX
		call getonesec_ext
%else
		call getonesec
%endif
		popad
		pop es
.hit:
		; Update LRU, then compute buffer address
		TRACER 'H'

		sub si,CachePtrs+cptr_size
		shl si,SECTOR_SHIFT-cptr_size_lg2	; Buffer address

		; Remove from current position in the list
		call cache_remove_lru

		; Add to just before head node
		call cache_add_lru

		pop di
		pop bx
		pop cx
		ret

		section .latebss

		; Each CachePtr contains:
		; - Block pointer
		; - LRU previous pointer
		; - LRU next pointer
		; The first entry is the head node of the list
		alignb 4
CachePtrs	resb (NCacheEntries+1)*cptr_size
