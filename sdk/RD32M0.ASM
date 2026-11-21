; RD32M0.ASM - Startcode fuer 32-Bit-Module (.M32), PLAN_32BIT: muss als erste Datei gelinkt werden (wie RD32C0).
; SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
; Ein Modul laeuft im Adressraum des Programms, das es laedt (rd32_modload, RD32PRG.C): tools/rd32mod.py linkt es zweimal,
; die Unterschiede sind die Verschiebungen. Was im Programm Zustand hat (Bruecke in den Realmodus, DPMI-Speicher,
; Vollbild), rufen die Stuecke unten ueber die Tabelle im Kopf auf, die der Lader fuellt. Das Modul liefert RdMod32Init.
.386p
.model flat
.code
public _rd32_tlin, _rd32_base, rd32_call_, rd32_int31_, rd32_alloc_, rd32_free_, SCREENMODE, ScreenPtr_, SCREENPALETTE, SCREENRESTORE, ScreenModeVesa_, ScreenVesaList_
public rd32_mkopf, setjmp_, longjmp_, __8087, __real87, __chipbug, ___chipbug, _rd32_jb, _rd32_ecode, _rd32_beiende
extrn RdMod32Init_:near
rd32_mkopf:
        db 'R32M'
        dd offset minit               ; +4  Einsprung: RdMod32Init, EAX = 0 ok
        dd 0                          ; +8  Bildgroesse (tools/rd32mod.py)
        dd offset rd32_mende          ; +12 Speicher bis Ende BSS
        dd 1                          ; +16 Fassung
_rd32_tlin dd 0                       ; +20 Taskblock linear   \
_rd32_base dd 0                       ; +24 Bildbasis linear    |
f_call  dd 0                          ; +28 rd32_call           |
f_alloc dd 0                          ; +32 rd32_alloc          |  fuellt der Lader
f_free  dd 0                          ; +36 rd32_free           |
f_smode dd 0                          ; +40 ScreenMode          |
f_sptr  dd 0                          ; +44 ScreenPtr           |
f_spal  dd 0                          ; +48 ScreenPalette       |
f_srest dd 0                          ; +52 ScreenRestore       |
f_int31 dd 0                          ; +56 rd32_int31         /
        dd offset _rd32_beiende       ; +60 Ende des Moduls (atexit, Dateien der Laufzeit), ruft rd32_modfree
        dd offset _rd32_jb            ; +64 Kopie aus dem Programm: exit im Modul beendet das Programm
        dd offset __8087              ; +68 Gleitkomma wie im Programm
f_svesa dd 0                          ; +72 ScreenModeVesa (Lader)
f_slist dd 0                          ; +76 ScreenVesaList (Lader)

minit:  jmp  RdMod32Init_
rd32_call_:    jmp dword ptr f_call
rd32_alloc_:   jmp dword ptr f_alloc
rd32_free_:    jmp dword ptr f_free
SCREENMODE:    jmp dword ptr f_smode
ScreenPtr_:    jmp dword ptr f_sptr
SCREENPALETTE: jmp dword ptr f_spal
SCREENRESTORE: jmp dword ptr f_srest
rd32_int31_:   jmp dword ptr f_int31
ScreenModeVesa_: jmp dword ptr f_svesa
ScreenVesaList_: jmp dword ptr f_slist

setjmp_:                              ; wie RD32C0: EAX = jmp_buf
        mov  [eax], ebx
        mov  [eax+4], esi
        mov  [eax+8], edi
        mov  [eax+12], ebp
        lea  ecx, [esp+4]
        mov  [eax+16], ecx
        mov  ecx, [esp]
        mov  [eax+20], ecx
        xor  eax, eax
        ret
longjmp_:
        mov  ebx, [eax]
        mov  esi, [eax+4]
        mov  edi, [eax+8]
        mov  ebp, [eax+12]
        mov  esp, [eax+16]
        mov  ecx, [eax+20]
        mov  eax, edx
        test eax, eax
        jnz  lj_1
        inc  eax
lj_1:   jmp  ecx

_DATA   segment dword public use32 'DATA'
__8087    db 0
__real87  db 0
          dw 0
__chipbug  dd 0
___chipbug dd 0
_rd32_jb   dd 6 dup(0)
_rd32_ecode dd 0
_rd32_beiende dd 0
_DATA   ends
CONST   segment dword public use32 'DATA'
CONST   ends
CONST2  segment dword public use32 'DATA'
CONST2  ends
_BSS    segment dword public use32 'BSS'
_BSS    ends
RD32ENDE segment dword public use32 'BSS'
rd32_mende label byte
RD32ENDE ends
DGROUP  group _DATA, CONST, CONST2, _BSS, RD32ENDE
end
