		name    cstart
		assume  nothing
	
		extern _start_:proc

DGROUP		group _TEXT,CONST,STRINGS,_DATA,DATA,XIB,XI,XIE,YIB,YI,YIE,_BSS
	
_TEXT		segment use16 para public 'CODE'

		assume  cs:_TEXT

		org 100h
_cstart_	proc near

		call _start_

		mov ah,4Ch		; AL = exit code
		int 21h
	
_cstart_	endp

_TEXT		ends

		; Make sure we declare all the DGROUP segments...

CONST		segment word public 'DATA'
CONST		ends	
STRINGS		segment word public 'DATA'
STRINGS		ends	
XIB		segment word public 'DATA'
XIB		ends	
XI		segment word public 'DATA'
XI		ends	
XIE		segment word public 'DATA'
XIE		ends	
YIB		segment word public 'DATA'
YIB		ends	
YI		segment word public 'DATA'
YI		ends	
YIE		segment word public 'DATA'
YIE		ends	
DATA		segment word public 'DATA'
DATA		ends	
	
		public _small_code_

_DATA		segment word public 'DATA'
_small_code_	db 0
_DATA		ends

_BSS		segment word public 'BSS'
_BSS		ends
		
		end _cstart_

