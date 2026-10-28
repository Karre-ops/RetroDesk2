; ZTEST.COM - Test fuer den Treiberzeiger (test/rdmaus.sh): fuer jeden Modus aus der Liste Modus setzen,
; INT 33h zuruecksetzen, Zeiger zeigen, auf (320,100) setzen, dann auf eine Taste warten.
; Danach Modus 13h mit Funktion 10h (Bereich 300,90-340,110 um den Zeiger: aus), Taste, Funktion 1 (wieder an), Taste.
; Am Ende Textmodus.
org 100h
    mov  si, modi
nx: lodsb
    cmp  al, 0FFh
    je   fertig
    xor  ah, ah
    int  10h
    xor  ax, ax
    int  33h
    mov  ax, 1
    int  33h
    mov  ax, 4
    mov  cx, 320
    mov  dx, 100
    int  33h
    xor  ah, ah
    int  16h
    mov  ax, 2
    int  33h
    jmp  nx
fertig: mov ax, 13h
    int  10h
    xor  ax, ax
    int  33h
    mov  ax, 1
    int  33h
    mov  ax, 4
    mov  cx, 320
    mov  dx, 100
    int  33h
    mov  ax, 10h
    mov  cx, 300
    mov  dx, 90
    mov  si, 340
    mov  di, 110
    int  33h
    xor  ah, ah
    int  16h
    mov  ax, 1
    int  33h
    xor  ah, ah
    int  16h
    mov ax, 3
    int  10h
    mov  ax, 4C00h
    int  21h
modi db 3, 6, 4, 13h, 12h, 0FFh
