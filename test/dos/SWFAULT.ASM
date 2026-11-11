; Isolierter FreeDOS-Test: einmaliger Fehler beim dritten 16-KB-Schreiben
; (Teil-Schreiben, echter DOS-Aufruf mit 512 Bytes weniger), oder /R: erster Lesezugriff.
; Nur in wegwerfbaren Test-VMs laden. Keine Veraenderung am laufenden Host/Browser-Gast.
bits 16
org 100h
jmp install
old21: dd 0
left: dw 3
readmode: db 0
swaph: dw 0FFFFh
handler:
    cmp ah,3Ch
    jne io
    push si
    mov si,dx
scan:
    cmp byte [si],0
    je notswap
    cmp word [si],4452h
    jne next
    cmp word [si+2],2E32h
    jne next
    cmp word [si+4],5753h
    jne next
    cmp word [si+6],0050h
    je created
next:
    inc si
    jmp scan
notswap:
    pop si
    jmp chain
created:
    pop si
    pushf
    call far [cs:old21]
    jc return_create
    mov [cs:swaph],ax
return_create:
    retf 2
io:
    cmp bx,[cs:swaph]
    jne chain
    cmp cx,4000h
    jne chain
    cmp byte [cs:readmode],0
    jne readcheck
    cmp ah,40h
    jne chain
    cmp word [cs:left],0
    je chain
    dec word [cs:left]
    jnz chain
    push cx
    sub cx,512
    pushf
    call far [cs:old21]
    pop cx
    retf 2
readcheck:
    cmp ah,3Fh
    jne chain
    cmp word [cs:left],0
    je chain
    mov word [cs:left],0
    mov ax,5
    stc
    retf 2
chain:
    jmp far [cs:old21]
resident_end:
install:
    cmp byte [80h],0
    je hook
    mov byte [readmode],1
hook:
    mov ax,3521h
    int 21h
    mov [old21],bx
    mov [old21+2],es
    mov dx,handler
    mov ax,2521h
    int 21h
    mov dx,(resident_end-$$+100h+15)/16
    mov ax,3100h
    int 21h
