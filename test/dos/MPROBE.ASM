; MPROBE.COM - Maustest fuer test/busmaus.sh (Bochs, Bus-/InPort-Maus)
; Schreibt auf COM1 (9600 8N1): "R<ax> N<bx>" nach INT 33h/0, dann bei jeder Aenderung
; "X<cx> Y<dx> B<bx>" (Funktion 3), am Ende "P<l> <r> <m>" (Funktion 5, Druecken je Taste).
; Ende nach 6 s ohne Aenderung oder nach 90 s.
; nasm -f bin -o MPROBE.COM MPROBE.ASM
        org 100h
        mov  ax, 00E3h          ; COM1 9600 8N1
        xor  dx, dx
        int  14h
        xor  ax, ax
        int  33h
        mov  [t], bx
        mov  bl, 'R'
        call zahl_bl
        mov  ax, [t]
        mov  bl, 'N'
        call zahl_bl
        call crlf
        mov  ax, 1
        int  33h
        mov  ax, 40h
        mov  es, ax
        mov  ax, [es:6Ch]
        mov  [start], ax
        mov  [zuletzt], ax
schl:   mov  ax, 3
        int  33h
        cmp  cx, [ax_x]
        jne  neu
        cmp  dx, [ax_y]
        jne  neu
        cmp  bx, [ax_b]
        je   gleich
neu:    mov  [ax_x], cx
        mov  [ax_y], dx
        mov  [ax_b], bx
        mov  ax, [es:6Ch]
        mov  [zuletzt], ax
        mov  ax, cx
        mov  bl, 'X'
        call zahl_bl
        mov  ax, [ax_y]
        mov  bl, 'Y'
        call zahl_bl
        mov  ax, [ax_b]
        mov  bl, 'B'
        call zahl_bl
        call crlf
gleich: mov  ax, [es:6Ch]
        sub  ax, [zuletzt]
        cmp  ax, 109            ; 6 s ruhig
        ja   ende
        mov  ax, [es:6Ch]
        sub  ax, [start]
        cmp  ax, 1638           ; 90 s
        jb   schl
ende:   mov  al, 'P'
        call zeichen
        xor  si, si
pl:     mov  ax, 5
        mov  bx, si
        int  33h
        mov  ax, bx
        mov  bl, ' '
        call zahl_bl
        inc  si
        cmp  si, 3
        jb   pl
        call crlf
        mov  ax, 4C00h
        int  21h

zahl_bl: push ax                ; BL = Kennbuchstabe, AX = Zahl (vorzeichenlos dezimal)
        mov  al, bl
        call zeichen
        pop  ax
        mov  cx, 0
        mov  bx, 10
z1:     xor  dx, dx
        div  bx
        push dx
        inc  cx
        or   ax, ax
        jnz  z1
z2:     pop  ax
        add  al, '0'
        call zeichen
        loop z2
        mov  al, ' '
        jmp  zeichen
crlf:   mov  al, 13
        call zeichen
        mov  al, 10
zeichen: push dx                ; AL auf COM1
        push ax
        mov  dx, 3FDh
zw:     in   al, dx
        test al, 20h
        jz   zw
        pop  ax
        mov  dx, 3F8h
        out  dx, al
        pop  dx
        ret

t       dw 0
start   dw 0
zuletzt dw 0
ax_x    dw 0FFFFh
ax_y    dw 0FFFFh
ax_b    dw 0FFFFh
