; -*- fundamental -*- ---------------------------------------------------
;   
;   Copyright 2004 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------
; $Id$

		section .text
;
; initcache: Initialize the cache data structures
;
initcache:
		xor eax,eax			; We don't care about sector 0
		mov di,CachePtrs
		mov cx,65536/SECTOR_SIZE
		rep stosd
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
		mov si,cache_seg
		mov gs,si
		mov si,CachePtrs	; Sector cache pointers
		mov cx,65536/SECTOR_SIZE
.search:
		cmp eax,[si]
		jz .hit
		add si,4
		loop .search

.miss:
		TRACER 'M'
		; Need to load it.  Highly inefficient cache replacement
		; algorithm: Least Recently Written (LRW)
		push bx
		push es
		push gs
		pop es
		mov bx,[NextCacheSlot]
		inc bx
		and bx,(1 << (16-SECTOR_SHIFT))-1
		mov [NextCacheSlot],bx
		shl bx,2
		mov [CachePtrs+bx],eax
		shl bx,SECTOR_SHIFT-2
		mov si,bx
		pushad
		call getonesec
		popad
		pop es
		pop bx
		pop cx
		ret

.hit:		; We have it; get the pointer
		TRACER 'H'
		sub si,CachePtrs
		shl si,SECTOR_SHIFT-2
		pop cx
		ret

		section .bss
		alignb 4
CachePtrs	resd 65536/SECTOR_SIZE	; Cached sector pointers
NextCacheSlot	resw 1			; Next cache slot to occupy
