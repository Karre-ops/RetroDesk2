; KASM.ASM - Assemblerteile des RD2-Kerns (Modell Small, Code near)
.8086
.8087
DGROUP group _DATA, _BSS
_TEXT segment word public 'CODE'
_TEXT ends
_DATA segment word public 'DATA'
public _k_sndtick
_k_sndtick  dd 0                     ; RDSND: Klang-Takt (72,8 Hz, im Interrupt), 0 = keiner
public _rd_ticks, _rd_preempt, _rd_killreq, _k_crit, _k_critdrv, _rd_gateptr, _k_xmsent, _k_xdx
_k_xmsent   dd 0                     ; Einsprung des XMS-Treibers (HIMEM), 0 = keiner
_k_xdx      dw 0                     ; DX nach dem letzten XMS-Aufruf
_rd_ticks   dd 0
_rd_preempt dw 0
_rd_killreq dw 0
_k_crit     dw 0
_k_critdrv  dw 0
_rd_gateptr dd modgate
old24       dd 0
tdiv        dw 0
old8        dd 0
old23       dd 0
mslt        dd 0FFFFFFFFh            ; ms_now: letzter Takt
mslm        dd 0                     ; ms_now: Millisekunden dazu
_DATA ends
_BSS segment word public 'BSS'
_BSS ends

_TEXT segment word public 'CODE'
assume cs:_TEXT, ds:DGROUP
extrn _task_tick:near, _mod_enter:near, _mod_leave:near

; ---------------------------------------------------------------------------
; short rd_enter(WORD cs, WORD ip, void far *api, WORD hinst, WORD show, char far *cmd)
public _rd_enter
_rd_enter proc near
    push bp
    mov  bp, sp
    push si
    push di
    push ds
    push es
    push word ptr [bp+4]
    push word ptr [bp+6]
    mov  bx, [bp+8]
    mov  es, [bp+10]
    mov  cx, [bp+12]
    mov  dx, [bp+14]
    mov  si, [bp+16]
    mov  di, [bp+18]
    mov  ax, 5232h
    call dword ptr [bp-12]
    add  sp, 4
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  bp
    ret
_rd_enter endp

; ---------------------------------------------------------------------------
; WORD rd_xms(WORD ax, WORD dx, WORD si)   XMS-Funktion AH, DS:SI = Kern-DS; DX -> k_xdx
public _rd_xms
_rd_xms proc near
    push bp
    mov  bp, sp
    push si
    mov  ax, [bp+4]
    mov  dx, [bp+6]
    mov  si, [bp+8]
    call dword ptr _k_xmsent
    mov  _k_xdx, dx
    pop  si
    pop  bp
    ret
_rd_xms endp

; ---------------------------------------------------------------------------
; void rd_int(WORD nr, KREGS *r)   KREGS: ax bx cx dx si di ds es flags
; Selbstveraendernd: Interruptnummer wird in den INT-Befehl geschrieben.
public _rd_int
_rd_int proc near
    push bp
    mov  bp, sp
    push si
    push di
    push ds
    push es
    mov  al, [bp+4]
    mov  byte ptr cs:intno, al
    jmp  short $+2                   ; Prefetch-Puffer leeren (8088)
    mov  bx, [bp+6]
    push bx                          ; Zeiger auf KREGS merken
    push ds
    mov  ax, [bx]
    mov  cx, [bx+4]
    mov  dx, [bx+6]
    mov  si, [bx+8]
    mov  di, [bx+10]
    mov  es, [bx+14]
    push word ptr [bx+12]            ; ds
    mov  bx, [bx+2]
    pop  ds
    db   0CDh                        ; INT nn
intno db 21h
    pushf
    push ds
    push bx
    mov  bx, sp
    mov  ds, ss:[bx+6]               ; Kern-DS
    mov  bx, ss:[bx+8]               ; Zeiger
    mov  [bx], ax
    pop  word ptr [bx+2]             ; bx
    pop  word ptr [bx+12]            ; ds
    pop  word ptr [bx+16]            ; flags
    mov  [bx+4], cx
    mov  [bx+6], dx
    mov  [bx+8], si
    mov  [bx+10], di
    mov  [bx+14], es
    add  sp, 4
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  bp
    ret
_rd_int endp

; ---------------------------------------------------------------------------
; void rd_swtch(WORD *save, WORD ss, WORD sp) - Task wechseln.
; Sichert Flags und Register auf dem eigenen Stack, SS:SP nach save[0], save[1].
public _rd_swtch
_rd_swtch proc near
    push bp
    mov  bp, sp
    pushf
    push si
    push di
    push ds
    push es
    mov  bx, [bp+4]
    mov  ax, [bp+6]
    mov  dx, [bp+8]
    cli
    mov  [bx], ss
    mov  [bx+2], sp
    mov  ss, ax
    mov  sp, dx
    pop  es
    pop  ds
    pop  di
    pop  si
    popf                             ; stellt auch IF des neuen Tasks her
    pop  bp
    ret
_rd_swtch endp

; WORD rd_newstack(WORD ss, WORD sp, void (*start)(void))
; Baut auf einem neuen Stack den Rahmen, den rd_swtch erwartet; liefert neuen SP.
public _rd_newstack
_rd_newstack proc near
    push bp
    mov  bp, sp
    push es
    push di
    mov  es, [bp+4]
    mov  di, [bp+6]
    sub  di, 2
    mov  ax, [bp+8]
    mov  es:[di], ax                 ; Ruecksprung -> start
    sub  di, 2
    mov  word ptr es:[di], 0         ; bp
    sub  di, 2
    mov  word ptr es:[di], 0202h     ; Flags: IF=1
    sub  di, 2
    mov  word ptr es:[di], 0         ; si
    sub  di, 2
    mov  word ptr es:[di], 0         ; di
    sub  di, 2
    mov  es:[di], ds                 ; ds = Kern
    sub  di, 2
    mov  es:[di], ds                 ; es
    mov  ax, di
    pop  di
    pop  es
    pop  bp
    ret
_rd_newstack endp

; ---------------------------------------------------------------------------
; void rd_pmove(WORD dst, WORD src, WORD paras) - ueberlappend, in Bloecken zu 2048 Paragraphen
public _rd_pmove
_rd_pmove proc near
    push bp
    mov  bp, sp
    push si
    push di
    push ds
    push es
    mov  dx, [bp+4]                  ; dst
    mov  bx, [bp+6]                  ; src
    mov  bp, [bp+8]                  ; paras
    cmp  dx, bx
    je   pm_done
    jb   pm_fwd
    std                              ; dst > src: von hinten
pm_back:
    or   bp, bp
    jz   pm_done
    mov  ax, bp
    cmp  ax, 2048
    jbe  pm_b1
    mov  ax, 2048
pm_b1:
    sub  bp, ax
    mov  cx, bx
    add  cx, bp
    mov  ds, cx
    mov  cx, dx
    add  cx, bp
    mov  es, cx
    mov  cx, ax
    shl  cx, 1
    shl  cx, 1
    shl  cx, 1                       ; Woerter
    mov  si, cx
    shl  si, 1
    sub  si, 2
    mov  di, si
    rep  movsw
    jmp  pm_back
pm_fwd:
    cld
pm_f0:
    or   bp, bp
    jz   pm_done
    mov  ax, bp
    cmp  ax, 2048
    jbe  pm_f1
    mov  ax, 2048
pm_f1:
    sub  bp, ax
    mov  ds, bx
    mov  es, dx
    xor  si, si
    xor  di, di
    mov  cx, ax
    shl  cx, 1
    shl  cx, 1
    shl  cx, 1
    rep  movsw
    add  bx, ax
    add  dx, ax
    jmp  pm_f0
pm_done:
    cld
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  bp
    ret
_rd_pmove endp

; void rd_pfill(WORD seg, WORD paras, WORD wort)
public _rd_pfill
_rd_pfill proc near
    push bp
    mov  bp, sp
    push di
    push es
    mov  dx, [bp+4]
    mov  bx, [bp+6]
    mov  ax, [bp+8]
    cld
pf0:
    or   bx, bx
    jz   pf9
    mov  cx, bx
    cmp  cx, 2048
    jbe  pf1
    mov  cx, 2048
pf1:
    sub  bx, cx
    mov  es, dx
    add  dx, cx
    shl  cx, 1
    shl  cx, 1
    shl  cx, 1
    xor  di, di
    rep  stosw
    jmp  pf0
pf9:
    pop  es
    pop  di
    pop  bp
    ret
_rd_pfill endp

; ---------------------------------------------------------------------------
; Zeitgeber: PIT auf 72,8 Hz (Teiler 16384), alter INT 8 weiter mit 18,2 Hz.
public _rd_timer_on
_rd_timer_on proc near
    push bp
    mov  bp, sp
    push es
    mov  ax, 3508h
    int  21h
    mov  word ptr old8, bx
    mov  word ptr old8+2, es
    mov  ax, 3523h
    int  21h
    mov  word ptr old23, bx
    mov  word ptr old23+2, es
    mov  ax, 3524h
    int  21h
    mov  word ptr old24, bx
    mov  word ptr old24+2, es
    mov  ax, 3509h
    int  21h
    mov  word ptr cs:old9, bx
    mov  word ptr cs:old9+2, es
    push ds
    push cs
    pop  ds
    mov  dx, offset int23
    mov  ax, 2523h                   ; Strg+C waehrend RD2 laeuft: ignorieren
    int  21h
    mov  dx, offset int24
    mov  ax, 2524h                   ; kritische Fehler: immer "Fail", der Kern fragt selbst
    int  21h
    mov  dx, offset int9
    mov  ax, 2509h
    int  21h
    mov  dx, offset int8
    mov  ax, 2508h
    cli
    int  21h
    pop  ds
    mov  al, 36h
    out  43h, al
    mov  ax, [bp+4]
    out  40h, al
    mov  al, ah
    out  40h, al
    sti
    pop  es
    pop  bp
    ret
_rd_timer_on endp

public _rd_timer_off
_rd_timer_off proc near
    cli
    mov  al, 36h
    out  43h, al
    xor  al, al
    out  40h, al
    out  40h, al
    push ds
    lds  dx, old8
    mov  ax, 2508h
    int  21h
    pop  ds
    push ds
    lds  dx, old23
    mov  ax, 2523h
    int  21h
    pop  ds
    push ds
    lds  dx, old24
    mov  ax, 2524h
    int  21h
    pop  ds
    push ds
    lds  dx, cs:old9
    mov  ax, 2509h
    int  21h
    pop  ds
    sti
    ret
_rd_timer_off endp

old9 dd 0

; INT 24h: Fehlercode (DI) + 1 nach k_crit, AX nach k_critdrv, Antwort "Fail"
int24:
    push ds
    push bx
    mov  bx, DGROUP
    mov  ds, bx
    mov  bx, di
    xor  bh, bh
    inc  bx
    mov  _k_crit, bx
    mov  _k_critdrv, ax
    pop  bx
    pop  ds
    mov  al, 3
    iret

; INT 9: Strg+Alt+Rueckschritt merken, dann alter Tastaturhandler
int9:
    push ax
    in   al, 60h
    cmp  al, 0Eh
    jne  i9_old
    push ds
    mov  ax, 40h
    mov  ds, ax
    mov  al, ds:[17h]
    and  al, 0Ch
    cmp  al, 0Ch
    jne  i9_no
    mov  ax, DGROUP
    mov  ds, ax
    mov  _rd_killreq, 1
i9_no:
    pop  ds
i9_old:
    pop  ax
    jmp  dword ptr cs:old9

; ---------------------------------------------------------------------------
; Modul-Sprungstueck: AX = Nummer. Stapel: [IP][CS][Parameter...] (pascal)
modgate:
    push ds
    push ax
    mov  ax, DGROUP
    mov  ds, ax
    pop  ax
    pop  cx                          ; DS des Aufrufers
    pop  bx                          ; Ruecksprung IP
    pop  dx                          ; Ruecksprung CS -> jetzt liegen oben die Parameter
    push cx
    push dx
    push bx
    push ax
    call _mod_enter                  ; DX:AX = Ziel (laedt evtl. nach)
    add  sp, 6
    pop  cx
    push cs
    mov  bx, offset modback
    push bx
    push dx
    push ax
    mov  ds, cx
    retf                             ; "Sprung" ins Modul, Ruecksprung nach modback
modback:
    push dx
    push ax
    push ds
    mov  cx, DGROUP
    mov  ds, cx
    call _mod_leave                  ; DX:AX = Ruecksprung zum Aufrufer
    mov  bx, ax
    mov  cx, dx
    pop  ds
    pop  ax
    pop  dx
    push cx
    push bx
    retf

int23:
    iret

int8:
    push ax
    push ds
    mov  ax, DGROUP
    mov  ds, ax
    add  word ptr _rd_ticks, 1
    adc  word ptr _rd_ticks+2, 0
    inc  tdiv
    test tdiv, 3
    jnz  i8_eoi
    pushf
    call dword ptr old8              ; BIOS-Uhr, Diskettenmotor, EOI
    jmp  short i8_chk
i8_eoi:
    mov  al, 20h
    out  20h, al
i8_chk:
    cmp  word ptr _k_sndtick+2, 0    ; Klang: Toene beenden, Folgen weiterschalten (kurz, kein DOS)
    je   i8_pre
    push bx
    push cx
    push dx
    push si
    push di
    push es
    cld
    call dword ptr _k_sndtick
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
i8_pre:
    cmp  _rd_preempt, 0
    je   i8_done
    push bp
    mov  bp, sp
    mov  ax, [bp+8]                  ; CS des unterbrochenen Codes (bp, ds, ax, ip, cs, flags)
    pop  bp
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ax
    cld                              ; C erwartet DF = 0 (unterbrochener Code kann STD haben)
    call _task_tick                  ; schaltet evtl. um und kehrt spaeter hierher zurueck
    add  sp, 2
    pop  es
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
i8_done:
    pop  ds
    pop  ax
    iret

; ---------------------------------------------------------------------------
; DWORD ms_now(void): Millisekunden = (t*879)>>6 = t*13 + (t*47)>>6 (t = Takte zu 13,7 ms, 48 Bit gerechnet), Ergebnis bis zum naechsten
; Takt gemerkt. Mit gesperrten Interrupts (M9): rd_ticks sind zwei Woerter, INT 8 ruft ms_now ebenfalls.
public _ms_now
_ms_now proc near
    pushf
    cli
    mov  ax, word ptr _rd_ticks
    mov  dx, word ptr _rd_ticks+2
    cmp  ax, word ptr mslt
    jne  msn_neu
    cmp  dx, word ptr mslt+2
    je   msn_alt
msn_neu:
    mov  word ptr mslt, ax
    mov  word ptr mslt+2, dx
    mov  bx, 879                     ; 13*64 + 47: t*879 als 48 Bit, dann >> 6
    mul  bx
    mov  word ptr mslm, ax
    mov  cx, dx
    mov  ax, word ptr mslt+2
    mul  bx
    add  ax, cx
    adc  dx, 0                       ; dx:ax:mslm
    mov  cx, 6
msn_s:
    shr  dx, 1
    rcr  ax, 1
    rcr  word ptr mslm, 1
    loop msn_s
    mov  word ptr mslm+2, ax
msn_alt:
    mov  ax, word ptr mslm
    mov  dx, word ptr mslm+2
    popf
    ret
_ms_now endp

; ---------------------------------------------------------------------------
public _rd_idle
_rd_idle proc near
    sti
    hlt
    ret
_rd_idle endp

; WORD rd_cpudet(void): CPU_8088 (8088/8086), CPU_V20 (V20/V30/186), CPU_286, CPU_386, CPU_486, CPU_586.
; 8086-Klasse: Flags-Bits 12-15 immer 1; 286 im Real Mode: immer 0. V20/186 maskieren den
; Schiebezaehler auf 5 Bit. 486: AC-Flag (Bit 18) setzbar, 586: ID-Flag (Bit 21, CPUID).
; Die 32-Bit-Befehle stehen als Bytes da, der Kern bleibt fuer 8086 uebersetzt.
public _rd_cpudet
_rd_cpudet proc near
    pushf
    pushf
    pop  ax
    and  ax, 0FFFh
    push ax
    popf
    pushf
    pop  ax
    and  ax, 0F000h
    cmp  ax, 0F000h
    jne  cd_286
    mov  al, 1
    mov  cl, 33
    shl  al, cl
    mov  ax, 0
    jz   cd_ende
    mov  ax, 2
    jmp  short cd_ende
cd_286:
    pushf
    pop  ax
    or   ax, 7000h
    push ax
    popf
    pushf
    pop  ax
    and  ax, 7000h
    mov  ax, 4
    jz   cd_ende
    db 66h, 9Ch                     ; pushfd
    db 66h, 58h                     ; pop eax
    db 66h, 8Bh, 0C8h               ; mov ecx, eax
    db 66h, 35h, 0, 0, 4, 0         ; xor eax, 40000h (AC)
    db 66h, 50h                     ; push eax
    db 66h, 9Dh                     ; popfd
    db 66h, 9Ch                     ; pushfd
    db 66h, 58h                     ; pop eax
    db 66h, 33h, 0C1h               ; xor eax, ecx
    db 66h, 51h                     ; push ecx
    db 66h, 9Dh                     ; popfd
    db 66h, 0C1h, 0E8h, 10h         ; shr eax, 16
    test al, 4
    mov  ax, 5
    jz   cd_ende
    db 66h, 9Ch                     ; pushfd
    db 66h, 58h                     ; pop eax
    db 66h, 8Bh, 0C8h               ; mov ecx, eax
    db 66h, 35h, 0, 0, 20h, 0       ; xor eax, 200000h (ID)
    db 66h, 50h
    db 66h, 9Dh
    db 66h, 9Ch
    db 66h, 58h
    db 66h, 33h, 0C1h
    db 66h, 51h
    db 66h, 9Dh
    db 66h, 0C1h, 0E8h, 10h
    test al, 20h
    mov  ax, 6
    jz   cd_ende
    mov  ax, 7
cd_ende:
    popf
    ret
_rd_cpudet endp

public _rd_fpudet
_rd_fpudet proc near
    push bp
    mov  bp, sp
    sub  sp, 2
    mov  word ptr [bp-2], 0FFFFh
    fninit
    fnstcw [bp-2]
    mov  ax, [bp-2]
    and  ax, 0F3Fh
    cmp  ax, 033Fh
    mov  ax, 0
    jne  fd_no
    inc  ax
fd_no:
    mov  sp, bp
    pop  bp
    ret
_rd_fpudet endp

; WORD rd_absread(WORD drive, void *buf): Bootsektor per INT 25h lesen (0 = A:).
; Geht ueber den DOS-Treiber, der dabei den Diskettenwechsel prueft (6900h nutzt
; bei manchen DOS-Versionen alte Daten). 0 = ok, sonst DOS-Fehlercode.
public _rd_absread
_rd_absread proc near
    push bp
    mov  bp, sp
    push si
    push di
    mov  al, [bp+4]
    mov  bx, [bp+6]
    mov  cx, 1
    xor  dx, dx
    push bp
    int  25h                         ; laesst die Flags auf dem Stack
    pop  dx                          ; Flags weg (aendert CF nicht)
    pop  bp
    jc   absf
    xor  ax, ax
    jmp  short absx
absf:
    or   ax, ax
    jnz  absx
    mov  ax, 0FFh
absx:
    pop  di
    pop  si
    pop  bp
    ret
_rd_absread endp

public _rd_fsave
_rd_fsave proc near
    push bp
    mov  bp, sp
    mov  es, [bp+4]                  ; Segment, Offset: Ablage im Kern-Speicherblock
    mov  bx, [bp+6]
    fsave es:[bx]                    ; mit WAIT: 8087 muss fertig sein
    fwait
    pop  bp
    ret
_rd_fsave endp

public _rd_frstor
_rd_frstor proc near
    push bp
    mov  bp, sp
    mov  es, [bp+4]
    mov  bx, [bp+6]
    frstor es:[bx]
    fwait
    pop  bp
    ret
_rd_frstor endp

_TEXT ends
end
