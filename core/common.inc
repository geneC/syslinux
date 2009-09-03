;
; Modules common to all derivatives.  Do not include modules in this list
; which have special section requirements (i.e. need to be in .init for
; some derivatives.)
;

%include "getc.inc"		; getc et al
%include "conio.inc"		; Console I/O
%include "configinit.inc"	; Initialize configuration
%include "parseconfig.inc"	; High-level config file handling
%include "parsecmd.inc"		; Low-level config file handling
%include "pm.inc"		; Protected mode
%include "bcopy32.inc"		; 32-bit bcopy
%include "loadhigh.inc"		; Load a file into high memory
%include "font.inc"		; VGA font stuff
%include "graphics.inc"		; VGA graphics
%include "highmem.inc"		; High memory sizing
%include "strcpy.inc"           ; strcpy()
%include "idle.inc"		; Idle handling
%include "adv.inc"		; Auxillary Data Vector
%include "timer.inc"		; Timer handling

; Note: the prefix section is included late, to avoid problems with some
; versions of NASM that had issues with forward references to EQU symbols.
%include "prefix.inc"		; Prefix section for prepcore
