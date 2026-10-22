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
S_MODE  equ 76      ; 0 = auslagern und zurueck, 1 = Neustart (ohne XMS und Festplatte)
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
        jnc  st_frei
        call st_weg
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
st_tot: mov  dx, offset st_tmsg
        mov  ah, 9
        int  21h
        xor  ah, ah
        int  16h
        int  19h
; Neustart: Kern und Arena sind schon frei (RD hat sich beendet). Schleife: Programm,
; dann RD.EXE /W:seg + alte Befehlszeile. Endet dieses RD mit Code 0D5h, hat es hier das
; naechste Programm eingetragen.
st_neu0:
        call st_cat
st_neu: mov  es, ds:[S_PSP]
        mov  bx, 16
        mov  ah, 4Ah
        int  21h
st_lauf:
        call st_prog
        call st_rdneu
        mov  al, 1
        jc   st_ende
        mov  ah, 4Dh
        int  21h
        cmp  ax, 0D5h
        jne  st_ende
        call st_zeile                    ; das neue RD hat Programm und Zeile eingetragen
        jmp  st_lauf
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

; Programm S_PATH starten, Fehler/Code merken, auf Wunsch auf eine Taste warten
st_prog:
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
st_msg  db 13, 10, 'Taste druecken...$'
st_tmsg db 13, 10, 'RD2: Speicher nicht frei (residentes Programm?). Taste: Neustart.$'
_TEXT ends
end
