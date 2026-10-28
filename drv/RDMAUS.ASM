; RDMAUS.COM - residenter Maustreiber fuer RD2 und DOS-Programme (8086, INT 33h).
; Aufruf: RDMAUS          Maus suchen: PS/2, dann seriell an COM1..COM4 (Microsoft/Logitech meldet sich mit 'M')
;         RDMAUS /P       PS/2-Maus (ueber das BIOS, INT 15h/C2h)
;         RDMAUS /S1../S4 serielle Microsoft- oder Logitech-Maus an COMn (1200 Baud 7N1)
;         RDMAUS /M1../M4 serielle Mouse-Systems-Maus an COMn (1200 Baud 8N1, 5 Bytes)
; Auch als INSTALL= in CONFIG.SYS. Ist schon ein Maustreiber geladen, bleibt RDMAUS draussen.
; Stufe 1 (28.10.2026): PS/2 und seriell, INT 33h 00-0F, 14-17, 1A-1E, 21, 24; Textmodus-Zeiger.
; Noch offen (Stufe 2): Busmaus und InPort, Zeiger in Grafikmodi fuer DOS-Programme (RD2 zeichnet selbst).
; Erkennung: INT 33h AX=0024h liefert BX=0100h, CH=Art (1 Bus, 2 seriell, 3 InPort, 4 PS/2), CL=IRQ;
; AX=004Dh liefert ES:DI -> "RDMS".
.8086
_TEXT segment byte public 'CODE'
assume cs:_TEXT, ds:_TEXT
org 100h
start:
    jmp  install

sig     db 'RDMS'
art     db 0                    ; 1 Bus, 2 seriell MS/Logitech, 3 InPort, 4 PS/2, 5 seriell Mouse Systems
irq     db 0
tasten  dw 2                    ; Zahl der Tasten fuer Funktion 0
port    dw 0                    ; Basis der seriellen Schnittstelle
alt33   dd 0
altirq  dd 0
; --- Zustand (Funktionen 15h-17h sichern ab hier bis zst_ende) ---
zst     label byte
posx    dw 0
posy    dw 0
minx    dw 0
maxx    dw 639
miny    dw 0
maxy    dw 199
restx   dw 0
resty   dw 0
ratx    dw 8                    ; Mickeys je 8 Pixel
raty    dw 16
mickx   dw 0
micky   dw 0
knopf   db 0                    ; Bit 0 links, 1 rechts, 2 Mitte
zeig    db 0FFh                 ; Zaehler: 0 = sichtbar
gedr    dw 3 dup (0)            ; Druecken je Taste: Zahl
gedrx   dw 3 dup (0)
gedry   dw 3 dup (0)
losg    dw 3 dup (0)            ; Loslassen
losgx   dw 3 dup (0)
losgy   dw 3 dup (0)
hmaske  dw 0                    ; Handler (Funktion 0Ch)
handler dd 0
smaske  dw 77FFh                ; Textzeiger
cmaske  dw 7700h
empf    dw 50, 50, 50           ; Funktion 1Ah
zst_ende label byte
; --- Laufzeit ---
gemalt  db 0                    ; Textzeiger steht auf dem Schirm
zalt    dw 0                    ; gerettete Zelle
zofs    dw 0
zseg    dw 0
imev    db 0
pkt     db 5 dup (0)            ; serielles Paket
pn      db 0
mitte   db 0                    ; Logitech: mittlere Taste

; ---------------------------------------------------------------------- Zeiger (Textmodus)
weg proc near                   ; Zeiger vom Schirm nehmen
    cmp  gemalt, 0
    je   w9
    push es
    push di
    mov  es, zseg
    mov  di, zofs
    mov  ax, zalt
    mov  es:[di], ax
    mov  gemalt, 0
    pop  di
    pop  es
w9: ret
weg endp

malen proc near                 ; Zeiger zeichnen, wenn sichtbar und Textmodus
    cmp  zeig, 0
    jne  m9
    push es
    push di
    push bx
    push cx
    push dx
    mov  ax, 40h
    mov  es, ax
    mov  bl, es:[49h]           ; Videomodus
    mov  cx, 0B800h
    cmp  bl, 7
    jne  m1
    mov  cx, 0B000h
    jmp  short m2
m1: cmp  bl, 3
    ja   m8                     ; Grafik: noch kein Zeiger (Stufe 2)
m2: mov  zseg, cx
    mov  ax, posy
    mov  cl, 3
    shr  ax, cl                 ; Zeile
    mul  word ptr es:[4Ah]      ; Spalten je Zeile
    mov  di, ax
    mov  ax, posx
    mov  cl, 3
    cmp  word ptr es:[4Ah], 40
    ja   m3
    inc  cl                     ; 40 Spalten: 16 Punkte je Zeichen
m3: shr  ax, cl
    add  di, ax
    shl  di, 1
    add  di, es:[4Eh]           ; Seitenanfang
    mov  zofs, di
    mov  es, zseg
    mov  ax, es:[di]
    mov  zalt, ax
    and  ax, smaske
    xor  ax, cmaske
    mov  es:[di], ax
    mov  gemalt, 1
m8: pop  dx
    pop  cx
    pop  bx
    pop  di
    pop  es
m9: ret
malen endp

; ---------------------------------------------------------------------- Ereignis
; AX = Mickeys x, BX = Mickeys y (unten positiv), CL = Tasten. DS = CS.
klemm proc near                 ; AX auf [SI]..[SI+2] begrenzen
    cmp  ax, [si]
    jge  k1
    mov  ax, [si]
k1: cmp  ax, [si+2]
    jle  k2
    mov  ax, [si+2]
k2: ret
klemm endp

ereignis proc near
    push cx
    xor  dx, dx                 ; DX = Ereignismaske
    mov  di, ax
    or   di, bx
    jz   e5
    or   dl, 1
    add  mickx, ax
    add  micky, bx
    push bx
    mov  cl, 3
    sal  ax, cl
    add  ax, restx
    push dx
    cwd
    idiv ratx
    mov  restx, dx
    pop  dx
    add  ax, posx
    mov  si, offset minx
    call klemm
    mov  posx, ax
    pop  ax
    mov  cl, 3
    sal  ax, cl
    add  ax, resty
    push dx
    cwd
    idiv raty
    mov  resty, dx
    pop  dx
    add  ax, posy
    mov  si, offset miny
    call klemm
    mov  posy, ax
e5: pop  cx
    mov  ch, cl
    xor  ch, knopf              ; geaenderte Tasten
    mov  knopf, cl
    xor  bx, bx                 ; Taste 0..2
    mov  ax, 2                  ; Bit fuer "gedrueckt"
e6: test ch, 1
    jz   e8
    test cl, 1
    jz   e7
    or   dx, ax
    inc  gedr[bx]
    push posx
    pop  gedrx[bx]
    push posy
    pop  gedry[bx]
    jmp  short e8
e7: shl  ax, 1
    or   dx, ax
    shr  ax, 1
    inc  losg[bx]
    push posx
    pop  losgx[bx]
    push posy
    pop  losgy[bx]
e8: shr  ch, 1
    shr  cl, 1
    shl  ax, 1
    shl  ax, 1
    add  bx, 2
    cmp  bx, 6
    jb   e6
    test dl, 1
    jz   e9
    call weg
    call malen
e9: and  dx, hmaske
    jz   e10
    cmp  imev, 0
    jne  e10
    mov  ax, word ptr handler
    or   ax, word ptr handler+2
    jz   e10
    mov  imev, 1
    mov  ax, dx
    xor  bh, bh
    mov  bl, knopf
    mov  cx, posx
    mov  dx, posy
    mov  si, mickx
    mov  di, micky
    call dword ptr handler
    push cs
    pop  ds
    mov  imev, 0
e10: ret
ereignis endp

eintritt macro
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    push cs
    pop  ds
    cld
endm
austritt macro
    pop  es
    pop  ds
    pop  bp
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
endm

; ---------------------------------------------------------------------- PS/2 (BIOS ruft weit: Status, X, Y, 0)
ps2cb proc far
    eintritt
    mov  bp, sp
    mov  dx, [bp+18+4+6]        ; Status (nach 9 Registern, Ruecksprung; Reihenfolge Z, Y, X, Status)
    mov  al, [bp+18+4+4]        ; X
    mov  bl, [bp+18+4+2]        ; Y
    test dl, 0C0h               ; Ueberlauf: verwerfen
    jnz  p9
    xor  ah, ah
    test dl, 10h
    jz   p1
    mov  ah, 0FFh
p1: xor  bh, bh
    test dl, 20h
    jz   p2
    mov  bh, 0FFh
p2: neg  bx                     ; PS/2: oben positiv
    mov  cl, dl
    and  cl, 7                  ; Bit 0 links, 1 rechts, 2 Mitte
    call ereignis
p9: austritt
    ret
ps2cb endp

; ---------------------------------------------------------------------- seriell
serisr proc far
    eintritt
s0: mov  dx, port
    add  dx, 5
    in   al, dx
    test al, 1
    jz   s9
    sub  dx, 5
    in   al, dx
    call serbyte
    jmp  s0
s9: mov  al, 20h
    out  20h, al
    austritt
    iret
serisr endp

serbyte proc near                 ; AL = empfangenes Byte
    cmp  art, 5
    je   ms
    test al, 40h                ; Microsoft: Kopfbyte hat Bit 6
    jz   sb1
    mov  pkt, al
    mov  pn, 1
    ret
sb1: mov  bl, pn
    or   bl, bl
    jnz  sb2
    mov  cl, al                 ; Logitech: viertes Byte, Bit 5 = Mitte
    and  cl, 20h
    shr  cl, 1
    shr  cl, 1
    shr  cl, 1
    mov  mitte, cl
    mov  tasten, 3
    xor  ax, ax
    xor  bx, bx
    mov  cl, knopf
    and  cl, 3
    or   cl, mitte
    jmp  ereignis
sb2: xor  bh, bh
    mov  pkt[bx], al
    inc  pn
    cmp  pn, 3
    jb   sb9
    mov  pn, 0
    mov  ah, pkt
    mov  al, ah
    mov  cl, 6
    shl  al, cl
    or   al, pkt+1
    cbw
    push ax
    mov  al, ah
    and  al, 0Ch
    mov  cl, 4
    shl  al, cl
    or   al, pkt+2
    cbw
    mov  bx, ax
    pop  ax
    mov  cl, pkt
    shr  cl, 1
    shr  cl, 1
    shr  cl, 1
    shr  cl, 1
    mov  ch, cl
    and  ch, 1                  ; Bit 4 = rechts
    shl  ch, 1
    shr  cl, 1
    and  cl, 1                  ; Bit 5 = links
    or   cl, ch
    or   cl, mitte
    jmp  ereignis
sb9: ret
ms: mov  ah, al                 ; Mouse Systems: 10000LMR (0 = gedrueckt), dx1, dy1, dx2, dy2
    and  ah, 0F8h
    cmp  ah, 80h
    jne  ms1
    mov  pkt, al
    mov  pn, 1
    ret
ms1: mov  bl, pn
    or   bl, bl
    jz   sb9
    xor  bh, bh
    mov  pkt[bx], al
    inc  pn
    cmp  bl, 2
    je   ms2
    cmp  bl, 4
    jne  sb9
    mov  pn, 0
ms2: mov  al, pkt[bx-1]
    cbw
    push ax
    mov  al, pkt[bx]
    cbw
    neg  ax
    mov  bx, ax
    pop  ax
    mov  cl, pkt
    not  cl
    mov  ch, cl
    shr  ch, 1
    shr  ch, 1
    and  ch, 1                  ; L (Bit 2) -> Bit 0
    test cl, 1
    jz   ms3
    or   ch, 2                  ; R (Bit 0) -> Bit 1
ms3: test cl, 2
    jz   ms4
    or   ch, 4                  ; M (Bit 1) -> Bit 2
ms4: mov  cl, ch
    jmp  ereignis
serbyte endp

; ---------------------------------------------------------------------- INT 33h
grund proc near                 ; Funktion 0/21h: Zustand zuruecksetzen
    call weg
    mov  zeig, 0FFh
    xor  ax, ax
    mov  minx, ax
    mov  miny, ax
    mov  restx, ax
    mov  resty, ax
    mov  mickx, ax
    mov  micky, ax
    mov  hmaske, ax
    mov  word ptr handler, ax
    mov  word ptr handler+2, ax
    push cs
    pop  es
    mov  di, offset gedr
    mov  cx, 18
    rep  stosw
    mov  ratx, 8
    mov  raty, 16
    mov  smaske, 77FFh
    mov  cmaske, 7700h
    mov  maxx, 639
    push es
    mov  ax, 40h
    mov  es, ax
    mov  al, es:[49h]
    mov  bx, 349
    cmp  al, 0Fh
    je   g1
    cmp  al, 10h
    je   g1
    mov  bx, 479
    cmp  al, 11h
    je   g1
    cmp  al, 12h
    je   g1
    mov  bx, 199
    cmp  al, 3
    ja   g1
    mov  bl, es:[84h]           ; Textzeilen - 1 (EGA/VGA), sonst 24
    or   bl, bl
    jnz  g0
    mov  bl, 24
g0: xor  bh, bh
    inc  bx
    mov  cl, 3
    shl  bx, cl
    dec  bx
    cmp  word ptr es:[4Ah], 40
    ja   g1
    mov  maxx, 639              ; 40 Spalten: weiter 0..639
g1: pop  es
    mov  maxy, bx
    mov  ax, maxx
    inc  ax
    shr  ax, 1
    mov  posx, ax
    mov  ax, maxy
    inc  ax
    shr  ax, 1
    mov  posy, ax
    ret
grund endp

int33 proc far
    cmp  ax, 24h
    jbe  i0
    cmp  ax, 4Dh
    je   i4d
    iret
i0: eintritt
    mov  bp, sp                 ; Rueckgabe in die geretteten Register: [bp+16]=AX [bp+14]=BX [bp+12]=CX [bp+10]=DX
    mov  si, ax
    shl  si, 1
    call word ptr tab33[si]
    austritt
    iret
i4d: push cs
    pop  es
    mov  di, offset sig
    iret
int33 endp

tab33 dw f00, f01, f02, f03, f04, f05, f06, f07, f08, fnix, f0a, f0b, f0c, fnix, fnix, f0f
      dw fnix, fnix, fnix, fnix, f14, f15, f16, f17, fnix, fnix, f1a, f1b, fnix, f1d, f1e, fnix
      dw fnix, f21, fnix, fnix, f24

r_ax equ word ptr [bp+16]
r_bx equ word ptr [bp+14]
r_cx equ word ptr [bp+12]
r_dx equ word ptr [bp+10]
r_es equ word ptr [bp+0]

fnix: ret
f00: cli
    call grund
    sti
    mov  r_ax, 0FFFFh
    mov  ax, tasten
    mov  r_bx, ax
    ret
f21: cli
    call grund
    sti
    mov  r_ax, 0FFFFh
    mov  ax, tasten
    mov  r_bx, ax
    ret
f01: cli
    cmp  zeig, 0
    je   f01e
    inc  zeig
    call malen
f01e: sti
    ret
f02: cli
    call weg
    dec  zeig
    sti
    ret
f03: xor  ah, ah
    mov  al, knopf
    mov  r_bx, ax
    mov  ax, posx
    mov  r_cx, ax
    mov  ax, posy
    mov  r_dx, ax
    ret
f04: cli
    mov  ax, r_cx
    mov  si, offset minx
    call klemm
    mov  posx, ax
    mov  ax, r_dx
    mov  si, offset miny
    call klemm
    mov  posy, ax
    call weg
    call malen
    sti
    ret
f05: mov  si, offset gedr
    jmp  short f056
f06: mov  si, offset losg
f056: mov  bx, r_bx
    cmp  bx, 2
    ja   f05e
    shl  bx, 1
    add  si, bx
    cli
    mov  ax, [si]
    mov  r_bx, ax
    mov  word ptr [si], 0
    mov  ax, [si+6]
    mov  r_cx, ax
    mov  ax, [si+12]
    mov  r_dx, ax
    sti
f05e: xor  ah, ah
    mov  al, knopf
    mov  r_ax, ax
    ret
f07: mov  si, offset minx
    jmp  short f078
f08: mov  si, offset miny
f078: mov  ax, r_cx
    mov  bx, r_dx
    cmp  ax, bx
    jle  f07a
    xchg ax, bx
f07a: cli
    mov  [si], ax
    mov  [si+2], bx
    mov  ax, [si-4]             ; posx/posy liegen 4 Bytes vor minx/miny
    cmp  si, offset miny
    jne  f07b
    mov  ax, posy
    call klemm
    mov  posy, ax
    jmp  short f07c
f07b: mov  ax, posx
    call klemm
    mov  posx, ax
f07c: call weg
    call malen
    sti
    ret
f0a: cmp  r_bx, 0
    jne  f0ae
    cli
    call weg
    mov  ax, r_cx
    mov  smaske, ax
    mov  ax, r_dx
    mov  cmaske, ax
    call malen
    sti
f0ae: ret
f0b: cli
    xor  ax, ax
    xchg ax, mickx
    mov  r_cx, ax
    xor  ax, ax
    xchg ax, micky
    mov  r_dx, ax
    sti
    ret
f0c: cli
    mov  ax, r_cx
    mov  hmaske, ax
    mov  ax, r_dx
    mov  word ptr handler, ax
    mov  ax, r_es
    mov  word ptr handler+2, ax
    sti
    ret
f0f: mov  ax, r_cx
    or   ax, ax
    jz   f0f1
    mov  ratx, ax
f0f1: mov  ax, r_dx
    or   ax, ax
    jz   f0f2
    mov  raty, ax
f0f2: ret
f14: cli
    mov  ax, r_cx
    xchg ax, hmaske
    mov  r_cx, ax
    mov  ax, r_dx
    xchg ax, word ptr handler
    mov  r_dx, ax
    mov  ax, r_es
    xchg ax, word ptr handler+2
    mov  r_es, ax
    sti
    ret
f15: mov  r_bx, offset zst_ende - offset zst
    ret
f16: mov  es, r_es              ; ES:DX <- Zustand
    mov  di, r_dx
    mov  si, offset zst
    mov  cx, offset zst_ende - offset zst
    rep  movsb
    ret
f17: push ds                    ; Zustand <- ES:DX
    pop  es
    mov  di, offset zst
    mov  si, r_dx
    mov  ds, r_es
    mov  cx, offset zst_ende - offset zst
    cli
    rep  movsb
    push cs
    pop  ds
    call weg
    call malen
    sti
    ret
f1a: mov  ax, r_bx
    mov  empf, ax
    mov  ax, r_cx
    mov  empf+2, ax
    mov  ax, r_dx
    mov  empf+4, ax
    ret
f1b: mov  ax, empf
    mov  r_bx, ax
    mov  ax, empf+2
    mov  r_cx, ax
    mov  ax, empf+4
    mov  r_dx, ax
    ret
f1d: ret
f1e: mov  r_bx, 0
    ret
f24: mov  r_bx, 0100h
    mov  ah, art
    cmp  ah, 5
    jne  f24a
    mov  ah, 2
f24a: mov  al, irq
    mov  r_cx, ax
    ret

ende_res label byte

; ====================================================================== Installation (wird freigegeben)
warte proc near                 ; CX Takte (55 ms) warten
    push es
    mov  ax, 40h
    mov  es, ax
w1: mov  ax, es:[6Ch]
w2: cmp  ax, es:[6Ch]
    je   w2
    loop w1
    pop  es
    ret
warte endp

serinit proc near               ; BX = COM-Nummer 0..3, AH = Rahmen (02h 7N1, 03h 8N1). CF = keine Schnittstelle
    push es
    push ax
    mov  ax, 40h
    mov  es, ax
    shl  bx, 1
    mov  dx, es:[bx]
    shr  bx, 1
    pop  ax
    pop  es
    or   dx, dx
    jnz  si1
    stc
    ret
si1: mov  port, dx
    add  dx, 3
    mov  al, 80h
    out  dx, al                 ; Teiler 96 = 1200 Baud
    sub  dx, 3
    mov  al, 96
    out  dx, al
    inc  dx
    xor  al, al
    out  dx, al
    add  dx, 2
    mov  al, ah
    out  dx, al
    dec  dx
    dec  dx
    xor  al, al
    out  dx, al                 ; IER aus
    add  dx, 3
    out  dx, al                 ; MCR: DTR, RTS aus (Maus ohne Strom)
    clc
    ret
serinit endp

; seriell pruefen: Maus meldet sich nach dem Einschalten von DTR/RTS mit 'M' (Logitech 'M3'). CF = nichts
serprobe proc near
    mov  ah, 2
    call serinit
    jc   sp9
    mov  cx, 2
    call warte
    mov  dx, port
    in   al, dx                 ; Puffer leeren
    in   al, dx
    add  dx, 4
    mov  al, 0Bh                ; DTR, RTS, OUT2
    out  dx, al
    mov  cx, 6                  ; ca. 300 ms
    push es
    mov  ax, 40h
    mov  es, ax
    mov  bx, es:[6Ch]
    pop  es
sp1: mov  dx, port
    add  dx, 5
    in   al, dx
    test al, 1
    jz   sp2
    sub  dx, 5
    in   al, dx
    cmp  al, '3'
    jne  sp3
    mov  tasten, 3
sp3: cmp  al, 'M'
    jne  sp1
    mov  ah, 1
    mov  cx, 2                  ; Rest der Kennung abwarten ('3')
    push ax
    call warte
    mov  dx, port
    add  dx, 5
    in   al, dx
    test al, 1
    jz   sp4
    sub  dx, 5
    in   al, dx
    cmp  al, '3'
    jne  sp4
    mov  tasten, 3
sp4: pop  ax
    clc
    ret
sp2: push es
    mov  ax, 40h
    mov  es, ax
    mov  ax, es:[6Ch]
    pop  es
    sub  ax, bx
    cmp  ax, 6
    jb   sp1
sp9: stc
    ret
serprobe endp

; serielle Maus an COM (BX = 0..3) einschalten: IRQ einhaengen
seran proc near
    mov  al, 4                  ; COM1/COM3 IRQ 4, COM2/COM4 IRQ 3
    test bl, 1
    jz   sa1
    mov  al, 3
sa1: mov  irq, al
    add  al, 8
    mov  ah, 35h
    push ax
    int  21h
    mov  word ptr altirq, bx
    mov  word ptr altirq+2, es
    pop  ax
    mov  ah, 25h
    mov  dx, offset serisr
    int  21h
    mov  dx, port
    add  dx, 4
    mov  al, 0Bh
    out  dx, al
    sub  dx, 3
    mov  al, 1                  ; IER: Empfang
    out  dx, al
    sub  dx, 1
    in   al, dx
    in   al, dx
    mov  cl, irq
    mov  ah, 1
    shl  ah, cl
    not  ah
    in   al, 21h
    and  al, ah
    out  21h, al
    ret
seran endp

ps2an proc near                 ; CF = keine PS/2-Maus
    int  11h
    test al, 4
    jz   pa9
    mov  ax, 0C205h             ; Paketgroesse 3
    mov  bh, 3
    int  15h
    jc   pa9
    push cs
    pop  es
    mov  bx, offset ps2cb
    mov  ax, 0C207h
    int  15h
    jc   pa9
    mov  ax, 0C200h             ; einschalten
    mov  bh, 1
    int  15h
    jc   pa9
    mov  art, 4
    mov  irq, 0
    mov  tasten, 3
    clc
    ret
pa9: stc
    ret
ps2an endp

install:
    push cs
    pop  ds
    mov  ax, 3533h              ; schon ein Maustreiber da?
    int  21h
    mov  ax, es
    or   ax, bx
    jz   n1
    cmp  byte ptr es:[bx], 0CFh ; IRET
    je   n1
    xor  ax, ax
    int  33h
    cmp  ax, 0FFFFh
    jne  n1
    mov  dx, offset m_fremd
    mov  ah, 9
    int  21h
    mov  ax, 4C01h
    int  21h
n1: mov  si, 81h                ; Befehlszeile: /P, /Sn, /Mn
    xor  bx, bx                 ; BL = Wahl (0 auto, 'P', 'S', 'M'), BH = COM
n2: lodsb
    cmp  al, 13
    je   n5
    cmp  al, '/'
    jne  n2
    lodsb
    and  al, 0DFh
    mov  bl, al
    lodsb
    sub  al, '1'
    cmp  al, 3
    ja   n3
    mov  bh, al
    jmp  n2
n3: dec  si
    jmp  n2
n5: cmp  bl, 'P'
    je   n6
    cmp  bl, 'S'
    je   n7
    cmp  bl, 'M'
    je   n8
    call ps2an                  ; automatisch: PS/2, dann COM1..COM4
    jnc  n9
    xor  bx, bx
n5a: push bx
    call serprobe
    pop  bx
    jnc  n7a
    inc  bx
    cmp  bx, 4
    jb   n5a
    jmp  short keine
n6: call ps2an
    jnc  n9
    jmp  short keine
n7: mov  bl, bh
    xor  bh, bh
    push bx
    mov  ah, 2
    call serinit
    pop  bx
    jc   keine
n7a: mov  art, 2
    call seran
    jmp  short n9
n8: mov  bl, bh
    xor  bh, bh
    push bx
    mov  ah, 3
    call serinit
    pop  bx
    jc   keine
    mov  art, 5
    mov  tasten, 3
    call seran
n9: call grund
    mov  ax, 3533h
    int  21h
    mov  word ptr alt33, bx
    mov  word ptr alt33+2, es
    mov  dx, offset int33
    mov  ax, 2533h
    int  21h
    mov  dx, offset m_ok
    mov  ah, 9
    int  21h
    mov  bl, art
    xor  bh, bh
    shl  bx, 1
    mov  dx, m_art[bx]
    mov  ah, 9
    int  21h
    mov  es, cs:[2Ch]           ; Umgebung freigeben
    mov  ah, 49h
    int  21h
    mov  dx, offset ende_res + 15
    mov  cl, 4
    shr  dx, cl
    mov  ax, 3100h
    int  21h
keine: mov  dx, offset m_keine
    mov  ah, 9
    int  21h
    mov  ax, 4C02h
    int  21h

m_art   dw m_x, m_x, m_ser, m_x, m_ps2, m_msys
m_ok    db 'RDMAUS geladen: $'
m_ser   db 'serielle Maus (Microsoft/Logitech)',13,10,'$'
m_msys  db 'serielle Maus (Mouse Systems)',13,10,'$'
m_ps2   db 'PS/2-Maus',13,10,'$'
m_x     db '?',13,10,'$'
m_keine db 'RDMAUS: keine Maus gefunden.',13,10,'$'
m_fremd db 'Maustreiber ist schon geladen, RDMAUS bleibt draussen.',13,10,'$'
_TEXT ends
end start
