; KB512.COM - Testhilfe (test/min512.sh): laesst DOS nur 512 KB konventionellen Speicher, wie ein PC mit 512 KB.
; Belegt den Speicher oberhalb 512 KB (Strategie "letzter passender Block"), schreibt DOS (8) als Besitzer ein, damit
; der Block nach dem Programmende bleibt, und setzt 40:13 auf 512. nasm -f bin -o KB512.COM KB512.ASM
        org 100h
        int 12h                 ; KB laut BIOS
        cmp ax, 512
        jbe fertig
        sub ax, 512
        mov cl, 6
        shl ax, cl              ; Absaetze oberhalb 512 KB
        dec ax                  ; einer fuer den Speicherblock-Kopf (MCB)
        mov si, ax
        mov ax, 5801h
        mov bx, 2               ; letzter passender Block
        int 21h
        mov ah, 48h
        mov bx, si
        int 21h
        jc fehler
        dec ax
        mov es, ax
        mov word [es:1], 8      ; Besitzer DOS: bleibt nach dem Ende
        push ds
        xor ax, ax
        mov ds, ax
        mov word [413h], 512
        pop ds
        mov ax, 5801h
        xor bx, bx
        int 21h
fertig: mov dx, ok
        jmp aus
fehler: mov ax, 5801h
        xor bx, bx
        int 21h
        mov dx, nein
aus:    mov ah, 9
        int 21h
        mov ax, 4C00h
        int 21h
ok      db 'KB512: nur noch 512 KB', 13, 10, '$'
nein    db 'KB512: FEHLER', 13, 10, '$'
