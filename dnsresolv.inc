; -*- fundamental -*-
; -----------------------------------------------------------------------
;   
;   Copyright 2004 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Bostom MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; dnsresolv.inc
;
; Very simple DNS resolver (assumes recursion-enabled DNS server;
; this should be the normal thing for client-serving DNS servers.)
;

%include "macros.inc"
%include "pxe.inc"

DNS_PORT	equ htons(53)		; Default DNS port 
DNS_MAX_PACKET	equ 512			; Defined by protocol
; TFTP uses the range 49152-57343; avoid that range
DNS_LOCAL_PORT	equ htons(5353)		; All local DNS queries come from this port #
DNS_MAX_SERVERS equ 4			; Max no of DNS servers

		section .text

;
; Turn a string in DS:SI into a DNS "label set" in ES:DI.
; On return, DI points to the first byte after the label set.
;
; Assumes DS == ES.  Change references to [bx] to [es:bx] to remove
; that assumption.
;
dns_mangle:
		push ax
		push bx
.nostart:
.isdot:
		xor al,al
		mov bx,di
		stosb
.getbyte:
		lodsb
		and al,al
		jz .endstring
		cmp al,'.'
		je .isdot
		inc byte [bx]
		stosb
		jmp .getbyte
.endstring:
		cmp byte [bx],0
		jz .done
		xor al,al
		stosb
.done:
		pop bx
		pop ax
		ret

;
; Compare two sets of DNS labels, in DS:SI and ES:DI; the one in SI
; is allowed pointers relative to a packet address in BX.
;
; Assumes DS == ES.  ZF = 1 if same; no registers changed.
; (Note: change reference to [di] to [es:di] to remove DS == ES assumption)
;
dns_compare:
		pusha

.label:
		lodsb
		cmp al,0C0h
		jb .noptr
		and al,03Fh			; Get pointer value
		mov ah,al			; ... in network byte order!
		lodsb
		mov si,bx
		add si,ax
		jmp .label
.noptr:
		cmp al,[di]
		jne .done			; Mismatch
		inc di
		movzx cx,al			; End label?
		and cx,cx			; ZF = 1 if match
		jz .done
		
		; We have a string of bytes that need to match now
		repe cmpsb
		jz .label

.done:
		popa
		ret

;
; Skip past a DNS label set in DS:SI.
;
dns_skiplabel:
		push ax
		xor ax,ax			; AH == 0
.loop:
		lodsb
		cmp al,0C0h			; Pointer?
		jae .ptr
		and al,al
		jz .done
		add si,ax
		jmp .loop
.ptr:
		inc si				; Pointer is two bytes
.done:
		pop ax
		ret

; Actual resolver function
; Points to a null-terminated string in DS:SI
; and returns the name in EAX if it exists and can be found.
; If EAX = 0 on exit, the lookup failed.

		; DNS header format
		struc dnshdr
.id:		resw 1
.flags:		resw 1
.qdcount:	resw 1
.ancount:	resw 1
.nscount:	resw 1
.arcount:	resw 1
		endstruc

		; DNS query
		struc dnsquery
.qtype:		resw 1
.qclass:	resw 1
		endstruc

		; DNS RR
		struc dnsrr
.type:		resw 1
.class:		resw 1
.ttl:		resd 1
.rdlength:	resw 1
.rdata:		equ $
		endstruc

		section .bss
		alignb 2, db 0
DNSSendBuf	resb DNS_MAX_PACKET
DNSRecvBuf	resb DNS_MAX_PACKET
DNSServers	resd DNS_MAX_SERVERS

		section .data
pxe_udp_write_pkt_dns:
.status:	dw 0			; Status
.sip:		dd 0			; Server IP
.gip:		dd 0			; Gateway IP
.lport:		dw DNS_LOCAL_PORT	; Local port
.rport:		dw DNS_PORT		; Remote port
.buffersize:	dw 0			; Size of packet
.buffer:	dw 0, DNSSendBuf	; seg:off of buffer

pxe_udp_read_pkt_dns:
.status:	dw 0			; Status
.sip:		dd 0			; Source IP
.dip:		dd 0			; Destination (our) IP
.rport:		dw DNS_PORT		; Remote port
.lport:		dw DNS_LOCAL_PORT	; Local port
.buffersize:	dw DNS_MAX_PACKET	; Max packet size
.buffer:	dw 0, DNSRecvBuf	; seg:off of buffer

LastDNSServer	dw DNSServers

		section .text
dns_resolv:
		push di
		push cx
		push dx

		; Initialize...
		mov eax,[MyIP]
		mov [pxe_udp_read_pkt_dns.dip],eax

		; First, build query packet
		mov di,DNSSendBuf+dnshdr.flags
		inc word [di-2]		; New query ID
		mov ax,htons(0100h)	; Recursion requested
		stosw
		mov ax,htons(1)		; One question
		stosw
		xor ax,ax		; No answers, NS or ARs
		stosw
		stosw
		stosw
		
		call dns_mangle		; Convert name to DNS labels
		mov ax,htons(1)
		stosw			; QTYPE  = 1 = A
		stosw			; QCLASS = 1 = IN

		sub di,DNSSendBuf
		mov [pxe_udp_write_pkt_dns.buffersize],di

		; Now, send it to the nameserver(s)
		; Outer loop: exponential backoff
		; Inner loop: scan the various DNS servers

		mov dx,PKT_TIMEOUT
		mov cx,PKT_RETRY
.backoff:
		mov di,[DNSServers]
.servers:
		cmp di,[LastDNSServer]
		jae .nomoreservers

		push di
		push cx
		push dx

		mov eax,[di]

		mov word [pxe_udp_write_pkt_dns.status],0	

		mov [pxe_udp_write_pkt_dns.sip],eax
		mov [pxe_udp_read_pkt_dns.sip],eax
		xor eax,[MyIP]
		and eax,[Netmask]
		jz .nogw
		mov eax,[Gateway]
.nogw:
		mov [pxe_udp_write_pkt_dns.gip],eax

		mov di,pxe_udp_write_pkt_dns
		mov bx,PXENV_UDP_WRITE
		call pxenv
		jc .timeout				; Treat failed transmit as timeout
		cmp word [pxe_udp_write_pkt_dns.status],0
		jne .timeout

		mov cx,[BIOS_timer]
.waitrecv:
		mov ax,[BIOS_timer]
		sub ax,cx
		cmp ax,dx
		jae .timeout

		mov word [pxe_udp_read_pkt_dns.status],0
		mov word [pxe_udp_read_pkt_dns.buffersize],DNS_MAX_PACKET
		mov di,pxe_udp_read_pkt_dns
		mov bx,PXENV_UDP_READ
		call pxenv
		and ax,ax
		jnz .waitrecv

		; Got a packet, deal with it...
		hlt

.timeout:
		pop dx
		pop cx
		pop di
		add di,4
		jmp .servers
.nomoreservers:
		add dx,dx			; Exponential backoff
		loop .backoff

		xor eax,eax			; Nothing...
.done:
		push dx
		push cx
		push di
		ret
