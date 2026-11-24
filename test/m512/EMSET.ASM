; EMSET.COM - Testhilfe (test/fpu_em.sh): setzt CR0.EM wie ein BIOS auf einem 386SX ohne 387. Danach loest jeder
; FPU-Befehl INT 7 aus; RD2 muss das ueber MSW.EM erkennen und darf keinen FPU-Befehl ausfuehren. nasm -f bin
        org 100h
        cpu 386
        mov eax, cr0
        or  al, 4
        mov cr0, eax
        mov dx, t
        mov ah, 9
        int 21h
        mov ax, 4C00h
        int 21h
t       db 'EMSET: CR0.EM gesetzt', 13, 10, '$'
