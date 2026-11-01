; RD2MOD.ASM - Startcode fuer RD2-Module (.MOD/.DRV), Modell Medium, mit -zu bauen.
; SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
; Der Kern ruft den Einsprung mit AX = 5232h, ES:BX = API-Tabelle, CX = HMOD.
; RdModInit() liefert in DX:AX einen Zeiger auf: WORD anzahl; FARPROC f[anzahl].
; Exportierte Funktionen laufen mit dem Stack des Aufrufers: RDEXPORT (__loadds).
.8086
RD2MOD_TEXT segment word public 'CODE'
RD2MOD_TEXT ends
DGROUP group _DATA, CONST, CONST2, _BSS, STACK
_DATA segment word public 'DATA'
public __rd2api
__rd2api dd 0
nomsg    db 'Dies ist ein RetroDesk-2-Modul.', 13, 10, '$'
_DATA ends
CONST  segment word public 'DATA'
CONST  ends
CONST2 segment word public 'DATA'
CONST2 ends
_BSS   segment word public 'BSS'
bss_anf label byte                  ; erstes Objekt: Anfang von _BSS
_BSS   ends
STACK  segment para stack 'STACK'
bss_end label byte                  ; _BSS endet vor STACK
       db 16 dup(?)
STACK  ends

RD2MOD_TEXT segment word public 'CODE'
assume cs:RD2MOD_TEXT, ds:DGROUP
extrn RDMODINIT:far
_start:
    cmp  ax, 5232h
    jne  dosrun
    push ds
    mov  ax, DGROUP
    mov  ds, ax
    push es                          ; _BSS loeschen
    push di
    mov  es, ax
    mov  di, offset DGROUP:bss_anf
    mov  cx, offset DGROUP:bss_end
    sub  cx, di
    xor  al, al
    cld
    rep  stosb
    pop  di
    pop  es
    mov  word ptr __rd2api, bx
    mov  word ptr __rd2api+2, es
    call RDMODINIT
    pop  ds
    retf
dosrun:
    mov  ax, DGROUP
    mov  ds, ax
    mov  dx, offset DGROUP:nomsg
    mov  ah, 9
    int  21h
    mov  ax, 4C01h
    int  21h
RD2MOD_TEXT ends
end _start
