; KASM.ASM - Assemblerteile des RD2-Kerns (Modell Small, Code near)
.8086
.8087
DGROUP group _DATA, _BSS
_TEXT segment word public 'CODE'
_TEXT ends
_DATA segment word public 'DATA'
public _rd_ticks, _rd_preempt, _rd_killreq, _k_crit, _k_critdrv, _rd_gateptr
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
public _rd_idle
_rd_idle proc near
    sti
    hlt
    ret
_rd_idle endp

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

public _rd_fsave
_rd_fsave proc near
    push bp
    mov  bp, sp
    mov  bx, [bp+4]
    fnsave [bx]
    fwait
    pop  bp
    ret
_rd_fsave endp

public _rd_frstor
_rd_frstor proc near
    push bp
    mov  bp, sp
    mov  bx, [bp+4]
    frstor [bx]
    fwait
    pop  bp
    ret
_rd_frstor endp

_TEXT ends
end
