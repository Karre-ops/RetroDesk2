; SETASM.ASM - CPU- und 8087-Erkennung fuer RD2SETUP (aus dem Kern uebernommen), Modell Small
.8086
.8087
_TEXT segment word public 'CODE'
assume cs:_TEXT
public _rd_cpudet
_rd_cpudet proc near
    pushf
    pushf
    pop  ax
    and  ax, 0FFFh
    push ax
    popf
    pushf
    pop  ax
    and  ax, 0F000h
    cmp  ax, 0F000h
    jne  cd_286
    mov  al, 1
    mov  cl, 33
    shl  al, cl
    mov  ax, 0
    jz   cd_ende
    mov  ax, 2
    jmp  short cd_ende
cd_286:
    pushf
    pop  ax
    or   ax, 7000h
    push ax
    popf
    pushf
    pop  ax
    and  ax, 7000h
    mov  ax, 4
    jz   cd_ende
    db 66h, 9Ch                     ; pushfd
    db 66h, 58h                     ; pop eax
    db 66h, 8Bh, 0C8h               ; mov ecx, eax
    db 66h, 35h, 0, 0, 4, 0         ; xor eax, 40000h (AC)
    db 66h, 50h                     ; push eax
    db 66h, 9Dh                     ; popfd
    db 66h, 9Ch                     ; pushfd
    db 66h, 58h                     ; pop eax
    db 66h, 33h, 0C1h               ; xor eax, ecx
    db 66h, 51h                     ; push ecx
    db 66h, 9Dh                     ; popfd
    db 66h, 0C1h, 0E8h, 10h         ; shr eax, 16
    test al, 4
    mov  ax, 5
    jz   cd_ende
    db 66h, 9Ch                     ; pushfd
    db 66h, 58h                     ; pop eax
    db 66h, 8Bh, 0C8h               ; mov ecx, eax
    db 66h, 35h, 0, 0, 20h, 0       ; xor eax, 200000h (ID)
    db 66h, 50h
    db 66h, 9Dh
    db 66h, 9Ch
    db 66h, 58h
    db 66h, 33h, 0C1h
    db 66h, 51h
    db 66h, 9Dh
    db 66h, 0C1h, 0E8h, 10h
    test al, 20h
    mov  ax, 6
    jz   cd_ende
    mov  ax, 7
cd_ende:
    popf
    ret
_rd_cpudet endp

public _rd_fpudet
_rd_fpudet proc near
    push bp
    mov  bp, sp
    sub  sp, 2
    mov  word ptr [bp-2], 0FFFFh
    fninit
    fnstcw [bp-2]
    mov  ax, [bp-2]
    and  ax, 0F3Fh
    cmp  ax, 033Fh
    mov  ax, 0
    jne  fd_no
    inc  ax
fd_no:
    mov  sp, bp
    pop  bp
    ret
_rd_fpudet endp
_TEXT ends
end
