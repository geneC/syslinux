;; -*- fundamental -*- ---------------------------------------------------
;;
;;   Copyright 2008 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;;   Boston MA 02110-1301, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; pxeidle.inc
;;
;;
;; Query for the NIC type, and detect certain special cases.
;;

		section .text

;;
;; Initializes the idle mechanism based on the device type
;;
;;	Assumes CS == DS == ES
;;
pxe_detect_nic_type:
		pushad

		mov di,pxenv_get_nic_type
		mov bx,PXENV_UNDI_GET_NIC_TYPE
		call pxenv
		jc .done
		cmp word [di],0
		jne .done
		cmp byte [di+2],2		; PCI_NIC
		jne .done			; No list for non-PCI nics

		mov cx,pxe_idle_pci_list.len
		mov si,pxe_idle_pci_list
.look_for_id:
		lodsd
		cmp eax,[di+3]			; VID:DID
		je .found_device
		loop .look_for_id
.done:
		popad
		ret

.found_device:
		and byte [feature_flags],~2
		jmp .done

;;
;; List of devices for which we want to actually issue idle calls.
;;
		section .data
		alignz 4
pxe_idle_pci_list:
;
; Older Broadcom NICs; these need idle calls to avoid FIFO stalls.
;
		dw 0x14e4, 0x1659		; BCM5721
		dw 0x14e4, 0x165a		; BCM5722
		dw 0x14e4, 0x165b		; BCM5723
		dw 0x14e4, 0x1668		; BCM5714
		dw 0x14e4, 0x1669		; BCM5714S
		dw 0x14e4, 0x166a		; BCM5780
		dw 0x14e4, 0x166b		; BCM5780S
		dw 0x14e4, 0x1673		; BCM5755M
		dw 0x14e4, 0x1674		; BCM5756ME
		dw 0x14e4, 0x1678		; BCM5715
		dw 0x14e4, 0x1679		; BCM5715S
		dw 0x14e4, 0x167b		; BCM5755
;
.len		equ ($-pxe_idle_pci_list) >> 2

		section .bss
pxenv_get_nic_type:
.status:	resw 1
.nic_type:	resb 1
.vid:		resw 1
.did:		resw 1
.base_class:	resb 1
.sub_class:	resb 1
.prog_intf:	resb 1
.rev:		resb 1
.busdevfunc:	resw 1
.svid:		resw 1
.sdid:		resw 1

		section .text
