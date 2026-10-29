; RD2PAS.ASM - Vorspann fuer Pascal-Programme (Turbo Pascal 5.5-7.0, Free Pascal i8086-msdos)
; nasm -f bin -o RD2PAS.BIN RD2PAS.ASM
;
; RDRC /PAS (rd2link.py --pas) setzt dieses Stueck VOR das Bild einer Pascal-EXE und verschiebt das Bild
; um seine Laenge (alle Verweise +N Paragraphen). Der Kern ruft den Einsprung wie bei C per FAR CALL:
;   AX = 5232h, ES:BX = API-Tabelle, CX = HINST, DX = show, DI:SI = Befehlszeile
; Das Laufzeitsystem von Pascal erwartet einen DOS-Start: PSP in DS/ES, eigener Stack aus dem MZ-Kopf,
; Speicherende in PSP:2 bzw. im MCB davor, und es ruft beim Start und am Ende DOS auf. Dafuer baut der
; Vorspann ein MCB und ein PSP vor dem Bild und faengt waehrend des Starts INT 21h ab, aber nur fuer
; Aufrufe aus dem eigenen Bild:
;   25h Vektor setzen   wird ignoriert (Pascal haengt sonst INT 0/23h/24h systemweit um)
;   4Ah Block aendern   gelingt bis zum Ende des Programmblocks
;   48h/49h             kein DOS-Speicher (Fehler 8) / ok
;   51h/62h PSP         liefert das eigene PSP
;   00h/31h/4Ch Ende    zurueck zum Kern
; Die Unit RD2 meldet am Ende ihrer Initialisierung "fertig" (Haken weg) und kehrt ueber ExitProc mit
; dem Exitcode durch "ende" zum Kern zurueck. Ohne Unit bleibt der Haken bis zum Ende stehen.
;
; Aufbau (Offsets ab Ladesegment BASE):
;   000h  MCB  ('M', Besitzer = PSP, Groesse bis Blockende)
;   010h  PSP  (CD 20, PSP:2 Blockende, PSP:2Ch Umgebung, PSP:80h Befehlszeile)
;   110h  Einsprung und Daten, Werte fuer die Unit ab PSP:100h (api PSP:114h, hinst 118h, show 11Ah, cmd 11Ch, fertig 120h, ende 122h)
cpu 8086
org 0

mcb:    db 'M'
        dw 0, 0
        times 16 - ($ - mcb) db 0
psp:    times 256 db 0
                                        ; ---- PSP:100h ----
start:  jmp short einsprung             ; 110h
        db 'RD2PAS'                     ; 112h Kennung (RDRC: schon vorgespannt)
        dw 1                            ; 118h Fassung
o_ip    dw 0                            ; 11Ah  Einsprung, Stack (relativ zu BASE)
o_cs    dw 0                            ; 11Ch  und Blockende in Paragraphen ab BASE
o_sp    dw 0                            ; 11Eh  setzt RDRC
o_ss    dw 0                            ; 120h
o_top   dw 0                            ; 122h
api     dd 0                            ; 124h  PSP:114h  fuer die Unit
hinst   dw 0                            ; 128h  PSP:118h
show    dw 0                            ; 12Ah  PSP:11Ah
cmd     dd 0                            ; 12Ch  PSP:11Ch
        dw fertig                       ; 130h  PSP:120h  far BASE:fertig
        dw ende                         ; 132h  PSP:122h  far BASE:ende (WORD code), pascal
kss     dw 0
ksp     dw 0
alt21   dd 0
haken   db 0
        db 0
o_endp  dw 0                            ; Bildende (Paragraph, Byte) ab BASE, setzt RDRC
o_endo  dw 0

%macro DBG 1
%ifdef DEBUG
        push ax
        push dx
        mov dx,3F8h
        mov al,%1
        out dx,al
        pop dx
        pop ax
%endif
%endmacro
einsprung:
        DBG 'A'
        cmp  ax, 5232h
        jne  dosstart
        mov  [cs:api], bx
        mov  [cs:api+2], es
        mov  [cs:hinst], cx
        mov  [cs:show], dx
        mov  [cs:cmd], si
        mov  [cs:cmd+2], di
        mov  [cs:kss], ss
        mov  [cs:ksp], sp
        mov  ax, cs
        mov  ds, ax
        mov  es, ax
        inc  ax
        mov  [mcb+1], ax                ; Besitzer = PSP
        mov  bx, [o_top]
        dec  bx
        mov  [mcb+3], bx                ; Groesse ohne MCB
        mov  word [psp], 20CDh
%ifdef DEBUG
        mov  word [psp], 5AEBh            ; jmp short PSP:5Ch
        mov  byte [psp+5Ch], 0EAh
        mov  word [psp+5Dh], dbgz
        mov  [psp+5Fh], cs
%endif
        mov  ax, cs
        add  ax, [o_top]
        mov  [psp+2], ax                ; Blockende
        mov  ax, cs
        add  ax, (umgebung - $$) / 16
        mov  [psp+2Ch], ax
        push ds                         ; Befehlszeile nach PSP:80h (Laenge, Text, CR)
        lds  si, [cmd]
        mov  di, psp + 81h
        xor  cx, cx
        mov  ax, ds
        or   ax, si
        jz   .leer
.kopie: lodsb
        or   al, al
        jz   .leer
        stosb
        inc  cx
        cmp  cx, 126
        jb   .kopie
.leer:  pop  ds
        mov  al, 0Dh
        stosb
        mov  [psp+80h], cl
        mov  ax, cs                     ; Speicher hinter dem Bild loeschen (Pascal-Variablen, Heap, Stack)
        add  ax, [o_endp]
        mov  es, ax
        mov  di, [o_endo]
        mov  cx, 16
        sub  cx, di
        xor  al, al
        cld
        rep  stosb
        mov  bx, cs
        add  bx, [o_top]
        mov  dx, es
        inc  dx
.null:  cmp  dx, bx
        jae  .genull
        mov  es, dx
        mov  cx, bx
        sub  cx, dx
        cmp  cx, 0FFFh
        jbe  .stueck
        mov  cx, 0FFFh
.stueck:
        add  dx, cx
        shl  cx, 1
        shl  cx, 1
        shl  cx, 1
        xor  di, di
        xor  ax, ax
        rep  stosw
        jmp  .null
.genull:
        xor  ax, ax                     ; INT 21h einhaengen (direkt in der Tabelle, ohne DOS)
        mov  es, ax
        cli
        mov  ax, [es:84h]
        mov  [alt21], ax
        mov  ax, [es:86h]
        mov  [alt21+2], ax
        mov  word [es:84h], haken21
        mov  [es:86h], cs
        mov  byte [haken], 1
        DBG 'B'
        mov  ax, cs                     ; Stack und Einsprung aus dem MZ-Kopf
        add  ax, [o_ss]
        mov  bx, [o_sp]
        mov  ss, ax
        mov  sp, bx
        sti
        mov  ax, cs
        add  ax, [o_cs]
        push ax
        push word [o_ip]
        mov  ax, cs
        inc  ax
        mov  ds, ax                     ; DS = ES = PSP wie beim DOS-Start
        mov  es, ax
        xor  ax, ax
        retf

dosstart:                               ; direkt unter DOS gestartet
        push cs
        pop  ds
        mov  dx, meldung
        mov  ah, 9
        int  21h
        mov  ax, 4C01h
        int  21h

; ---- INT 21h waehrend des Starts --------------------------------------------
haken21:
%ifdef DEBUG
        push bp
        mov bp,sp
        push ax
        push dx
        mov dx,3F8h
        mov al,'i'
        out dx,al
        pop dx
        pop ax
        push ax
        call hexw
        mov ax,[bp+4]
        call hexw
        pop ax
        pop bp
%endif
        cmp  ah, 25h
        je   .pruef
        cmp  ah, 4Ah
        je   .pruef
        cmp  ah, 4Ch
        je   .pruef
        cmp  ah, 48h
        je   .pruef
        cmp  ah, 49h
        je   .pruef
        cmp  ah, 62h
        je   .pruef
        cmp  ah, 51h
        je   .pruef
        cmp  ah, 31h
        je   .pruef
        or   ah, ah
        jz   .pruef
.weiter:
        jmp  far [cs:alt21]
.pruef: push bp                         ; nur Aufrufe aus dem eigenen Programmblock
        mov  bp, sp
        push bx
        mov  bx, cs
        cmp  [bp+4], bx
        jb   .fremd
        add  bx, [cs:o_top]
        cmp  [bp+4], bx
        jae  .fremd
        pop  bx
        pop  bp
%ifdef DEBUG
        push ax
        mov al,'h'
        push dx
        mov dx,3F8h
        out dx,al
        pop dx
        pop ax
        push ax
        call hexw
        pop ax
%endif
        cmp  ah, 25h
        je   .ok
        cmp  ah, 49h
        je   .ok
        cmp  ah, 4Ah
        je   .block
        cmp  ah, 48h
        je   .nomem
        cmp  ah, 62h
        je   .psp
        cmp  ah, 51h
        je   .psp
        cmp  ah, 4Ch                    ; 4Ch, 31h: Code in AL; 00h: Code 0
        je   .ende
        cmp  ah, 31h
        je   .ende
        xor  al, al
.ende:  xor  ah, ah
        DBG 'X'
        jmp  zumkern
.psp:   mov  bx, cs
        inc  bx
.ok:    clc
.zurueck:
        sti
        retf 2
.nomem: mov  ax, 8
        xor  bx, bx
        stc
        jmp  .zurueck
.block: push ax                         ; ES = Block, BX = gewuenschte Paragraphen
        push dx
        mov  ax, cs
        add  ax, [cs:o_top]
        mov  dx, es
        sub  ax, dx                     ; verfuegbar ab ES
        cmp  bx, ax
        pop  dx
        pop  ax
        jbe  .ok
        mov  bx, cs
        add  bx, [cs:o_top]
        push dx
        mov  dx, es
        sub  bx, dx
        pop  dx
        mov  ax, 8
        stc
        jmp  .zurueck
.fremd: DBG 'f'
%ifdef DEBUG
        push ax
        call hexw
        mov ax,[bp+4]
        call hexw
        mov ax,cs
        call hexw
        mov ax,[cs:o_top]
        call hexw
        pop ax
%endif
        pop  bx
        pop  bp
        jmp  .weiter

; ---- fuer die Unit RD2 ---------------------------------------------------------
abhaengen:                              ; Haken entfernen, wenn er noch obenauf liegt
        push ax
        push es
        cmp  byte [cs:haken], 0
        je   .raus
        xor  ax, ax
        mov  es, ax
        cli
        cmp  word [es:84h], haken21
        jne  .bleibt
        mov  ax, cs
        cmp  [es:86h], ax
        jne  .bleibt
        mov  ax, [cs:alt21]
        mov  [es:84h], ax
        mov  ax, [cs:alt21+2]
        mov  [es:86h], ax
        mov  byte [cs:haken], 0
.bleibt:
        sti
.raus:  pop  es
        pop  ax
        ret

fertig: DBG 'F'
        call abhaengen                  ; far: Start abgeschlossen
        retf

ende:   mov  bp, sp                     ; far pascal ende(code: Word): kehrt nie zurueck
        mov  ax, [bp+4]
zumkern:
        call abhaengen
        cli
        mov  ss, [cs:kss]
        mov  sp, [cs:ksp]
        sti
        retf                            ; zum Kern, AX = Exitcode

%ifdef DEBUG
dbgz:   push bp
        mov bp,sp
        mov al,'Z'
        mov dx,3F8h
        out dx,al
        mov ax,[bp+2]
        call hexw
        mov ax,[bp+4]
        call hexw
        mov ax,[bp+6]
        call hexw
        mov ax,ss
        call hexw
        mov ax,sp
        call hexw
.h:     jmp .h
%endif
 %ifdef DEBUG
hexw:   push cx
        push dx
        mov cx,4
.l:     push cx
        mov cl,4
        rol ax,cl
        pop cx
        push ax
        and al,15
        add al,'0'
        cmp al,'9'
        jbe .o
        add al,7
.o:     mov dx,3F8h
        out dx,al
        pop ax
        loop .l
        mov al,' '
        out dx,al
        pop dx
        pop cx
        ret
%endif
meldung db 'Dieses Programm ben', 94h, 'tigt RetroDesk 2.', 13, 10, '$'

        align 16, db 0
umgebung:
        dw 0                            ; leere Umgebung, dann Anzahl 1 und leerer Programmname
        dw 1
        db 0
        align 16, db 0
