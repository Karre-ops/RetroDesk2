; RD32C0.ASM - Startcode fuer RD2/32-Programme (32 Bit, flach), PLAN_32BIT Schritt 4a
; SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
; Muss als erste Datei gelinkt werden (wlink format raw bin, offset=0, start=rd32_kopf): der Kopf liegt am Bildanfang.
; RD32.MOD laedt das Bild in DPMI-Speicher, CS/DS/SS haben die Bildbasis, Grenze 4 GB (lineare Adressen: rd32_lin).
; Umschaltung roh (DPMI 0306h), Taskblock (TB) unter 1 MB: Offsets wie RD32A.ASM.
.386p
.model flat
.code
public _rd32_tlin, _rd32_base, rd32_call_, rd32_int31_, rd32_kopf
extrn rd32_main_:near, rd32_cb_:near
rd32_kopf:
hdr     db 'R32!'
        dd offset pm_start            ; +4
        dd offset pm_cb               ; +8
        dd 0                          ; +12 frei (frueher Bildgroesse)
        dd offset rd32_ende           ; +16 Speicher bis Ende BSS (STACK-Segment liegt zuletzt, traegt der Linker ein)
        dd 16384                      ; +20 Stapel
_rd32_tlin dd 0                       ; +24 Taskblock linear (RD32.MOD)
_rd32_base dd 0                       ; +28 Bildbasis linear
        dd 0, 0

tbz macro                             ; EBP = Taskblock (Bild-relativ, ueber 4 GB umlaufend)
        mov  ebp, _rd32_tlin
        sub  ebp, _rd32_base
endm
torm macro ziel                       ; zurueck in den Realmodus: DS = TB, SS:SP = TB_RMSS:TB_RMSP, CS:IP = TB_RMCS:ziel
        movzx eax, word ptr [ebp+38]
        mov  ecx, eax
        movzx edx, word ptr [ebp+2]
        movzx ebx, word ptr [ebp+0]
        movzx esi, word ptr [ebp+36]
        movzx edi, word ptr [ebp+ziel]
        jmp  fword ptr [ebp+20]
endm

pm_start:                             ; erster Einsprung, ESP = Stapelspitze
        sti
        cld
        mov  ax, ds
        mov  es, ax
        tbz
        mov  dword ptr [ebp+48], offset pm_back
        call rd32_main_
        jmp  short pm_ret
pm_cb:                                ; Rueckruf (Fensterprozedur), ESP = TB_PMESP
        sti
        cld
        mov  ax, ds
        mov  es, ax
        call rd32_cb_
pm_ret:
        tbz
        mov  [ebp+52], eax            ; TB_CBRES
        torm 42                       ; rm_cbret

rd32_call_:                           ; API-Aufruf: Nummer und Woerter stehen schon im TB
        pushad
        tbz
        push dword ptr [ebp+4]
        mov  [ebp+4], esp
        torm 40                       ; rm_api
rd32_int31_:                          ; DPMI-Aufruf: EAX = Zeiger auf { eax, ebx, ecx, edx, esi, edi, cf }
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ebp
        mov  ebp, eax
        mov  eax, [ebp]
        mov  ebx, [ebp+4]
        mov  ecx, [ebp+8]
        mov  edx, [ebp+12]
        mov  esi, [ebp+16]
        mov  edi, [ebp+20]
        push ebp
        int  31h
        pop  ebp
        mov  [ebp], eax
        mov  [ebp+4], ebx
        mov  [ebp+8], ecx
        mov  [ebp+12], edx
        mov  [ebp+16], esi
        mov  [ebp+20], edi
        sbb  eax, eax
        mov  [ebp+24], eax
        pop  ebp
        pop  edi
        pop  esi
        pop  edx
        pop  ecx
        pop  ebx
        ret

pm_back:                              ; vom Realmodus: DS = ES = SS = Datenselektor, ESP = TB_PMESP
        sti
        cld
        tbz
        pop  dword ptr [ebp+4]
        popad
        ret

_DATA   segment dword public use32 'DATA'   ; Reihenfolge wie wcc386, dazu RD32ENDE als letztes BSS-Segment
_DATA   ends
CONST   segment dword public use32 'DATA'
CONST   ends
CONST2  segment dword public use32 'DATA'
CONST2  ends
_BSS    segment dword public use32 'BSS'
_BSS    ends
RD32ENDE segment dword public use32 'BSS'    ; leer: markiert das Ende des Speichers (Kopf +16)
public rd32_ende
rd32_ende label byte
RD32ENDE ends
DGROUP  group _DATA, CONST, CONST2, _BSS, RD32ENDE
end
