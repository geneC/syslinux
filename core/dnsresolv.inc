; -*- fundamental -*-
; -----------------------------------------------------------------------
;
;   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
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
DNS_MAX_PACKET	equ     512			;Defined by protocol
                
                section .bss16
                global LocalDomain
                global DNSSendBuf, DNSRecvBuf
		alignb 2
DNSSendBuf	resb DNS_MAX_PACKET
DNSRecvBuf	resb DNS_MAX_PACKET
LocalDomain	resb 256		; Max possible length

                section .text16