;; -----------------------------------------------------------------------
;;   
;;   Copyright 1999 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
;;   USA; either version 2 of the License, or (at your option) any later
;;   version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------
;; $Id$

;;
;; pxe.inc
;;
;; PXE opcodes
;;

%ifndef _PXE_INC
%define _PXE_INC 1

%define PXENV_TFTP_OPEN			0020h
%define PXENV_TFTP_CLOSE		0021h
%define PXENV_TFTP_READ			0022h
%define PXENV_TFTP_READ_FILE		0023h
%define PXENV_TFTP_READ_FILE_PMODE	0024h
%define PXENV_TFTP_GET_FSIZE		0025h

%define PXENV_UDP_OPEN			0030h
%define PXENV_UDP_CLOSE			0031h
%define PXENV_UDP_READ			0032h
%define PXENV_UDP_WRITE			0033h

%define PXENV_UNDI_STARTUP		0001h
%define PXENV_UNDI_CLEANUP		0002h
%define PXENV_UNDI_INITIALIZE		0003h
%define PXENV_UNDI_RESET_NIC		0004h
%define PXENV_UNDI_SHUTDOWN		0005h
%define PXENV_UNDI_OPEN			0006h
%define PXENV_UNDI_CLOSE		0007h
%define PXENV_UNDI_TRANSMIT		0008h
%define PXENV_UNDI_SET_MCAST_ADDR	0009h
%define PXENV_UNDI_SET_STATION_ADDR	000Ah
%define PXENV_UNDI_SET_PACKET_FILTER	000Bh
%define PXENV_UNDI_GET_INFORMATION	000Ch
%define PXENV_UNDI_GET_STATISTICS	000Dh
%define PXENV_UNDI_CLEAR_STATISTICS	000Eh
%define PXENV_UNDI_INITIATE_DIAGS	000Fh
%define PXENV_UNDI_FORCE_INTERRUPT	0010h
%define PXENV_UNDI_GET_MCAST_ADDR	0011h

%define PXENV_UNLOAD_STACK		0070h
%define PXENV_GET_CACHED_INFO		0071h
%define PXENV_RESTART_DHCP		0072h
%define PXENV_RESTART_TFTP		0073h
%define PXENV_MODE_SWITCH		0074h

%endif ; _PXE_INC
