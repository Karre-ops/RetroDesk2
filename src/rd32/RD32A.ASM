; RD32A.ASM - RD32.MOD, Realmodus-Teil der 32-Bit-Bruecke (PLAN_32BIT Schritt 4a, ab 386)
; SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
; RD2 bleibt im Realmodus. Beim ersten RD2/32-Programm meldet sich RD2 als 32-Bit-DPMI-Client an (CWSDPMI o. a.)
; und wechselt danach nur noch ueber die rohe Umschaltung (DPMI 0306h) zwischen den Modi, ohne Verschachtelung im Host.
; Je 32-Bit-Task ein Taskblock (TB, eigenes Segment unter 1 MB): Stapelzeiger beider Seiten, Aufrufnummer, Woerter,
; Ergebnis, Rueckruf-Daten, Austauschpuffer. Offsets wie in RD32.C / RD32LIB.C (TB_*).
; API-Aufruf aus dem 32-Bit-Code: rm_api legt die Woerter auf den Stapel des Tasks und ruft call far [api + nr*4].
; Rueckruf (Fensterprozedur, Start): rm_topm sichert den Realmodus-Stapel und springt zum 32-Bit-Code; rm_cbret kommt zurueck.
.386
DGROUP group _DATA
_DATA segment word public use16 'DATA'
public _g
_g label byte
g_entry   dd 0        ;  0 DPMI-Einsprung (2F/1687h)
g_r2p     dd 0        ;  4 Realmodus -> PM (off, seg)
g_p2r     dd 0        ;  8 PM -> Realmodus (off32)
g_p2rsel  dw 0        ; 12            (Selektor)
g_pcs     dw 0        ; 14 PM-Selektor fuer RD32A_TEXT
g_pds     dw 0        ; 16 PM-Selektor fuer DGROUP
g_flat    dw 0        ; 18 4-GB-Datenselektor, Basis 0
g_rmss    dw 0        ; 20
g_rmsp    dw 0        ; 22
g_rmds    dw 0        ; 24 DGROUP
g_rmcs    dw 0        ; 26 RD32A_TEXT
g_svcnr   dw 0        ; 28
g_err     dw 0        ; 30
g_stsize  dw 0        ; 32 Zustandsspeicher (0305h)
g_svcsp   dw 0        ; 34 Stapel fuer Dienste im PM
g_a       dd 0        ; 36
g_b       dd 0        ; 40
g_c       dd 0        ; 44
g_d       dd 0        ; 48
g_api     dd 0        ; 52 API-Tabelle
g_tab     dw 16 dup(0); 56 je Task: HTASK, TB-Segment
g_old21   dd 0        ; 88 INT 21h waehrend des Abmeldens
g_psp     dw 0        ; PSP von RD2
g_pspsv   db 14 dup(0); PSP 0Ah..17h (Ende-, Abbruch-, Fehleradresse, Eltern-PSP)
g_ivtsv   dd 3 dup(0) ; INT 22h..24h
g_p32seg  dw 0        ; Segment RD32P_TEXT (32-Bit-Code: Zeitgeber, Ausnahmen)
g_p32cs   dw 0        ; Selektor dafuer
g_tick    dd 0        ; Zeitgeber-Takte im PM
public _g_preon, _g_cs32, _g_tblin, _g_pmin, _g_pstub, _g_extb, _g_exnr, _g_exeip
_g_preon  dw 0        ; 1 = 32-Bit-Tasks praeemptiv umschalten (nicht bei RD2 /KOOP)
_g_cs32   dw 8 dup(0) ; je Task (Index wie g_tab): Code-Selektor, 0 = frei
_g_tblin  dd 8 dup(0) ;   Taskblock linear
_g_pmin   dd 8 dup(0) ;   erste Adresse nach RD32C0 (davor nie unterbrechen)
_g_pstub  dd 8 dup(0) ;   pm_preempt im Bild
_g_extb   dw 0        ; Ausnahme: Taskblock-Segment, Nummer (Index), Adresse
_g_exnr   dw 0
_g_exeip  dd 0
g_jo      dd 0        ; Sprungziel beim Weiterreichen
g_js      dw 0
g_stk     db 384 dup(0)
public _g_stktop
_g_stktop label byte
_DATA ends

TB_RMSP  equ 0
TB_RMSS  equ 2
TB_PMESP equ 4
TB_NR    equ 8
TB_NW    equ 10
TB_RES   equ 12
TB_API   equ 16
TB_P2R   equ 20       ; off32 + sel
TB_R2P   equ 28       ; off, seg
TB_CS32  equ 32
TB_DS32  equ 34
TB_RMCS  equ 36
TB_RMDS  equ 38
TB_RMAPI equ 40
TB_RMCB  equ 42
TB_PMCB  equ 44
TB_PMBACK equ 48
TB_CBRES equ 52
TB_CBSLOT equ 56
TB_CBW   equ 58
TB_CBMSG equ 60
TB_CBP1  equ 62
TB_CBP2  equ 64
TB_PMSTART equ 76
TB_ARGS  equ 128

RD32A_TEXT segment word public use16 'CODE'
assume cs:RD32A_TEXT, ds:DGROUP
public dpmi_enter_, dpmi_leave_, svc_call_, rm_kill_, rm_start_, rm_api_, rm_cbret_, wp0_, wp1_, wp2_, wp3_, wp4_, wp5_, wp6_, wp7_

; ---- Anmelden beim DPMI-Host: AX = Segment fuer die Host-Daten. Ergebnis in g_err (0 = ok) -------------------
dpmi_enter_ proc far
    pushad
    push ds
    push es
    push fs
    push gs
    mov  bx, DGROUP
    mov  ds, bx
    mov  es, ax
    mov  g_rmds, ds
    mov  g_rmcs, cs
    mov  g_rmss, ss
    mov  g_rmsp, sp
    push ax
    mov  ax, RD32P_TEXT
    mov  g_p32seg, ax
    pop  ax
    mov  g_err, 1
    mov  ax, 1                        ; 32-Bit-Client
    call dword ptr g_entry
    jnc  de_pm
    jmp  de_back
de_pm:
    mov  g_pcs, cs
    mov  g_pds, ds
    mov  ax, 0306h                    ; rohe Umschaltung
    int  31h
    mov  word ptr g_r2p, cx
    mov  word ptr g_r2p+2, bx
    mov  g_p2r, edi
    mov  g_p2rsel, si
    mov  ax, 0305h
    int  31h
    mov  g_stsize, ax
    mov  g_err, 2
    xor  ax, ax                       ; flacher Datenselektor (Kopieren)
    mov  cx, 1
    int  31h
    jc   de_rm
    mov  g_flat, ax
    mov  bx, ax
    mov  dx, cs
    and  dl, 3
    shl  dl, 5
    mov  cl, 92h
    or   cl, dl
    mov  ch, 0C0h
    mov  ax, 0009h
    int  31h
    mov  bx, g_flat
    xor  cx, cx
    xor  dx, dx
    mov  ax, 0007h
    int  31h
    mov  bx, g_flat
    mov  cx, 0FFFFh
    mov  dx, 0FFFFh
    mov  ax, 0008h
    int  31h
    jc   de_rm
    mov  g_err, 3                     ; 32-Bit-Codeselektor fuer RD32P_TEXT, Alias zum Beschreiben
    xor  ax, ax
    mov  cx, 1
    int  31h
    jc   de_rm
    mov  g_p32cs, ax
    mov  bx, ax
    mov  dx, cs
    and  dl, 3
    shl  dl, 5
    mov  cl, 9Ah
    or   cl, dl
    mov  ch, 40h
    mov  ax, 0009h
    int  31h
    jc   de_rm
    movzx eax, g_p32seg
    shl  eax, 4
    mov  dx, ax
    shr  eax, 16
    mov  cx, ax
    mov  bx, g_p32cs
    mov  ax, 0007h
    int  31h
    mov  bx, g_p32cs
    xor  cx, cx
    mov  dx, 0FFFFh
    mov  ax, 0008h
    int  31h
    mov  bx, g_p32cs
    mov  ax, 000Ah
    int  31h
    jc   de_rm
    push es
    mov  es, ax
    mov  bx, ax                       ; Alias merken (bleibt)
    mov  ax, g_pds
    mov  word ptr es:p_pds, ax
    mov  ax, 0204h                    ; alter PM-Zeitgeber
    mov  bl, 8
    int  31h
    mov  dword ptr es:p_old8, edx
    mov  word ptr es:p_old8+4, cx
    xor  si, si
de_ex1:
    mov  bl, byte ptr cs:exnum[si]
    mov  ax, 0202h
    int  31h
    mov  di, si
    imul di, 6
    mov  dword ptr es:p_oldex[di], edx
    mov  word ptr es:p_oldex[di+4], cx
    inc  si
    cmp  si, 5
    jb   de_ex1
    pop  es
    mov  ax, 0205h                    ; eigener PM-Zeitgeber (Zeitscheiben fuer 32-Bit-Tasks)
    mov  bl, 8
    mov  cx, g_p32cs
    mov  edx, offset i8pm
    int  31h
    xor  si, si
de_ex2:
    mov  bl, byte ptr cs:exnum[si]
    mov  cx, g_p32cs
    mov  di, si
    shl  di, 2
    mov  edx, dword ptr cs:exofs[di]
    mov  ax, 0203h
    int  31h
    inc  si
    cmp  si, 5
    jb   de_ex2
    mov  g_err, 0
de_rm:
    mov  ax, g_rmds
    mov  cx, ax
    mov  dx, g_rmss
    movzx ebx, g_rmsp
    movzx esi, g_rmcs
    mov  edi, offset de_back
    jmp  fword ptr g_p2r
de_back:
    sti
    pop  gs
    pop  fs
    pop  es
    pop  ds
    popad
    retf
dpmi_enter_ endp

; ---- Abmelden beim DPMI-Host am Ende von RD2: im PM INT 21h/4Ch, der Host raeumt den Client ab und reicht 4Ch an
; DOS weiter; dort faengt ein kurz gesetzter INT-21h-Vektor den Aufruf ab und kehrt hierher zurueck (RD2 endet normal).
dpmi_leave_ proc far
    pushad
    push ds
    push es
    push fs
    push gs
    mov  bx, DGROUP
    mov  ds, bx
    mov  ah, 62h                      ; PSP von RD2 und was der Host beim Beenden dort umbiegt sichern
    int  21h
    mov  g_psp, bx
    mov  es, bx
    mov  si, 0Ah
    mov  di, offset g_pspsv
dl_s1:
    mov  al, es:[si]
    mov  [di], al
    inc  si
    inc  di
    cmp  si, 18h
    jb   dl_s1
    xor  ax, ax
    mov  es, ax
    mov  eax, es:[22h*4]
    mov  g_ivtsv, eax
    mov  eax, es:[23h*4]
    mov  g_ivtsv+4, eax
    mov  eax, es:[24h*4]
    mov  g_ivtsv+8, eax
    mov  eax, es:[21h*4]
    mov  g_old21, eax
    cli
    mov  word ptr es:[21h*4], offset dl_rm
    mov  word ptr es:[21h*4+2], cs
    sti
    mov  g_rmss, ss
    mov  g_rmsp, sp
    mov  ax, g_pds
    mov  cx, ax
    mov  dx, ax
    movzx ebx, g_svcsp
    movzx esi, g_pcs
    mov  edi, offset dl_pm
    jmp  dword ptr g_r2p
dl_pm:                                ; erst Zeitgeber und Ausnahmen wieder an den Host (sonst zeigen sie beim naechsten Client ins Leere)
    mov  es, g_p32cs                  ; lesbarer Code-Selektor fuer RD32P_TEXT
    mov  edx, dword ptr es:p_old8
    mov  cx, word ptr es:p_old8+4
    mov  bl, 8
    mov  ax, 0205h
    int  31h
    xor  si, si
dl_ex:
    mov  di, si
    imul di, 6
    mov  edx, dword ptr es:p_oldex[di]
    mov  cx, word ptr es:p_oldex[di+4]
    mov  bl, byte ptr cs:exnum[si]
    mov  ax, 0203h
    int  31h
    inc  si
    cmp  si, 5
    jb   dl_ex
    mov  ax, 4C00h
    int  21h
dl_halt:
    jmp  dl_halt
dl_rm:                                ; Realmodus, vom Host (oder DOS-Kette): Vektor zurueck, Stapel wie vorher
    cli
    mov  ax, DGROUP
    mov  ds, ax
    xor  ax, ax
    mov  es, ax
    mov  eax, g_old21
    mov  es:[21h*4], eax
    mov  eax, g_ivtsv
    mov  es:[22h*4], eax
    mov  eax, g_ivtsv+4
    mov  es:[23h*4], eax
    mov  eax, g_ivtsv+8
    mov  es:[24h*4], eax
    mov  es, g_psp
    mov  si, offset g_pspsv
    mov  di, 0Ah
dl_s2:
    mov  al, [si]
    mov  es:[di], al
    inc  si
    inc  di
    cmp  di, 18h
    jb   dl_s2
    mov  bx, g_psp                    ; aktueller PSP wieder RD2 (der Host kann ihn umgestellt haben)
    mov  ah, 50h
    int  21h
    mov  ss, g_rmss
    mov  sp, g_rmsp
    sti
    cld
    pop  gs
    pop  fs
    pop  es
    pop  ds
    popad
    retf
dpmi_leave_ endp

; ---- Dienst im PM (Speicher, Selektoren, Kopieren): AX = Nummer, Daten in g_a..g_d, Ergebnis g_err -----------
svc_call_ proc far
    pushad
    push ds
    push es
    push fs
    push gs
    mov  bx, DGROUP
    mov  ds, bx
    mov  g_svcnr, ax
    mov  g_rmss, ss
    mov  g_rmsp, sp
    mov  ax, g_pds
    mov  cx, ax
    mov  dx, ax
    movzx ebx, g_svcsp
    movzx esi, g_pcs
    mov  edi, offset svc_pm
    jmp  dword ptr g_r2p
svc_pm:
    sti
    cld
    mov  ax, g_svcnr
    cmp  ax, 1
    je   s_alloc
    cmp  ax, 2
    je   s_copy
    cmp  ax, 3
    je   s_free
    cmp  ax, 4
    je   s_zero
    mov  ax, 0FFFFh
    jmp  s_err
s_alloc:                              ; g_a Groesse -> g_b linear, g_c Handle, g_d Selektoren (Code, Daten)
    mov  bx, word ptr g_a+2
    mov  cx, word ptr g_a
    mov  ax, 0501h
    int  31h
    jc   s_err
    mov  word ptr g_b, cx
    mov  word ptr g_b+2, bx
    mov  word ptr g_c, di
    mov  word ptr g_c+2, si
    xor  ax, ax
    mov  cx, 2
    int  31h
    jc   s_err
    mov  word ptr g_d, ax
    mov  ax, 0003h
    int  31h
    add  ax, word ptr g_d
    mov  word ptr g_d+2, ax
    mov  bx, word ptr g_d
    mov  al, 9Ah                      ; Code, lesbar
    call s_desc
    jc   s_err
    mov  bx, word ptr g_d+2
    mov  al, 92h                      ; Daten, schreibbar
    call s_desc
    jc   s_err
    jmp  s_ok
s_copy:                               ; g_a Quelle, g_b Ziel (linear), g_c Bytes
    push es
    push ds
    mov  esi, g_a
    mov  edi, g_b
    mov  ecx, g_c
    mov  ax, g_flat
    mov  es, ax
    mov  ds, ax
    rep  movs byte ptr es:[edi], byte ptr ds:[esi]
    pop  ds
    pop  es
    jmp  s_ok
s_zero:                               ; g_b Ziel (linear), g_c Bytes: mit Nullen fuellen (BSS)
    push es
    mov  edi, g_b
    mov  ecx, g_c
    mov  ax, g_flat
    mov  es, ax
    xor  al, al
    rep  stos byte ptr es:[edi]
    pop  es
    jmp  s_ok
s_free:                               ; g_c Handle, g_d Selektoren
    mov  di, word ptr g_c
    mov  si, word ptr g_c+2
    mov  ax, 0502h
    int  31h
    mov  bx, word ptr g_d
    mov  ax, 0001h
    int  31h
    mov  bx, word ptr g_d+2
    mov  ax, 0001h
    int  31h
s_ok:
    xor  ax, ax
    jmp  s_end
s_err:
    or   ax, 8000h
s_end:
    mov  g_err, ax
    mov  ax, g_rmds
    mov  cx, ax
    mov  dx, g_rmss
    movzx ebx, g_rmsp
    movzx esi, g_rmcs
    mov  edi, offset svc_back
    jmp  fword ptr g_p2r
svc_back:
    sti
    pop  gs
    pop  fs
    pop  es
    pop  ds
    popad
    retf
svc_call_ endp

s_desc proc near                      ; BX = Selektor, AL = Zugriff ohne DPL; Basis g_b, Grenze 4 GB, 32 Bit
    mov  dx, cs
    and  dl, 3
    shl  dl, 5
    or   al, dl
    mov  cl, al
    mov  ch, 0C0h
    push bx
    mov  ax, 0009h
    int  31h
    pop  bx
    jc   sd_x
    push bx
    mov  cx, word ptr g_b+2
    mov  dx, word ptr g_b
    mov  ax, 0007h
    int  31h
    pop  bx
    jc   sd_x
    mov  cx, 0FFFFh
    mov  dx, 0FFFFh
    mov  ax, 0008h
    int  31h
sd_x:
    ret
s_desc endp

; ---- API-Aufruf aus dem 32-Bit-Code: DS = TB, SS:SP = Stapel des Tasks -------------------------------------
rm_api_ proc far
    sti
    cld
    push ds
    mov  cx, ds:[TB_NW]
    mov  si, TB_ARGS
ra_l:
    jcxz ra_go
    lodsw
    push ax
    dec  cx
    jmp  ra_l
ra_go:
    mov  si, ds:[TB_NR]
    shl  si, 2
    les  bx, ds:[TB_API]
    call dword ptr es:[bx+si]         ; pascal: die Funktion nimmt die Woerter selbst vom Stapel
    pop  ds
    mov  ds:[TB_RES], ax
    mov  ds:[TB_RES+2], dx
    movzx eax, word ptr ds:[TB_DS32]  ; zurueck zum 32-Bit-Code (pm_back), Stapel TB_PMESP
    mov  ecx, eax
    mov  edx, eax
    mov  ebx, ds:[TB_PMESP]
    movzx esi, word ptr ds:[TB_CS32]
    mov  edi, ds:[TB_PMBACK]
    jmp  dword ptr ds:[TB_R2P]
rm_api_ endp

; ---- Rueckruf in den 32-Bit-Code: DS = TB, EDI = Einsprung im 32-Bit-Code -----------------------------------
rm_topm proc near
    pushad
    push es
    push fs
    push gs
    push ds
    push word ptr ds:[TB_RMSP]
    push word ptr ds:[TB_RMSS]
    mov  ds:[TB_RMSP], sp
    mov  ds:[TB_RMSS], ss
    movzx eax, word ptr ds:[TB_DS32]
    mov  ecx, eax
    mov  edx, eax
    mov  ebx, ds:[TB_PMESP]
    movzx esi, word ptr ds:[TB_CS32]
    jmp  dword ptr ds:[TB_R2P]
rm_cbret_ label far                   ; vom 32-Bit-Code: DS = TB, SS:SP = TB_RMSS:TB_RMSP
    sti
    pop  word ptr ds:[TB_RMSS]
    pop  word ptr ds:[TB_RMSP]
    pop  ds
    pop  gs
    pop  fs
    pop  es
    popad
    ret
rm_topm endp

; ---- Start: AX = TB-Segment; Rueckgabe (RdMain) in TB_CBRES -------------------------------------------------
rm_start_ proc far
    push ds
    push edi
    mov  ds, ax
    mov  edi, ds:[TB_PMSTART]
    call rm_topm
    pop  edi
    pop  ds
    retf
rm_start_ endp

; ---- Fenster- und Dialogprozeduren fuer 32-Bit-Code (je vier je Task): DWORD far pascal (HWND, WORD, WORD, DWORD) --
wp0_: mov ax, 0
      jmp short wpall
wp1_: mov ax, 1
      jmp short wpall
wp2_: mov ax, 2
      jmp short wpall
wp3_: mov ax, 3
      jmp short wpall
wp4_: mov ax, 4                       ; 4..7: Dialogprozeduren (DialogRun, Stufe 4b)
      jmp short wpall
wp5_: mov ax, 5
      jmp short wpall
wp6_: mov ax, 6
      jmp short wpall
wp7_: mov ax, 7
wpall proc far
    push bp
    mov  bp, sp
    push ds
    push es
    push bx
    push cx
    push edi
    push ax
    mov  bx, DGROUP
    mov  ds, bx
    les  bx, g_api
    call dword ptr es:[bx+2*4]        ; TaskSelf
    mov  bx, offset g_tab
    mov  cx, 8
wa_s:
    cmp  [bx], ax
    je   wa_f
    add  bx, 4
    loop wa_s
    pop  ax
    xor  ax, ax
    xor  dx, dx
    jmp  wa_x
wa_f:
    mov  ds, [bx+2]
    pop  ax
    mov  ds:[TB_CBSLOT], ax
    mov  ax, [bp+14]
    mov  ds:[TB_CBW], ax
    mov  ax, [bp+12]
    mov  ds:[TB_CBMSG], ax
    mov  ax, [bp+10]
    mov  ds:[TB_CBP1], ax
    mov  eax, [bp+6]
    mov  ds:[TB_CBP2], eax
    mov  edi, ds:[TB_PMCB]
    call rm_topm
    mov  ax, ds:[TB_CBRES]
    mov  dx, ds:[TB_CBRES+2]
wa_x:
    pop  edi
    pop  cx
    pop  bx
    pop  es
    pop  ds
    pop  bp
    retf 10
wpall endp
exnum db 0, 6, 0Ch, 0Dh, 0Eh           ; Ausnahmen: Teilen, Befehl, Stapel, Schutz, Seite
exofs dd offset ex0, offset ex1, offset ex2, offset ex3, offset ex4

extrn rd32_ausnahme_:far
rm_kill_:                             ; Realmodus nach einer Ausnahme im 32-Bit-Code: DS = DGROUP, Stapel des Tasks
    sti
    cld
    call rd32_ausnahme_               ; beendet den Task, kehrt nicht zurueck
rk_halt:
    jmp  rk_halt
RD32A_TEXT ends

; ---- 32-Bit-Code im PM (eigener Selektor, Basis = Segment): Zeitgeber und Ausnahmen -----------------------------
RD32P_TEXT segment para public use32 'CODE'
assume cs:RD32P_TEXT, ds:DGROUP
p_pds   dw 0
p_old8  dd 0
        dw 0
p_oldex db 30 dup(0)

; IRQ 0 im PM: laeuft gerade ein 32-Bit-Task in seinem eigenen Code (hinter RD32C0), alle 8 Takte (145,6 Hz: ca. 55 ms) die
; Rueckkehradresse auf pm_preempt im Bild umbiegen: der Task ruft dort TaskYield und macht danach weiter.
i8pm:
    push ds
    push eax
    push ebx
    push ecx
    push edx
    push edi
    mov  ds, cs:p_pds
    inc  g_tick
    cmp  _g_preon, 0
    je   i8_w
    test byte ptr g_tick, 7
    jnz  i8_w
    mov  edi, 24                      ; Rahmen des unterbrochenen Codes (EIP, CS, EFLAGS) hinter 6 Registern; CWSDPMI legt
i8_r:                                 ; davor einen eigenen Rahmen (EIP, CS, EFLAGS) zu seinem Rueckweg: dann 12 weiter
    mov  ax, [esp+edi+4]
    xor  ebx, ebx
i8_s:
    cmp  ax, _g_cs32[ebx*2]
    je   i8_f
    inc  ebx
    cmp  ebx, 8
    jb   i8_s
    add  edi, 12
    cmp  edi, 36
    jbe  i8_r
    jmp  i8_w
i8_f:
    mov  edx, [esp+edi]
    cmp  edx, _g_pmin[ebx*4]
    jb   i8_w
    mov  ecx, _g_pstub[ebx*4]
    jecxz i8_w
    push es
    mov  ax, g_flat
    mov  es, ax
    mov  eax, _g_tblin[ebx*4]
    mov  es:[eax+120], edx            ; TB_PREEIP
    pop  es
    mov  [esp+edi], ecx
i8_w:
    pop  edi
    pop  edx
    pop  ecx
    pop  ebx
    pop  eax
    pop  ds
    jmp  fword ptr cs:p_old8

; Ausnahme (DPMI 0203h, 32-Bit-Rahmen): im Code eines 32-Bit-Tasks -> Task beenden (ex_kill), sonst weiterreichen
ex0: push 0
     jmp  short exall
ex1: push 1
     jmp  short exall
ex2: push 2
     jmp  short exall
ex3: push 3
     jmp  short exall
ex4: push 4
exall:                                ; [esp] Index, +4 Rueckkehr zum Host (EIP, CS), +12 Fehler, +16 EIP, CS, EFLAGS, ESP, SS
    push ds
    push eax
    push ebx
    mov  ds, cs:p_pds
    mov  ax, [esp+32]
    xor  ebx, ebx
ex_s:
    cmp  ax, _g_cs32[ebx*2]
    je   ex_f
    inc  ebx
    cmp  ebx, 8
    jb   ex_s
    mov  eax, [esp+12]                ; nicht von uns: an den alten Handler (DS bleibt DGROUP)
    imul eax, 6
    mov  ebx, dword ptr cs:p_oldex[eax]
    mov  g_jo, ebx
    mov  bx, word ptr cs:p_oldex[eax+4]
    mov  g_js, bx
    pop  ebx
    pop  eax
    add  esp, 8
    jmp  fword ptr g_jo
ex_f:
    mov  eax, [esp+12]
    mov  _g_exnr, ax
    mov  eax, [esp+28]
    mov  _g_exeip, eax
    mov  ax, word ptr g_tab[ebx*4+2]
    mov  _g_extb, ax
    mov  dword ptr [esp+28], offset ex_kill
    movzx eax, g_p32cs
    mov  [esp+32], eax
    and  dword ptr [esp+36], NOT 100h
    movzx eax, g_svcsp
    mov  [esp+40], eax
    movzx eax, g_pds
    mov  [esp+44], eax
    pop  ebx
    pop  eax
    pop  ds
    add  esp, 4
    retf
ex_kill:                              ; PM, Stapel der Dienste: in den Realmodus auf den Stapel des Tasks, rm_kill
    mov  ds, cs:p_pds
    push es
    mov  ax, g_flat
    mov  es, ax
    movzx ebx, _g_extb
    shl  ebx, 4
    movzx edx, word ptr es:[ebx+2]    ; TB_RMSS
    movzx ecx, word ptr es:[ebx+0]    ; TB_RMSP
    pop  es
    mov  ebx, ecx
    mov  ax, g_rmds
    mov  cx, ax
    movzx esi, g_rmcs
    mov  edi, offset rm_kill_
    jmp  fword ptr g_p2r
RD32P_TEXT ends
end
