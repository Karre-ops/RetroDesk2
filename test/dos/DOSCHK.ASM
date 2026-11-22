; DOSCHK.COM - Pruefprogramm fuer test/dosstart.sh (PLAN_DOS Schritte 2-4, 22.11.2026). Befehlszeile:
;   H          so viele Dateien oeffnen wie moeglich (NUL), Rueckgabe = Anzahl (vererbte RD2-Dateien kosten Plaetze)
;   K          Taste im Puffer? Rueckgabe 1 = ja, 0 = leer
;   X datei    freies XMS in KB als Dezimalzahl in die Datei, Rueckgabe 0 (1 = kein XMS)
        org  100h
        mov  si, 81h
sk:     lodsb
        cmp  al, ' '
        je   sk
        and  al, 0DFh
        cmp  al, 'H'
        je   mh
        cmp  al, 'K'
        je   mk
        cmp  al, 'X'
        je   mx
        mov  ax, 4CFFh
        int  21h
mh:     xor  bx, bx
mh1:    mov  dx, nul
        mov  ax, 3D00h
        int  21h
        jc   mh2
        inc  bx
        cmp  bx, 250
        jb   mh1
mh2:    mov  al, bl
        mov  ah, 4Ch
        int  21h
mk:     mov  ah, 1
        int  16h
        mov  al, 0
        jz   mk1
        mov  al, 1
mk1:    mov  ah, 4Ch
        int  21h
mx:     lodsb                            ; Dateiname nach Leerzeichen, bis CR
        cmp  al, ' '
        je   mx
        dec  si
        mov  di, si
mx1:    cmp  byte [di], 13
        je   mx2
        cmp  byte [di], ' '
        je   mx2
        inc  di
        jmp  mx1
mx2:    mov  byte [di], 0
        mov  [name], si
        mov  ax, 4300h
        int  2Fh
        cmp  al, 80h
        jne  keinx
        mov  ax, 4310h
        int  2Fh
        mov  [xent], bx
        mov  [xent+2], es
        mov  ah, 8
        call far [xent]
        mov  ax, dx                      ; DX = freies XMS gesamt in KB
        mov  di, zbuf + 6
        mov  byte [di], 13
        mov  byte [di+1], 10
        mov  cx, 10
mx3:    xor  dx, dx
        div  cx
        add  dl, '0'
        dec  di
        mov  [di], dl
        test ax, ax
        jnz  mx3
        push di
        mov  dx, [name]
        xor  cx, cx
        mov  ah, 3Ch
        int  21h
        pop  dx
        jc   keinx
        mov  bx, ax
        mov  cx, zbuf + 8
        sub  cx, dx
        mov  ah, 40h
        int  21h
        mov  ah, 3Eh
        int  21h
        mov  ax, 4C00h
        int  21h
keinx:  mov  ax, 4C01h
        int  21h
nul     db 'NUL', 0
xent    dd 0
name    dw 0
zbuf    times 8 db 0
