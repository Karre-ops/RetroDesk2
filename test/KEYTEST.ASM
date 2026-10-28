; KEYTEST.COM - liest Tasten per INT 16h und schreibt "K ssaa" (Scan, Zeichen) auf COM1, Ende mit Esc.
.8086
_TEXT segment byte public 'CODE'
assume cs:_TEXT, ds:_TEXT
org 100h
start:
    mov  dx, 3FBh
    mov  al, 80h
    out  dx, al
    mov  dx, 3F8h
    mov  al, 12                 ; 9600 Baud
    out  dx, al
    inc  dx
    xor  al, al
    out  dx, al
    mov  dx, 3FBh
    mov  al, 3
    out  dx, al
    mov  si, offset bereit
    call text
lies:
    xor  ah, ah
    int  16h
    push ax
    mov  si, offset kk
    call text
    pop  ax
    push ax
    call hex4
    mov  si, offset nl
    call text
    pop  ax
    cmp  ax, 011Bh
    jne  lies
    mov  ax, 4C00h
    int  21h
hex4:
    mov  cx, 4
h1: rol  ax, 1
    rol  ax, 1
    rol  ax, 1
    rol  ax, 1
    push ax
    and  al, 0Fh
    add  al, '0'
    cmp  al, '9'
    jbe  h2
    add  al, 7
h2: call zei
    pop  ax
    loop h1
    ret
text:
    lodsb
    or   al, al
    jz   t1
    call zei
    jmp  text
t1: ret
zei:
    push dx
    push ax
    mov  dx, 3FDh
z1: in   al, dx
    test al, 20h
    jz   z1
    pop  ax
    mov  dx, 3F8h
    out  dx, al
    pop  dx
    ret
bereit db 'KEYTEST: bereit',13,10,0
kk     db 'K ',0
nl     db 13,10,0
_TEXT ends
end start
