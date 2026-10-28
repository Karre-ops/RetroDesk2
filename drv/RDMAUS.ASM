; RDMAUS.COM - residenter Maustreiber fuer RD2 und DOS-Programme (8086, INT 33h).
; Aufruf: RDMAUS          Maus suchen: InPort 23Ch/238h, Logitech-Busmaus 23Ch, PS/2, seriell COM1..COM4
;         RDMAUS /P       PS/2-Maus (ueber das BIOS, INT 15h/C2h)
;         RDMAUS /B       Logitech-/Microsoft-Busmaus (8255) an 23Ch
;         RDMAUS /I1 /I2  InPort-Busmaus an 23Ch bzw. 238h
;         /Q2../Q7        IRQ der Busmaus vorgeben (sonst wird er gesucht, Rueckfall 5)
;         RDMAUS /S1../S4 serielle Microsoft- oder Logitech-Maus an COMn (1200 Baud 7N1)
;         RDMAUS /M1../M4 serielle Mouse-Systems-Maus an COMn (1200 Baud 8N1, 5 Bytes)
; Auch als INSTALL= in CONFIG.SYS. Ist schon ein Maustreiber geladen, bleibt RDMAUS draussen.
; INT 33h 00-10, 13-17, 1A-1E, 21, 24. Zeiger im Textmodus und in den Grafikmodi 4, 5, 6 (CGA direkt), 13h
; (direkt) und 0Dh-12h (EGA/VGA ueber das BIOS; INT 10h wird mitgezaehlt, damit der Zeiger nie mitten in
; einen BIOS-Aufruf zeichnet). RD2 zeichnet seinen Zeiger selbst. Hercules-Grafik: kein Treiberzeiger.
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
hotx    dw 0
hoty    dw 0
form    dw 3FFFh,1FFFh,0FFFh,07FFh,03FFh,01FFh,00FFh,007Fh,003Fh,001Fh,01FFh,10FFh,30FFh,0F87Fh,0F87Fh,0FC3Fh
        dw 0000h,4000h,6000h,7000h,7800h,7C00h,7E00h,7F00h,7F80h,7C00h,6C00h,4600h,0600h,0300h,0300h,0000h
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
alt10   dd 0
in10    db 0                    ; INT 10h laeuft gerade
nachher db 0                    ; Zeichnen verschoben (BIOS war belegt)
bport   dw 23Ch                 ; Busmaus
slave   db 0                    ; IRQ am zweiten PIC
; Grafikzeiger
gart    db 0                    ; 0 keiner, 1 CGA 4 Farben, 2 CGA 2 Farben, 3 BIOS, 4 Modus 13h
gw      dw 0
gh      dw 0
gcm     db 0
gxs     db 0
ghx     dw 0
ghy     dw 0
gbit    dw 0
gsave   db 256 dup (0)

; ---------------------------------------------------------------------- Zeiger (Textmodus)
weg proc near                   ; Zeiger vom Schirm nehmen
    cmp  gemalt, 1
    jb   w9
    ja   w5
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
w5: cmp  gart, 3                ; Grafik ueber das BIOS: nicht mitten in INT 10h
    jne  w6
    cmp  in10, 0
    jne  w9
w6: push bx
    push cx
    push dx
    push si
    push bp
    xor  bp, bp                 ; 1 = zurueckschreiben
    inc  bp
    call graster
    mov  gemalt, 0
    pop  bp
    pop  si
    pop  dx
    pop  cx
    pop  bx
    ret
weg endp

malen proc near                 ; Zeiger zeichnen, wenn sichtbar
    cmp  zeig, 0
    jne  ma9
    cmp  gemalt, 0
    jne  ma9
    push es
    mov  ax, 40h
    mov  es, ax
    mov  al, es:[49h]
    pop  es
    cmp  al, 3
    jbe  ma1
    cmp  al, 7
    je   ma1
    call gsetup
    jc   ma9
    cmp  gart, 3
    jne  ma2
    cmp  in10, 0
    je   ma2
    mov  nachher, 1             ; spaeter, wenn das BIOS frei ist
ma9: ret
ma2: push bx
    push cx
    push dx
    push si
    push bp
    mov  ax, posx
    mov  cl, gxs
    shr  ax, cl
    sub  ax, hotx
    mov  ghx, ax
    mov  ax, posy
    sub  ax, hoty
    mov  ghy, ax
    xor  bp, bp                 ; 0 = retten und zeichnen
    call graster
    mov  gemalt, 2
    pop  bp
    pop  si
    pop  dx
    pop  cx
    pop  bx
    ret
ma1: jmp  tmalen
malen endp

gsetup proc near                ; AL = Videomodus. CF = kein Grafikzeiger
    push bx
    push cx
    push di
    push es
    push cs
    pop  es
    mov  di, offset gmodi
    mov  cx, 10
    repne scasb
    jne  gs9
    sub  di, offset gmodi + 1
    mov  bx, di
    mov  al, gma[bx]
    mov  gart, al
    mov  al, gmf[bx]
    mov  gcm, al
    mov  al, gmx[bx]
    mov  gxs, al
    shl  bx, 1
    mov  ax, gmw[bx]
    mov  gw, ax
    mov  ax, gmh[bx]
    mov  gh, ax
    pop  es
    pop  di
    pop  cx
    pop  bx
    clc
    ret
gs9: mov  gart, 0
    pop  es
    pop  di
    pop  cx
    pop  bx
    stc
    ret
gsetup endp
gmodi db 4, 5, 6, 0Dh, 0Eh, 0Fh, 10h, 11h, 12h, 13h
gma   db 1, 1, 2, 3, 3, 3, 3, 3, 3, 4
gmf   db 3, 3, 1, 0Fh, 0Fh, 1, 0Fh, 1, 0Fh, 0Fh          ; 13h: XOR mit 0Fh (Farbe 255 ist in der Standardpalette schwarz)
gmx   db 1, 1, 0, 1, 0, 0, 0, 0, 0, 1
gmw   dw 320, 320, 640, 320, 640, 640, 640, 640, 640, 320
gmh   dw 200, 200, 200, 200, 200, 350, 350, 480, 480, 200

; 16x16 Raster ab ghx/ghy. BP = 0: Punkte retten, (alt AND Schirmmaske) XOR Zeigermaske setzen; BP = 1: zurueck
graster proc near
    push di
    push es
    xor  si, si                 ; Index in gsave / Bit
    xor  di, di                 ; Zeile * 2
gr1: mov  gbit, 8000h
    mov  dx, di
    shr  dx, 1
    add  dx, ghy                ; DX = y
    xor  cx, cx
gr2: push cx
    add  cx, ghx                ; CX = x
    or   dx, dx
    js   gr8
    cmp  dx, gh
    jge  gr8
    or   cx, cx
    js   gr8
    cmp  cx, gw
    jge  gr8
    or   bp, bp
    jnz  gr6
    call gget
    mov  gsave[si], al
    mov  bx, gbit
    test form[di], bx
    jnz  gr3
    xor  al, al
gr3: test form[di+32], bx
    jz   gr4
    xor  al, gcm
gr4: call gput
    jmp  short gr8
gr6: mov  al, gsave[si]
    call gput
gr8: pop  cx
    inc  si
    shr  gbit, 1
    inc  cx
    cmp  cx, 16
    jb   gr2
    add  di, 2
    cmp  di, 32
    jb   gr1
    pop  es
    pop  di
    ret
graster endp

; Punkt lesen (AL) / setzen (AL) bei CX = x, DX = y; erhaelt CX, DX
gadr proc near                  ; CGA: ES:BX = Byte, AH = Bitnummer des Punkts (von rechts)
    push cx
    push dx
    mov  ax, 0B800h
    mov  es, ax
    mov  bx, dx
    shr  bx, 1
    mov  ax, 80
    push dx
    mul  bx
    pop  dx
    mov  bx, ax
    test dl, 1
    jz   ga1
    add  bx, 2000h
ga1: mov  ax, cx
    cmp  gart, 2
    je   ga2
    shr  ax, 1                  ; 4 Farben: 4 Punkte je Byte
    shr  ax, 1
    add  bx, ax
    and  cl, 3
    mov  ah, 3
    sub  ah, cl
    shl  ah, 1
    jmp  short ga3
ga2: mov  dx, cx
    shr  ax, 1
    shr  ax, 1
    shr  ax, 1
    add  bx, ax
    and  dl, 7
    mov  ah, 7
    sub  ah, dl
ga3: pop  dx
    pop  cx
    ret
gadr endp

gget proc near
    push bx
    cmp  gart, 3
    je   gg3
    ja   gg4
    push cx
    call gadr
    mov  al, es:[bx]
    mov  cl, ah
    shr  al, cl
    and  al, gcm
    pop  cx
    pop  bx
    ret
gg3: push cx
    push dx
    mov  ah, 0Dh
    xor  bh, bh
    pushf
    call dword ptr alt10
    pop  dx
    pop  cx
    pop  bx
    ret
gg4: push dx
    mov  ax, 0A000h
    mov  es, ax
    mov  ax, 320
    mul  dx
    add  ax, cx
    mov  bx, ax
    mov  al, es:[bx]
    pop  dx
    pop  bx
    ret
gget endp

gput proc near
    push bx
    cmp  gart, 3
    je   gp3
    ja   gp4
    push cx
    push ax
    call gadr
    mov  cl, ah
    pop  ax
    and  al, gcm
    shl  al, cl
    mov  ah, gcm
    shl  ah, cl
    not  ah
    and  ah, es:[bx]
    or   al, ah
    mov  es:[bx], al
    pop  cx
    pop  bx
    ret
gp3: push cx
    push dx
    mov  ah, 0Ch
    xor  bh, bh
    pushf
    call dword ptr alt10
    pop  dx
    pop  cx
    pop  bx
    ret
gp4: push dx
    push ax
    mov  ax, 0A000h
    mov  es, ax
    mov  ax, 320
    mul  dx
    add  ax, cx
    mov  bx, ax
    pop  ax
    mov  es:[bx], al
    pop  dx
    pop  bx
    ret
gput endp

tmalen proc near                ; Textzeiger
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
    ja   m8
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
    ret
tmalen endp

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

; ---------------------------------------------------------------------- INT 10h mitzaehlen
int10 proc far
    inc  cs:in10
    or   ah, ah
    jz   v1
    pushf
    call dword ptr cs:alt10
    dec  cs:in10
    cmp  cs:nachher, 0
    jne  v2
    retf 2
v1: pushf                       ; Modus setzen: Schirm ist neu, Zeiger gilt als weg
    call dword ptr cs:alt10
    mov  cs:gemalt, 0
    dec  cs:in10
v2: push ax
    push ds
    push cs
    pop  ds
    pushf
    cli
    mov  nachher, 0
    call malen
    popf
    pop  ds
    pop  ax
    retf 2
int10 endp

; ---------------------------------------------------------------------- Busmaus
eoi proc near
    mov  al, 20h
    cmp  slave, 0
    je   eo1
    out  0A0h, al
eo1: out  20h, al
    ret
eoi endp

busisr proc far                 ; Logitech/Microsoft-Busmaus (8255): X, Y in Halbbytes, Tasten 0 = gedrueckt
    eintritt
    mov  dx, bport
    add  dx, 2
    mov  al, 90h
    out  dx, al
    sub  dx, 2
    in   al, dx
    and  al, 0Fh
    mov  bl, al
    add  dx, 2
    mov  al, 0B0h
    out  dx, al
    sub  dx, 2
    in   al, dx
    mov  cl, 4
    shl  al, cl
    or   bl, al                 ; BL = X
    add  dx, 2
    mov  al, 0D0h
    out  dx, al
    sub  dx, 2
    in   al, dx
    and  al, 0Fh
    mov  bh, al
    add  dx, 2
    mov  al, 0F0h
    out  dx, al
    sub  dx, 2
    in   al, dx
    mov  ch, al                 ; Tasten in Bit 7 links, 6 Mitte, 5 rechts
    shl  al, cl
    or   bh, al                 ; BH = Y
    add  dx, 2
    xor  al, al                 ; Halten aus, Interrupt an
    out  dx, al
    not  ch
    xor  cl, cl
    test ch, 80h
    jz   bu1
    or   cl, 1
bu1: test ch, 20h
    jz   bu2
    or   cl, 2
bu2: test ch, 40h
    jz   bu3
    or   cl, 4
bu3: mov  al, bl
    cbw
    xchg ax, bx
    mov  al, ah
    cbw
    xchg ax, bx                 ; AX = X, BX = Y (unten positiv)
    call ereignis
    call eoi
    austritt
    iret
busisr endp

inpisr proc far                 ; InPort: Register 0 Tasten, 1 X, 2 Y, 7 Modus
    eintritt
    mov  dx, bport
    mov  al, 7
    out  dx, al
    inc  dx
    mov  al, 39h                ; Halten
    out  dx, al
    dec  dx
    mov  al, 1
    out  dx, al
    inc  dx
    in   al, dx
    mov  bl, al
    dec  dx
    mov  al, 2
    out  dx, al
    inc  dx
    in   al, dx
    mov  bh, al
    dec  dx
    xor  al, al
    out  dx, al
    inc  dx
    in   al, dx
    mov  ch, al                 ; Bit 0 Mitte, 1 links, 2 rechts (1 = gedrueckt)
    dec  dx
    mov  al, 7
    out  dx, al
    inc  dx
    mov  al, 19h                ; weiter mit 30 Hz und Interrupt
    out  dx, al
    xor  cl, cl
    test ch, 2
    jz   ip1
    or   cl, 1
ip1: test ch, 4
    jz   ip2
    or   cl, 2
ip2: test ch, 1
    jz   ip3
    or   cl, 4
ip3: mov  al, bl
    cbw
    xchg ax, bx
    mov  al, ah
    cbw
    xchg ax, bx
    call ereignis
    call eoi
    austritt
    iret
inpisr endp

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
    mov  hotx, 0                ; Pfeil wie beim Microsoft-Treiber
    mov  hoty, 0
    push ds
    pop  es
    mov  si, offset pfeil
    mov  di, offset form
    mov  cx, 32
    rep  movsw
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

pfeil dw 3FFFh,1FFFh,0FFFh,07FFh,03FFh,01FFh,00FFh,007Fh,003Fh,001Fh,01FFh,10FFh,30FFh,0F87Fh,0F87Fh,0FC3Fh
      dw 0000h,4000h,6000h,7000h,7800h,7C00h,7E00h,7F00h,7F80h,7C00h,6C00h,4600h,0600h,0300h,0300h,0000h
tab33 dw f00, f01, f02, f03, f04, f05, f06, f07, f08, f09, f0a, f0b, f0c, fnix, fnix, f0f
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
f09: cli                        ; Grafikzeiger: BX, CX Brennpunkt, ES:DX 16 Worte Schirm- und 16 Zeigermaske
    call weg
    mov  ax, r_bx
    mov  hotx, ax
    mov  ax, r_cx
    mov  hoty, ax
    push ds
    pop  es
    mov  di, offset form
    mov  si, r_dx
    mov  ds, r_es
    mov  cx, 32
    rep  movsw
    push cs
    pop  ds
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

qirq    db 0                    ; vorgegebener IRQ (0 = suchen)
inp_da proc near                ; InPort an bport? Kennung: abwechselnd DEh und Version. CF = nein
    mov  dx, bport
    add  dx, 2
    in   al, dx
    mov  ah, al
    in   al, dx
    mov  bl, al
    in   al, dx
    cmp  ah, 0DEh
    jne  id1
    cmp  al, 0DEh
    jne  id8
    cmp  bl, 0DEh
    je   id8
    clc
    ret
id1: cmp  bl, 0DEh
    jne  id8
    cmp  al, 0DEh
    je   id8
    clc
    ret
id8: stc
    ret
inp_da endp

logi_da proc near               ; 8255-Busmaus: Signatur in Port B schreiben und lesen. CF = nein
    mov  dx, bport
    add  dx, 3
    mov  al, 91h
    out  dx, al
    sub  dx, 2
    mov  al, 0A5h
    out  dx, al
    in   al, dx
    cmp  al, 0A5h
    jne  ld8
    mov  al, 5Ah
    out  dx, al
    in   al, dx
    cmp  al, 5Ah
    jne  ld8
    inc  dx
    mov  al, 10h                ; Interrupt der Karte aus
    out  dx, al
    clc
    ret
ld8: stc
    ret
logi_da endp

; IRQ der Karte suchen: Karten-Interrupt an/aus schalten und im IRR des PIC mitlesen (IRQ 2..7).
; AH = Wert "an", AL = Wert "aus", DX = Port dafuer. Ergebnis AL = IRQ (Rueckfall 5)
irqsuche proc near
    mov  al, qirq
    or   al, al
    jnz  is9
    mov  word ptr ianaus, ax
    mov  iport, dx
    mov  bl, 0FCh               ; Kandidaten IRQ 2..7
    mov  si, 4
is1: mov  dx, iport
    mov  al, byte ptr ianaus+1
    out  dx, al
    mov  cx, 2
    call warte
    mov  al, 0Ah                ; OCW3: IRR lesen
    out  20h, al
    in   al, 20h
    and  bl, al
    mov  dx, iport
    mov  al, byte ptr ianaus
    out  dx, al
    mov  cx, 2
    call warte
    mov  al, 0Ah
    out  20h, al
    in   al, 20h
    not  al
    and  bl, al
    dec  si
    jnz  is1
    mov  al, 2
is2: mov  cl, al
    mov  ah, 1
    shl  ah, cl
    test bl, ah
    jnz  is9
    inc  al
    cmp  al, 8
    jb   is2
    mov  al, 5
is9: ret
irqsuche endp
ianaus  dw 0
iport   dw 0

; Busmaus-IRQ (AL) einhaengen, DX = ISR. IRQ 2 am AT kommt als IRQ 9 (INT 71h) an.
busan proc near
    mov  irq, al
    mov  bx, dx
    mov  ah, al
    add  ah, 8                  ; INT 08h + IRQ
    cmp  al, 2
    jne  ba1
    push es
    mov  ax, 0F000h
    mov  es, ax
    mov  al, es:[0FFFEh]        ; Modellbyte: FCh und kleiner = AT-Klasse mit zweitem PIC
    pop  es
    cmp  al, 0FCh
    mov  ah, 0Ah
    ja   ba1
    mov  slave, 1
    mov  ah, 71h
ba1: mov  al, ah
    push ax
    push bx
    mov  ah, 35h
    int  21h
    mov  word ptr altirq, bx
    mov  word ptr altirq+2, es
    pop  dx
    pop  ax
    mov  ah, 25h
    int  21h
    cmp  slave, 0
    je   ba2
    in   al, 0A1h
    and  al, 0FDh               ; IRQ 9 frei
    out  0A1h, al
    in   al, 21h
    and  al, 0FBh               ; Kaskade frei
    out  21h, al
    ret
ba2: mov  cl, irq
    mov  ah, 1
    shl  ah, cl
    not  ah
    in   al, 21h
    and  al, ah
    out  21h, al
    ret
busan endp

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
n1: mov  si, 81h                ; Befehlszeile: /P, /Sn, /Mn, /B, /In, /Qn
    xor  bx, bx                 ; BL = Wahl (0 auto, 'P', 'S', 'M', 'B', 'I'), BH = Nummer - 1
n2: lodsb
    cmp  al, 13
    je   n5
    cmp  al, '/'
    jne  n2
    lodsb
    and  al, 0DFh
    cmp  al, 'Q'
    je   nq
    mov  bl, al
    lodsb
    sub  al, '1'
    cmp  al, 3
    ja   n3
    mov  bh, al
    jmp  n2
n3: dec  si
    jmp  n2
nq: lodsb
    sub  al, '0'
    cmp  al, 7
    ja   n3
    mov  qirq, al
    jmp  n2
n5: cmp  bl, 'B'
    jne  n5b
    mov  bport, 23Ch
    call logi_da
    jnc  ok1
    jmp  keine
ok1:
    jmp  nbus
n5b: cmp  bl, 'I'
    jne  n5c
    mov  bport, 23Ch
    or   bh, bh
    jz   n5i
    mov  bport, 238h
n5i: call inp_da
    jnc  ok2
    jmp  keine
ok2:
    jmp  ninp
n5c: cmp  bl, 'P'
    je   n6
    cmp  bl, 'S'
    je   n7
    cmp  bl, 'M'
    je   n8
    mov  bport, 23Ch            ; automatisch: InPort 23Ch/238h, Logitech-Bus 23Ch, PS/2, COM1..COM4
    call inp_da
    jnc  ninp
    mov  bport, 238h
    call inp_da
    jnc  ninp
    mov  bport, 23Ch
    call logi_da
    jnc  nbus
    call ps2an
    jnc  n9
    xor  bx, bx
n5a: push bx
    call serprobe
    pop  bx
    jnc  n7a
    inc  bx
    cmp  bx, 4
    jb   n5a
    jmp  keine
n6: call ps2an
    jnc  n9
    jmp  keine
n7: mov  bl, bh
    xor  bh, bh
    push bx
    mov  ah, 2
    call serinit
    pop  bx
    jnc  ok3
    jmp  keine
ok3:
n7a: mov  art, 2
    call seran
    jmp  short n9
n8: mov  bl, bh
    xor  bh, bh
    push bx
    mov  ah, 3
    call serinit
    pop  bx
    jnc  ok4
    jmp  keine
ok4:
    mov  art, 5
    mov  tasten, 3
    call seran
n9: mov  ax, 3510h              ; INT 10h mitzaehlen (Grafikzeiger ueber das BIOS)
    int  21h
    mov  word ptr alt10, bx
    mov  word ptr alt10+2, es
    mov  dx, offset int10
    mov  ax, 2510h
    int  21h
    call grund
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
nbus: mov  dx, bport           ; Logitech-Bus: Steuerport C, 00h = Interrupt an, 10h = aus
    add  dx, 2
    mov  ax, 0010h
    call irqsuche
    mov  dx, offset busisr
    call busan
    mov  art, 1
    mov  tasten, 3
    mov  dx, bport
    add  dx, 2
    xor  al, al
    out  dx, al
    jmp  n9
ninp: mov  dx, bport            ; InPort: zuruecksetzen, Modusregister waehlen; Daten 19h = an, 10h = aus
    mov  al, 80h
    out  dx, al
    mov  al, 7
    out  dx, al
    inc  dx
    mov  ax, 1910h
    call irqsuche
    mov  dx, offset inpisr
    call busan
    mov  art, 3
    mov  tasten, 3
    mov  dx, bport
    mov  al, 7
    out  dx, al
    inc  dx
    mov  al, 19h
    out  dx, al
    jmp  n9
keine: mov  dx, offset m_keine
    mov  ah, 9
    int  21h
    mov  ax, 4C02h
    int  21h

m_art   dw m_x, m_bus, m_ser, m_inp, m_ps2, m_msys
m_bus   db 'Busmaus (Logitech/Microsoft)',13,10,'$'
m_inp   db 'InPort-Busmaus',13,10,'$'
m_ok    db 'RDMAUS geladen: $'
m_ser   db 'serielle Maus (Microsoft/Logitech)',13,10,'$'
m_msys  db 'serielle Maus (Mouse Systems)',13,10,'$'
m_ps2   db 'PS/2-Maus',13,10,'$'
m_x     db '?',13,10,'$'
m_keine db 'RDMAUS: keine Maus gefunden.',13,10,'$'
m_fremd db 'Maustreiber ist schon geladen, RDMAUS bleibt draussen.',13,10,'$'
_TEXT ends
end start
