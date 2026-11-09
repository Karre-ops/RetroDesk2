; RDSWAP.ASM - Auslager-Rest fuer RunDos (SPEC 14), wird zu RDSWAP.BIN (roh, ab Versatz 0).
; Der Kern laedt die Datei in einen Block ganz oben im Speicher, fuellt den Datenbereich
; (0..S_DATA-1) und springt nach S_DATA. Der Rest schreibt Kern und belegte Arena in den
; XMS-Speicher oder in eine Datei, gibt den Speicher frei, startet das Programm (4B00h), holt
; alles an dieselbe Adresse zurueck und springt in den Kern (rd_swaprun) zurueck.
.8086
_TEXT segment word public 'CODE'
assume cs:_TEXT, ds:_TEXT
S_XENT  equ 0       ; dd XMS-Einsprung, 0 = Datei
S_XH    equ 4
S_FH    equ 6
S_PSP   equ 8
S_KPAR  equ 10
S_ASEG  equ 12
S_APAR  equ 14
S_SWSEG equ 16
S_SWPAR equ 18
S_RET   equ 20      ; dd Rueckkehr in den Kern
S_SS    equ 24
S_SP    equ 26
S_EXIT  equ 28
S_ERR   equ 30      ; 0 ok, 1 Auslagern ging nicht, 100h + DOS-Fehler beim Start
S_FLAGS equ 32
S_PB    equ 34      ; Parameterblock 4B00h
S_MV    equ 48      ; XMS-Verschiebeblock
S_CUR   equ 64
S_REST  equ 66
S_DIR   equ 68      ; 0 = auslagern, 1 = zurueckholen
S_XO    equ 70      ; dd Versatz im XMS-Block
S_TOP   equ 74      ; Stack-Ende im Block
S_MODE  equ 76      ; 0 = auslagern und zurueck, 1 = Neustart (ohne XMS und Festplatte), 2 = Shell
S_PATH  equ 78      ; Programmname
S_TAIL  equ 158     ; Befehlszeile (DOS-Form)
S_FCB   equ 286
S_RDP   equ 318     ; RD2-Verzeichnis (vom Kern), der Rest haengt RD.EXE bzw. RD2SWAP.$$$ an
S_SPX   equ 398     ; Stack beim Programmstart
S_DATA  equ 400     ; danach Code und Meldungen


        org  0
        db   S_DATA dup (0)
st_code:
        mov  ax, cs
        cli
        mov  ss, ax
        mov  sp, cs:[S_TOP]
        sti
        mov  ds, ax
        push ds
        pop  es
        call st_zeile
        mov  di, S_RDP                   ; Ende des Verzeichnisnamens
        xor  al, al
        mov  cx, 70
        repne scasb
        dec  di
        mov  si, offset st_rdexe
        cmp  word ptr ds:[S_MODE], 0
        jne  st_neu0
        mov  si, offset st_swpn
        call st_cat
        cmp  word ptr ds:[S_XENT+2], 0   ; XMS-Block holen, sonst Datei anlegen
        je   st_dat
        mov  ax, ds:[S_SWPAR]
        add  ax, 63
        mov  cl, 6
        shr  ax, cl
        mov  dx, ax
        inc  dx                          ; + 1 KB: Kopie der Interrupt-Vektoren
        mov  ah, 09h
        call dword ptr ds:[S_XENT]
        mov  ds:[S_XH], dx
        cmp  ax, 1
        je   st_aus
        mov  word ptr ds:[S_XENT+2], 0
st_dat: mov  ah, 3Ch
        xor  cx, cx
        mov  dx, S_RDP
        int  21h
        mov  ds:[S_FH], ax
        jc   st_f1
st_aus: mov  word ptr ds:[S_DIR], 0
        call st_xfer
        jc   st_af
        in   al, 21h                     ; Stand vor dem Programm: Interrupt-Maske und Vektoren
        mov  st_pic, al
        call st_ivt
        jnc  st_frei
st_af:  call st_weg
st_f1:  mov  word ptr ds:[S_ERR], 1
        jmp  st_zur
st_frei:
        mov  es, ds:[S_ASEG]             ; Arena frei
        mov  ah, 49h
        int  21h
        mov  es, ds:[S_PSP]              ; Kern-Block auf den PSP verkleinern
        mov  bx, 16
        mov  ah, 4Ah
        int  21h
        call st_prog
        call st_los                      ; residentes Programm? dann wieder entfernen
        jc   st_tot
        or   dl, dl
        jz   st_rein
        mov  ah, 48h                     ; Puffer fuer die Vektorkopie (DOS legt freie Bloecke zusammen)
        mov  bx, 64
        int  21h
        jc   st_tot
        mov  st_buf, ax
        mov  word ptr ds:[S_DIR], 1      ; Vektoren und Maske auf den Stand vor dem Programm
        call st_ivt
        jc   st_tot
        push ds
        mov  ax, st_buf
        mov  ds, ax
        xor  si, si
        xor  di, di
        mov  es, di
        mov  cx, 512
        cli
        rep  movsw
        pop  ds
        mov  al, st_pic
        out  21h, al
        sti
        mov  es, st_buf
        mov  ah, 49h
        int  21h
        mov  dx, offset st_rmsg
        call st_hinweis
st_rein:
        mov  es, ds:[S_PSP]
        mov  bx, ds:[S_KPAR]
        mov  ah, 4Ah
        int  21h
        jc   st_tot
        mov  bx, ds:[S_APAR]
        mov  ah, 48h
        int  21h
        jc   st_tot
        cmp  ax, ds:[S_ASEG]
        jne  st_tot
        mov  word ptr ds:[S_DIR], 1
        call st_xfer
        jc   st_tot
        call st_weg

st_zur: cli
        mov  ss, ds:[S_SS]
        mov  sp, ds:[S_SP]
        sti
        jmp  dword ptr cs:[S_RET]
; Zurueck geht nicht (Speicher belegt, Lesefehler): Auslagerziel freigeben und RD2 sauber beenden.
; Kein Neustart des Rechners. Laeuft RD2 als Shell, startet der Rest darueber es neu.
st_tot: call st_weg
        mov  dx, offset st_tmsg
        call st_hinweis
        mov  ax, 4C00h
        int  21h

; Meldung DX ausgeben, auf eine Taste warten, hoechstens etwa 10 s
st_hinweis:
        mov  ah, 9
        int  21h
        push ds
        mov  ax, 40h
        mov  ds, ax
        mov  bx, ds:[6Ch]
sh_w:   mov  ah, 1
        int  16h
        jnz  sh_t
        mov  ax, ds:[6Ch]
        sub  ax, bx
        cmp  ax, 182
        jb   sh_w
        jmp  short sh_e
sh_t:   xor  ah, ah
        int  16h
sh_e:   pop  ds
        ret

; Zurueckgelassene Bloecke zwischen S_SWSEG und dem Rest freigeben (TSR, Umgebung, eigene Bloecke).
; DL = 1: etwas freigegeben. CF: ein DOS-Block liegt dort, Freigeben geht nicht.
st_los: xor  dl, dl
sl_neu: push dx
        mov  ah, 52h
        int  21h
        pop  dx
        mov  ax, es:[bx-2]               ; erster Speicherblock
sl_lp:  mov  es, ax
        mov  bx, cs
        dec  bx
        cmp  ax, bx                      ; beim Rest angekommen: fertig
        jae  sl_ok
        mov  bx, es:[1]                  ; Besitzer
        cmp  ax, ds:[S_SWSEG]
        jb   sl_nx
        or   bx, bx
        jz   sl_nx
        cmp  bx, ds:[S_PSP]
        je   sl_nx
        cmp  bx, 8
        je   sl_err
        inc  ax
        mov  es, ax
        mov  ah, 49h
        int  21h
        jc   sl_err
        mov  dl, 1
        jmp  sl_neu                      ; DOS kann Bloecke zusammengelegt haben: von vorn
sl_nx:  cmp  byte ptr es:[0], 'Z'
        je   sl_ok
        add  ax, es:[3]
        inc  ax
        jmp  sl_lp
sl_ok:  clc
        ret
sl_err: stc
        ret

; Kopie der Interrupt-Vektoren (1 KB) hinter dem Auslagerbereich, ueber st_xfer. DIR 0: 0000:0000
; sichern, DIR 1: in den Puffer st_buf holen. CF = Fehler.
st_ivt: mov  ax, ds:[S_SWPAR]
        mov  dx, ax
        mov  cl, 4
        shl  ax, cl
        mov  cl, 12
        shr  dx, cl                      ; DX:AX = Versatz im Ziel
        mov  ds:[S_XO], ax
        mov  ds:[S_XO+2], dx
        mov  word ptr ds:[S_REST], 64
        xor  bx, bx
        cmp  word ptr ds:[S_DIR], 0
        je   iv_1
        mov  bx, st_buf
iv_1:   mov  ds:[S_CUR], bx
        cmp  word ptr ds:[S_XENT+2], 0
        jne  xf_loop
        mov  cx, dx
        mov  dx, ax
        mov  ax, 4200h
        mov  bx, ds:[S_FH]
        int  21h
        jc   xf_err
        jmp  xf_loop
; Neustart: Kern und Arena sind schon frei (RD hat sich beendet). Schleife: Programm,
; dann RD.EXE /W:seg + alte Befehlszeile. Endet dieses RD mit Code 0D5h, hat es hier das
; naechste Programm eingetragen.
st_neu0:
        call st_cat
st_neu: mov  es, ds:[S_PSP]
        mov  bx, 16
        mov  ah, 4Ah
        int  21h
        cmp  word ptr ds:[S_MODE], 2     ; Shell: zuerst RD selbst
        je   st_rd
st_lauf:
        call st_prog
st_rd:  call st_rdneu
        mov  al, 1
        jc   st_shell
        mov  ah, 4Dh
        int  21h
        cmp  ax, 0D5h
        jne  st_shell
        call st_zeile                    ; das neue RD hat Programm und Zeile eingetragen
        jmp  st_lauf
st_shell:                                ; RD ist zu Ende (oder fehlt)
        cmp  word ptr ds:[S_MODE], 2
        jne  st_ende
        mov  dx, offset st_smsg          ; als Shell nie enden: auf Taste neu starten
        mov  ah, 9
        int  21h
        xor  ah, ah
        int  16h
        jmp  st_rd
st_ende:
        mov  ah, 4Ch
        int  21h

; Befehlszeile: Kern legt den Text ab S_TAIL+2 ab, daraus wird die DOS-Form
st_zeile:
        mov  di, S_TAIL + 2
        xor  al, al
        mov  cx, 124
        repne scasb
        dec  di
        mov  byte ptr [di], 13
        mov  ax, di
        sub  ax, S_TAIL + 1
        mov  ds:[S_TAIL], al
        mov  byte ptr ds:[S_TAIL + 1], ' '
        mov  ax, cs
        mov  word ptr ds:[S_PB + 2], S_TAIL
        mov  ds:[S_PB + 4], ax
        mov  word ptr ds:[S_PB + 6], S_FCB
        mov  ds:[S_PB + 8], ax
        mov  word ptr ds:[S_PB + 10], S_FCB + 16
        mov  ds:[S_PB + 12], ax
        ret

; Text DS:SI (mit 0) nach ES:DI anhaengen
st_cat: lodsb
        stosb
        or   al, al
        jnz  st_cat
        ret

; Auslagerziel freigeben: XMS-Block oder Datei schliessen und loeschen
st_weg: cmp  word ptr ds:[S_XENT+2], 0
        je   sw_d
        mov  dx, ds:[S_XH]
        mov  ah, 0Ah
        call dword ptr ds:[S_XENT]
        ret
sw_d:   mov  bx, ds:[S_FH]
        mov  ah, 3Eh
        int  21h
        mov  dx, S_RDP
        mov  ah, 41h
        int  21h
        ret

; 4B00h mit eigenem Stack; DX = Name, Block S_PB. CF/AX wie DOS.
st_exec:
        push ds
        pop  es
        mov  bx, S_PB
        mov  ds:[S_SPX], sp
        mov  ax, 4B00h
        int  21h
        mov  bx, cs                      ; DOS 2 laesst nur CS:IP stehen
        cli
        mov  ss, bx
        mov  sp, cs:[S_SPX]
        sti
        mov  ds, bx
        ret


; Hauptumgebung (PLAN_DOS Schritt 1, 08.11.2026): Hat RD selbst kein COMSPEC (SHELL=RD.EXE,
; niemand hat AUTOEXEC.BAT ausgefuehrt), baut der Rest ab ENVO eine Umgebung aus den SET- und
; PATH-Zeilen der AUTOEXEC.BAT (Startlaufwerk, sonst C:) und setzt COMSPEC. Sonst erbt das Programm.
ENVO    equ 1920
ENVMAX  equ 500
st_env: push bp                          ; bp gehoert dem Kern (C-Aufrufer)
        mov  word ptr ds:[S_PB], 0
        mov  es, ds:[S_PSP]
        mov  es, es:[2Ch]
        xor  di, di
        call se_such
        jne  se_bau
        pop  bp
        ret
se_bau: push ds
        pop  es
        mov  di, ENVO
        mov  ah, 48h                     ; Puffer fuer AUTOEXEC.BAT
        mov  bx, 129
        int  21h
        jc   se_cs
        mov  bp, ax
        mov  ax, 3305h
        int  21h
        add  dl, '@'
        mov  st_ae, dl
        mov  dx, offset st_ae
        mov  ax, 3D00h
        int  21h
        jc   se_fr
        mov  bx, ax
        push ds
        mov  ds, bp
        xor  dx, dx
        mov  cx, 2048
        mov  ah, 3Fh
        int  21h
        jnc  se_g
        xor  ax, ax
se_g:   mov  si, ax
        mov  word ptr [si], 0Ah          ; LF, 0: Ende
        xor  si, si
        mov  ah, 3Eh
        int  21h
se_zl:  lodsb                            ; Zeilenanfang: Leerraum und @ ueberspringen
        or   al, al
        jz   se_ende
        cmp  al, ' '
        jbe  se_zl
        cmp  al, '@'
        je   se_zl
        dec  si
        mov  bx, offset st_wset
        call se_cmp
        je   se_set
        mov  bx, offset st_wpath
        call se_cmp
        je   se_path
se_rest: lodsb
        or   al, al
        jz   se_ende
        cmp  al, 10
        jne  se_rest
        jmp  se_zl
se_path: mov  bx, offset st_wpgr          ; "PATH="
se_p1:  mov  al, cs:[bx]
        inc  bx
        or   al, al
        jz   se_p2
        call se_put
        jmp  se_p1
se_p2:  inc  si                           ; Leerzeichen oder '=' nach PATH
        jmp  se_wert
se_set:                                  ; Name gross bis '='
se_s1:  lodsb
        cmp  al, ' '
        jb   se_wz
        cmp  al, 'a'
        jb   se_s2
        cmp  al, 'z'
        ja   se_s2
        sub  al, 20h
se_s2:  call se_put
        cmp  al, '='
        jne  se_s1
se_wert: lodsb
        cmp  al, ' '
        jb   se_wz
        call se_put
        jmp  se_wert
se_wz:  dec  si
        xor  al, al
        call se_put
        jmp  se_rest
se_ende: pop  ds
se_fr:  push es
        mov  es, bp
        mov  ah, 49h
        int  21h
        pop  es
se_cs:  mov  byte ptr es:[di], 0         ; Ende fuer die Suche
        push di                          ; COMSPEC schon gesetzt?
        mov  di, ENVO
        call se_such
        pop  di
        je   se_fertig
        mov  si, offset st_csp           ; COMSPEC=RD2-Ordner\COMMAND.COM (INSTALL legt es dort ab)
        mov  cx, 8
se_c0:  lodsb
        call se_put
        loop se_c0
        mov  si, S_RDP                   ; S_RDP bis zum letzten '\' (bleibt unveraendert)
        mov  bx, si
se_c1:  lodsb
        cmp  al, '\'
        jne  se_c3
        mov  bx, si
se_c3:  or   al, al
        jnz  se_c1
        mov  si, S_RDP
se_c4:  cmp  si, bx
        jae  se_c5
        lodsb
        call se_put
        jmp  se_c4
se_c5:  mov  si, offset st_cpf
        call se_puts
        xor  al, al
        call se_put
se_fertig:
        xor  al, al
        stosb
        mov  ax, cs
        add  ax, ENVO / 16
        mov  ds:[S_PB], ax
        pop  bp
        ret
se_puts: lodsb                           ; ds:si (Rest) anhaengen, ohne 0
        or   al, al
        jz   sp_r
        call se_put
        jmp  se_puts
sp_r:   ret
se_put: cmp  di, ENVO + ENVMAX           ; ein Zeichen nach es:di, wenn Platz
        jae  sp_r
        stosb
        ret
; COMSPEC= in es:di.. suchen (Zeichenketten bis 00 00). ZF = 1: gefunden
se_such: cmp  byte ptr es:[di], 0
        je   ss_n
        push di
        mov  si, offset st_csp
        mov  cx, 8
        repe cmpsb
        pop  di
        je   sp_r
ss_w:   inc  di
        cmp  byte ptr es:[di - 1], 0
        jne  ss_w
        jmp  se_such
ss_n:   inc  di                          ; ZF = 0
        ret
; Wort aus cs:bx (klein) mit ds:si vergleichen, ohne Gross/Klein. ZF = 1 und si dahinter
se_cmp: push si
sc_l:   mov  ah, cs:[bx]
        or   ah, ah
        jz   sc_ok
        lodsb
        or   al, 20h
        inc  bx
        cmp  al, ah
        je   sc_l
        pop  si
        or   ah, ah                      ; ah <> 0: ZF = 0
        ret
sc_ok:  pop  ax
        ret

; Programm S_PATH starten, Fehler/Code merken, auf Wunsch auf eine Taste warten
st_prog:
        cmp  byte ptr ds:[S_PATH], 0      ; RD_DOS_NEU: kein Programm, nur RD neu starten
        je   sp_e
        call st_env
        mov  dx, S_PATH
        call st_exec
        jnc  sp_ok
        or   ah, 1                       ; 100h + Fehlercode
        mov  ds:[S_ERR], ax
        ret
sp_ok:  mov  ah, 4Dh
        int  21h
        mov  ds:[S_EXIT], ax
        test byte ptr ds:[S_FLAGS], 2
        jz   sp_e
        mov  dx, offset st_msg
        mov  ah, 9
        int  21h
        xor  ah, ah
        int  16h
sp_e:   ret

; RD.EXE neu starten: Befehlszeile " /W:ssss" + alte Befehlszeile aus dem PSP
st_rdneu:
        push ds
        pop  es
        mov  di, S_TAIL + 1
        mov  ax, '/' * 256 + ' '
        stosw
        mov  ax, ':' * 256 + 'W'
        stosw
        mov  dx, cs
        mov  cx, 4
rn_hex: push cx
        mov  cl, 4
        rol  dx, cl
        pop  cx
        mov  al, dl
        and  al, 0Fh
        add  al, '0'
        cmp  al, '9'
        jbe  rn_h1
        add  al, 7
rn_h1:  stosb
        loop rn_hex
        mov  ax, ds:[S_PSP]
        push ds
        mov  ds, ax
        mov  si, 81h
        mov  cx, 100
rn_cp:  lodsb
        cmp  al, 13
        je   rn_e
        stosb
        loop rn_cp
rn_e:   pop  ds
        mov  byte ptr [di], 13
        mov  ax, di
        sub  ax, S_TAIL + 1
        mov  ds:[S_TAIL], al
        mov  dx, S_RDP
        jmp  st_exec

; Bereich S_SWSEG/S_SWPAR uebertragen. CF = Fehler.
st_xfer:
        mov  ax, ds:[S_SWSEG]
        mov  ds:[S_CUR], ax
        mov  ax, ds:[S_SWPAR]
        mov  ds:[S_REST], ax
        xor  ax, ax
        mov  ds:[S_XO], ax
        mov  ds:[S_XO+2], ax
        cmp  word ptr ds:[S_XENT+2], 0
        jne  xf_loop
        mov  ax, 4200h                   ; Datei: an den Anfang
        mov  bx, ds:[S_FH]
        xor  cx, cx
        xor  dx, dx
        int  21h
        jc   xf_err
xf_loop:
        mov  bx, ds:[S_REST]
        or   bx, bx
        jz   xf_ok
        cmp  bx, 2048
        jbe  xf_n
        mov  bx, 2048
xf_n:   mov  ax, bx
        mov  cl, 4
        shl  ax, cl
        mov  cx, ax                      ; Bytes (hoechstens 32 KB)
        cmp  word ptr ds:[S_XENT+2], 0
        je   xf_datei
        mov  ds:[S_MV], cx
        mov  word ptr ds:[S_MV+2], 0
        cmp  word ptr ds:[S_DIR], 0
        jne  xf_in
        mov  word ptr ds:[S_MV+4], 0     ; Quelle: Handle 0 = Segment:Versatz
        mov  word ptr ds:[S_MV+6], 0
        mov  ax, ds:[S_CUR]
        mov  ds:[S_MV+8], ax
        mov  ax, ds:[S_XH]
        mov  ds:[S_MV+10], ax
        mov  ax, ds:[S_XO]
        mov  ds:[S_MV+12], ax
        mov  ax, ds:[S_XO+2]
        mov  ds:[S_MV+14], ax
        jmp  short xf_mv
xf_in:  mov  ax, ds:[S_XH]
        mov  ds:[S_MV+4], ax
        mov  ax, ds:[S_XO]
        mov  ds:[S_MV+6], ax
        mov  ax, ds:[S_XO+2]
        mov  ds:[S_MV+8], ax
        mov  word ptr ds:[S_MV+10], 0
        mov  word ptr ds:[S_MV+12], 0
        mov  ax, ds:[S_CUR]
        mov  ds:[S_MV+14], ax
xf_mv:  push bx
        push cx
        mov  si, S_MV
        mov  ah, 0Bh
        call dword ptr ds:[S_XENT]
        pop  cx
        pop  bx
        cmp  ax, 1
        jne  xf_err
        add  ds:[S_XO], cx
        adc  word ptr ds:[S_XO+2], 0
        jmp  short xf_next
xf_datei:
        push bx
        push cx
        mov  ah, 40h
        cmp  word ptr ds:[S_DIR], 0
        je   xf_d1
        mov  ah, 3Fh
xf_d1:  mov  bx, ds:[S_FH]
        mov  di, ds:[S_CUR]
        push ds
        mov  ds, di
        xor  dx, dx
        int  21h
        pop  ds
        pop  cx
        pop  bx
        jc   xf_err
        cmp  ax, cx
        jne  xf_err
xf_next:
        add  ds:[S_CUR], bx
        sub  ds:[S_REST], bx
        jmp  xf_loop
xf_ok:  clc
        ret
xf_err: stc
        ret

st_rdexe db 'RD.EXE', 0
st_swpn db 'RD2SWAP.$$$', 0
st_smsg db 13, 10, 'RD2 beendet. Taste: neu starten$'
st_msg  db 13, 10, 'Taste druecken...$'
st_tmsg db 13, 10, 'RD2: Speicher belegt, RD2 endet.$'
st_rmsg db 13, 10, 'RD2: residentes Programm entfernt. TSR vor RD2 laden.$'
st_pic  db 0
st_buf  dw 0
st_ae   db 'C:\AUTOEXEC.BAT', 0
st_wset db 'set ', 0
st_wpath db 'path', 0
st_wpgr db 'PATH=', 0
st_csp  db 'COMSPEC='
st_cpf  db 'COMMAND.COM', 0
_TEXT ends
end
