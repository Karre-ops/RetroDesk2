; RD32C0.ASM - Startcode fuer RD2/32-Programme (32 Bit, flach), PLAN_32BIT Schritt 4a
; SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
; Muss als erste Datei gelinkt werden (wlink format raw bin, offset=0, start=rd32_kopf): der Kopf liegt am Bildanfang.
; RD32.MOD laedt das Bild in DPMI-Speicher, CS/DS/SS haben die Bildbasis, Grenze 4 GB (lineare Adressen: rd32_lin).
; Umschaltung roh (DPMI 0306h), Taskblock (TB) unter 1 MB: Offsets wie RD32A.ASM.
.386p
.model flat
.code
public _rd32_tlin, _rd32_base, rd32_call_, rd32_int31_, rd32_takttramp_, _rd32_hattakt, rd32_kopf, setjmp_, longjmp_, __8087, __real87, __chipbug, ___chipbug
extrn rd32_main_:near, rd32_cb_:near, rd32_taktlauf_:near
rd32_kopf:
hdr     db 'R32!'
        dd offset pm_start            ; +4
        dd offset pm_cb               ; +8
        dd 0                          ; +12 frei (frueher Bildgroesse)
        dd offset rd32_ende           ; +16 Speicher bis Ende BSS (STACK-Segment liegt zuletzt, traegt der Linker ein)
        dd 16384                      ; +20 Stapel
_rd32_tlin dd 0                       ; +24 Taskblock linear (RD32.MOD)
_rd32_base dd 0                       ; +28 Bildbasis linear
        dd offset pm_preempt          ; +32 Zeitscheibe: TaskYield-Einschub (RD32 biegt die Rueckkehr hierher)
        dd offset rd32_brende         ; +36 bis hier nie unterbrechen (Bruecke)

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
        cmp  byte ptr [ebp+124], 0    ; 387 vorhanden (RD32.MOD): Gleitkomma der Laufzeit in Hardware
        je   ps_nofpu
        fninit
        mov  __8087, 3
        mov  __real87, 3
ps_nofpu:
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

setjmp_:                              ; EAX = jmp_buf: EBX, ESI, EDI, EBP, ESP, EIP
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
longjmp_:                             ; EAX = jmp_buf, EDX = Wert (0 -> 1)
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
lj_1:
        jmp  ecx
pm_preempt:                           ; vom Zeitgeber eingeschoben: TaskYield, dann weiter an der alten Stelle (TB_PREEIP)
        push eax                      ; Platz fuer die Rueckkehradresse
        pushad
        pushfd
        cld
        tbz
        mov  eax, [ebp+120]
        mov  [esp+36], eax
        push dword ptr [ebp+8]        ; laufenden Aufruf (Nummer, Woerter, Ergebnis) sichern
        push dword ptr [ebp+12]
        mov  dword ptr [ebp+8], 3     ; TaskYield, keine Woerter
        call rd32_call_
        tbz
        pop  dword ptr [ebp+12]
        pop  dword ptr [ebp+8]
        popfd
        popad
        ret
rd32_brende:

; Systemtakt (PLAN_API 2b, 24.11.2026): RD32 ruft hier im IRQ 0 (145,6 Hz) fern herein, DS = Datenselektor des Tasks,
; Interrupts gesperrt. Eigener Stapel im Bild, dann rd32_taktlauf (RD32LIB: TaktSet-Eintraege mit Teiler).
rd32_takttramp_:
        cli                           ; Stapelwechsel nie unterbrechbar (Audit 24.11.)
        mov  t_esp, esp
        mov  t_ss, ss
        mov  ax, ds
        mov  ss, ax
        mov  esp, offset t_top
        push es
        push fs
        push gs
        mov  es, ax
        cld
        call rd32_taktlauf_
        pop  gs
        pop  fs
        pop  es
        lss  esp, fword ptr t_esp
        retf

_DATA   segment dword public use32 'DATA'   ; Reihenfolge wie wcc386, dazu RD32ENDE als letztes BSS-Segment
_rd32_hattakt dd 1                    ; Programm: TaktSet moeglich (Module: 0)
__8087    db 0                        ; fuer Open Watcoms Gleitkomma (clib3r): 0 = Software, 3 = 387
__real87  db 0
          dw 0
__chipbug  dd 0
___chipbug dd 0
_DATA   ends
CONST   segment dword public use32 'DATA'
CONST   ends
CONST2  segment dword public use32 'DATA'
CONST2  ends
_BSS    segment dword public use32 'BSS'
t_esp   dd ?
t_ss    dw ?
        dw ?
t_stk   db 2048 dup(?)                ; Stapel fuer Takt-Rueckrufe
t_top   label byte
_BSS    ends
RD32ENDE segment dword public use32 'BSS'    ; leer: markiert das Ende des Speichers (Kopf +16)
public rd32_ende
rd32_ende label byte
RD32ENDE ends
DGROUP  group _DATA, CONST, CONST2, _BSS, RD32ENDE
end
