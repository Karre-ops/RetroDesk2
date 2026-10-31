; RDKEYB.COM - residenter Tastaturtreiber DE/US fuer RD2 und DOS-Programme (8086).
; Aufruf: RDKEYB DE | RDKEYB US | RDKEYB datei[.KBD]   (auch als INSTALL= in CONFIG.SYS). Ist RDKEYB schon geladen,
; schaltet ein zweiter Aufruf nur die Belegung um (auch auf eine andere Datei).
; Belegungsdatei (31.10.2026, tools/mkkbd.py): 'RDKBD',1, dann 178 Bytes = yz, tab, tab56, totab (wie unten).
; Fehlt sie oder ist sie kaputt, bleibt die eingebaute DE-Belegung (Tastatur faellt nie aus). Ist ein fremder KEYB geladen, bleibt RDKEYB draussen.
; Zur Laufzeit: Strg+Alt+F1 = US, Strg+Alt+F2 = DE.
; Arbeitsweise: INT 9 ruft zuerst das BIOS, danach wird der neue Eintrag im BIOS-Tastaturpuffer ersetzt,
; entfernt (Tottaste) oder eingefuegt (Tasten, die das BIOS nicht kennt, z. B. AltGr auf dem XT, <>|).
; AltGr = rechte Alt-Taste (E0 38) oder Strg+Alt. Tottasten ^ ´ ` mit a e i o u (CP437), Leertaste = Zeichen.
; INT 2Fh AX=AD80h: AL=FFh (KEYB geladen), BX=0100h, ES:DI -> "RDKB", danach Belegung (0 US, 1 DE).
.8086
_TEXT segment byte public 'CODE'
assume cs:_TEXT, ds:_TEXT
org 100h
start:
    jmp  install

sig     db 'RDKB'
modus   db 1                    ; 0 US, 1 DE
e0      db 0                    ; letzter Code war E0
altgr   db 0                    ; rechte Alt gedrueckt
tot     db 0                    ; anstehende Tottaste 1 ^, 2 ´, 3 `
alt9    dd 0
alt15   dd 0
mit4f   db 0                    ; BIOS ruft INT 15h/4Fh: Scancode von dort, Port 60h nicht lesen
letzt   db 0FFh
alt2f   dd 0

; ab hier 178 Bytes, die eine Belegungsdatei ersetzt
yz      db 1                    ; 1: Strg/Alt+y/z tauschen (QWERTZ)
; DE-Tabelle ab Scancode 02h bis 35h: normal, Umschalt, AltGr. 0 = BIOS-Zeichen lassen, 1..3 = Tottaste
tab label byte
    db 0,0,0            ; 02 1
    db 0,'"',0FDh       ; 03 2  ²
    db 0,15h,0          ; 04 3  §
    db 0,0,0            ; 05 4
    db 0,0,0            ; 06 5
    db 0,'&',0          ; 07 6
    db 0,'/','{'        ; 08 7
    db 0,'(','['        ; 09 8
    db 0,')',']'        ; 0A 9
    db 0,'=','}'        ; 0B 0
    db 0E1h,'?','\'     ; 0C ß
    db 2,3,0            ; 0D ´ `
    db 0,0,0            ; 0E Rueckschritt
    db 0,0,0            ; 0F Tab
    db 0,0,'@'          ; 10 q
    db 0,0,0            ; 11 w
    db 0,0,0            ; 12 e
    db 0,0,0            ; 13 r
    db 0,0,0            ; 14 t
    db 'z','Z',0        ; 15 z
    db 0,0,0            ; 16 u
    db 0,0,0            ; 17 i
    db 0,0,0            ; 18 o
    db 0,0,0            ; 19 p
    db 81h,9Ah,0        ; 1A ü
    db '+','*','~'      ; 1B +
    db 0,0,0            ; 1C Eingabe
    db 0,0,0            ; 1D Strg
    db 0,0,0            ; 1E a
    db 0,0,0            ; 1F s
    db 0,0,0            ; 20 d
    db 0,0,0            ; 21 f
    db 0,0,0            ; 22 g
    db 0,0,0            ; 23 h
    db 0,0,0            ; 24 j
    db 0,0,0            ; 25 k
    db 0,0,0            ; 26 l
    db 94h,99h,0        ; 27 ö
    db 84h,8Eh,0        ; 28 ä
    db 1,0F8h,0         ; 29 ^ °
    db 0,0,0            ; 2A Umschalt
    db '#',27h,0        ; 2B #
    db 'y','Y',0        ; 2C y
    db 0,0,0            ; 2D x
    db 0,0,0            ; 2E c
    db 0,0,0            ; 2F v
    db 0,0,0            ; 30 b
    db 0,0,0            ; 31 n
    db 0,0,0E6h         ; 32 m  µ
    db 0,';',0          ; 33 ,
    db 0,':',0          ; 34 .
    db '-','_',0        ; 35 -
tab56   db '<','>','|'  ; 56 <>| (102 Tasten)
; Tottasten: je Akzent 5 Zeichen fuer a e i o u, dann das Zeichen allein
totab   db 83h,88h,8Ch,93h,96h,'^'      ; ^
        db 0A0h,82h,0A1h,0A2h,0A3h,27h  ; ´
        db 85h,8Ah,8Dh,95h,97h,'`'      ; `
KBDLEN  equ $ - yz
vokal   db 'aeiou'

int2f:
    cmp  ax, 0AD80h
    jne  i2f_alt
    push cs
    pop  es
    mov  di, offset sig
    mov  bx, 0100h
    mov  al, 0FFh
    iret
i2f_alt:
    jmp  dword ptr cs:alt2f

int15:
    cmp  ah, 4Fh
    jne  i15_alt
    mov  cs:letzt, al
i15_alt:
    jmp  dword ptr cs:alt15

int9:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    mov  dl, 0FFh
    cmp  cs:mit4f, 0
    jne  i9_0
    in   al, 60h                ; XT (8255): Lesen schadet nicht
    mov  dl, al
i9_0:
    mov  cs:letzt, 0FFh
    mov  ax, 40h
    mov  es, ax
    mov  di, es:[1Ch]           ; Ende vorher = Platz des neuen Eintrags
    pushf
    call dword ptr cs:alt9      ; BIOS verarbeitet die Taste
    push cs
    pop  ds
    cmp  mit4f, 0
    je   i9_00
    mov  dl, letzt              ; AT: Scancode, den das BIOS an INT 15h/4Fh gab
    cmp  dl, 0FFh
    jne  i9_00
    jmp  fertig
i9_00:
    mov  al, e0
    mov  e0, 0
    cmp  dl, 0E0h
    jne  i9_a
    mov  e0, 1
    jmp  fertig
i9_a:
    or   al, al
    jz   i9_b
    mov  al, dl                 ; E0 xx: nur rechte Alt merken
    and  al, 7Fh
    cmp  al, 38h
    jne  fertig_j
    mov  altgr, 1
    test dl, 80h
    jz   fertig_j
    mov  altgr, 0
fertig_j:
    jmp  fertig
i9_b:
    test dl, 80h
    jnz  fertig_j               ; Loslassen: nichts
    mov  bl, es:[17h]           ; bl = Umschalt-Status
    mov  bh, bl
    and  bh, 0Ch
    cmp  bh, 0Ch                ; Strg+Alt+F1 / F2
    jne  i9_c
    cmp  dl, 3Bh
    jb   i9_c
    cmp  dl, 3Ch
    ja   i9_c
    mov  al, dl
    sub  al, 3Bh
    mov  modus, al
    mov  tot, 0
    mov  es:[1Ch], di           ; Eintrag des BIOS (falls einer) verwerfen
    jmp  fertig
i9_c:
    cmp  modus, 0
    je   fertig_j
    mov  si, offset tab56
    cmp  dl, 56h
    je   i9_e
    xor  si, si                 ; Taste ohne Tabelle (a, s, Leertaste ...)
    cmp  dl, 2
    jb   i9_e
    cmp  dl, 35h
    ja   i9_e
    mov  al, dl
    sub  al, 2
    mov  ah, 3
    mul  ah
    add  ax, offset tab
    mov  si, ax
i9_e:
    xor  cx, cx                 ; cx = neuer BIOS-Eintrag (0 = keiner)
    cmp  es:[1Ch], di
    je   i9_f
    mov  cx, es:[di]
i9_f:
    mov  al, altgr
    cmp  bh, 0Ch                ; Strg+Alt gilt auch als AltGr
    jne  i9_g
    mov  al, 1
i9_g:
    or   al, al
    jz   i9_h
    or   si, si                 ; AltGr-Zeichen
    jz   fertig_j
    mov  al, [si+2]
    or   al, al
    jz   fertig_j
    jmp  setzen
i9_h:
    test bl, 0Ch                ; Strg oder Alt: nur y/z tauschen
    jz   i9_i
    cmp  yz, 0
    je   fertig_j
    cmp  dl, 15h
    je   i9_yz
    cmp  dl, 2Ch
    jne  fertig_j
i9_yz:
    jcxz fertig_j3
    xor  dl, 39h                ; 15h <-> 2Ch
    mov  ch, dl
    test bl, 04h
    jz   i9_yz2
    mov  cl, 1Ah                ; Strg+Z
    cmp  dl, 2Ch
    je   i9_yz2
    mov  cl, 19h                ; Strg+Y
i9_yz2:
    mov  es:[di], cx
fertig_j3:
    jmp  fertig
i9_i:
    xor  al, al
    or   si, si
    jz   i9_k
    xor  bh, bh
    test bl, 03h
    jz   i9_j
    inc  bh
i9_j:
    test bl, 40h                ; Feststelltaste bei Buchstaben (y z � � �)
    jz   i9_j2
    mov  al, [si]
    cmp  al, 'A'
    jb   i9_j2
    cmp  al, 0E1h
    je   i9_j2
    xor  bh, 1
i9_j2:
    mov  al, bh
    xor  ah, ah
    add  si, ax
    mov  al, [si]
i9_k:
    or   al, al
    jnz  i9_l
    cmp  tot, 0                 ; nur bei anstehender Tottaste: BIOS-Zeichen nehmen
    je   fertig_j2
    jcxz fertig_j2
    mov  al, cl
    or   al, al
    jz   fertig_j2
    jmp  short i9_m
i9_l:
    cmp  al, 3                  ; Tottaste?
    ja   i9_m
    mov  tot, al
    mov  es:[1Ch], di           ; nichts ausgeben
fertig_j2:
    jmp  fertig
i9_m:
    mov  dh, al
    cmp  tot, 0
    je   ausgeben
    mov  al, tot                ; Tottaste anstehend
    mov  tot, 0
    dec  al
    mov  ah, 6
    mul  ah
    add  ax, offset totab
    mov  si, ax                 ; si -> a e i o u, Zeichen allein
    xor  bx, bx
i9_v:
    cmp  dh, vokal[bx]
    je   i9_w
    inc  bx
    cmp  bx, 5
    jb   i9_v
    cmp  dh, ' '
    je   i9_w                   ; bx = 5: Zeichen allein
    mov  al, [si+5]             ; sonst: Akzent, danach die Taste
    mov  ah, dl
    jcxz i9_x
    mov  es:[di], al
    mov  al, dh
    call rein
    jmp  short fertig
i9_x:
    call rein
    jmp  short einfuegen
i9_w:
    mov  dh, [si+bx]
    jmp  short ausgeben
setzen:
    mov  dh, al
; ausgeben: dh = Zeichen, dl = Scancode, di = Platz des neuen Eintrags, cx = BIOS-Eintrag (0 = keiner)
ausgeben:
    jcxz einfuegen
    mov  es:[di], dh            ; Scancode im hohen Byte bleibt
    jmp  short fertig
einfuegen:
    mov  al, dh
    mov  ah, dl
    call rein
fertig:
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  dx
    pop  cx
    pop  bx
    pop  ax
    iret

; rein: ax ans Pufferende haengen (voll: verwerfen)
rein:
    push si
    push bx
    mov  si, es:[1Ch]
    mov  bx, si
    add  bx, 2
    cmp  bx, es:[82h]
    jb   r1
    mov  bx, es:[80h]
r1: cmp  bx, es:[1Ah]
    je   r2
    mov  es:[si], ax
    mov  es:[1Ch], bx
r2: pop  bx
    pop  si
    ret
ende_res:

; ---------------- Installation (nicht resident) ----------------
install:
    push cs
    pop  ds
    push cs
    pop  es
    cld
    mov  si, 81h                ; Befehlszeile: erstes Wort = DE / GR / US / Datei
p_sp:
    lodsb
    cmp  al, ' '
    je   p_sp
    cmp  al, 9
    je   p_sp
    dec  si
    mov  di, offset kname
    mov  cx, 70
    xor  dx, dx                 ; dl = Punkt gesehen
p_cp:
    lodsb
    cmp  al, ' '
    jbe  p_end
    cmp  al, '.'
    jne  p_cp2
    mov  dl, 1
p_cp2:
    cmp  al, 'a'
    jb   p_cp3
    cmp  al, 'z'
    ja   p_cp3
    sub  al, 20h
p_cp3:
    stosb
    loop p_cp
p_end:
    mov  byte ptr [di], 0
    mov  bl, 1
    mov  ax, di
    sub  ax, offset kname
    jz   gefunden               ; nichts angegeben: DE
    cmp  ax, 2
    jne  p_datei
    mov  ax, word ptr kname
    cmp  ax, 'SU'
    jne  p_de
    mov  bl, 0
    jmp  gefunden
p_de:
    cmp  ax, 'ED'
    je   gefunden
    cmp  ax, 'RG'
    je   gefunden
p_datei:
    or   dl, dl                 ; ohne Punkt: .KBD anhaengen
    jnz  p_offen
    mov  word ptr [di], 'K.'
    mov  word ptr [di+2], 'DB'
    mov  byte ptr [di+4], 0
    add  di, 4
p_offen:
    push di
    mov  ax, 3D00h
    mov  dx, offset kname
    int  21h
    pop  di
    jc   p_fehl
    mov  bx, ax
    mov  ah, 3Fh
    mov  cx, KBDLEN + 7         ; ein Byte mehr: zu lange Datei erkennen
    mov  dx, offset kbuf
    int  21h
    pushf
    push ax
    mov  ah, 3Eh
    int  21h
    pop  ax
    popf
    jc   p_fehl
    cmp  ax, KBDLEN + 6
    jne  p_fehl
    cmp  word ptr kbuf, 'DR'
    jne  p_fehl
    cmp  word ptr kbuf+2, 'BK'
    jne  p_fehl
    cmp  word ptr kbuf+4, 0144h ; 'D', 1
    jne  p_fehl
    mov  si, offset kbuf + 6
    mov  di, offset yz
    mov  cx, KBDLEN
    rep  movsb
    mov  datei, 1
    mov  bl, 1
    jmp  short gefunden
p_fehl:
    mov  byte ptr [di], '$'
    mov  dx, offset m_kfehl
    mov  ah, 9
    int  21h
    mov  dx, offset kname
    int  21h
    mov  dx, offset m_kfehl2
    int  21h
    mov  bl, 1
gefunden:
    mov  modus, bl
    push bx
    mov  ax, 0AD80h
    xor  di, di
    mov  es, di
    int  2Fh
    pop  bx
    cmp  al, 0FFh
    jne  laden
    mov  ax, es                 ; geladen: RDKEYB? dann nur umschalten
    or   ax, di
    jz   fremd
    cmp  word ptr es:[di], 'DR'
    jne  fremd
    cmp  word ptr es:[di+2], 'BK'
    jne  fremd
    mov  es:[di+4], bl
    or   bl, bl                 ; DE/Datei: Tabelle (eingebaut oder geladen) ins residente RDKEYB
    jz   um_ok
    add  di, offset yz - offset sig
    mov  si, offset yz
    mov  cx, KBDLEN
    cli
    rep  movsb
    sti
um_ok:
    mov  dx, offset m_um
    jmp  short meldung_ende
fremd:
    mov  dx, offset m_fremd
meldung_ende:
    call meldung
    mov  ax, 4C00h
    int  21h
laden:
    mov  ax, 3509h
    int  21h
    mov  word ptr alt9, bx
    mov  word ptr alt9+2, es
    mov  ah, 0C0h               ; BIOS-Merkmale: Bit 4 von Byte 5 = INT 9 ruft INT 15h/4Fh
    stc
    int  15h
    jc   l0
    or   ah, ah
    jnz  l0
    test byte ptr es:[bx+5], 10h
    jz   l0
    mov  mit4f, 1
    mov  ax, 3515h
    int  21h
    mov  word ptr alt15, bx
    mov  word ptr alt15+2, es
    mov  dx, offset int15
    mov  ax, 2515h
    int  21h
l0: mov  ax, 352Fh
    int  21h
    mov  word ptr alt2f, bx
    mov  word ptr alt2f+2, es
    mov  ax, 40h                ; Puffergrenzen fehlen im XT-BIOS: 1Eh..3Eh eintragen
    mov  es, ax
    cmp  word ptr es:[80h], 0
    jne  l1
    mov  word ptr es:[80h], 1Eh
    mov  word ptr es:[82h], 3Eh
l1: mov  dx, offset int9
    mov  ax, 2509h
    int  21h
    mov  dx, offset int2f
    mov  ax, 252Fh
    int  21h
    mov  dx, offset m_ok
    call meldung
    mov  es, cs:[2Ch]           ; Umgebung freigeben
    mov  ah, 49h
    int  21h
    mov  dx, offset ende_res + 15
    mov  cl, 4
    shr  dx, cl
    mov  ax, 3100h
    int  21h

meldung:
    push cs
    pop  es
    mov  ah, 9
    int  21h
    cmp  datei, 0
    je   m0
    cmp  modus, 0
    je   m0
    mov  di, offset kname       ; Dateiname ausgeben
    xor  al, al
    mov  cx, 80
    repne scasb
    mov  byte ptr [di-1], '$'
    mov  dx, offset kname
    mov  ah, 9
    int  21h
    mov  dx, offset m_nl
    jmp  short m1
m0: mov  dx, offset m_de
    cmp  modus, 0
    jne  m1
    mov  dx, offset m_us
m1: mov  ah, 9
    int  21h
    ret

m_ok    db 'RDKEYB geladen: $'
m_um    db 'RDKEYB umgeschaltet: $'
m_fremd db 'Tastaturtreiber ist schon geladen, RDKEYB bleibt draussen. $'
m_de    db 'DE',13,10,'$'
m_us    db 'US',13,10,'$'
m_nl    db 13,10,'$'
m_kfehl db 'RDKEYB: Belegung $'
m_kfehl2 db ' nicht lesbar, eingebaute DE-Belegung gilt.',13,10,'$'
datei   db 0
kname   db 80 dup (0)
kbuf    db KBDLEN + 8 dup (0)
_TEXT ends
end start
