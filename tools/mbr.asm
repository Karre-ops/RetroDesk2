; mbr.asm - Startsektor (MBR) fuer das RD2-USB-Abbild (tools/mkverteil.sh, 23.11.2026). 8086-tauglich.
; Zieht sich nach 0:0600h um, sucht die aktive Partition, laedt deren Startsektor (LBA ueber INT 13h 42h,
; sonst CHS aus dem Eintrag) nach 0:7C00h und springt mit DL = Laufwerk und DS:SI = Partitionseintrag hinein.
; SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING
        bits 16
        org  0x600
        cli
        xor  ax, ax
        mov  ss, ax
        mov  sp, 0x7C00
        mov  ds, ax
        mov  es, ax
        sti
        cld
        mov  si, 0x7C00
        mov  di, 0x600
        mov  cx, 256
        rep  movsw
        jmp  0:weiter
weiter: mov  [drv], dl
        mov  si, 0x600 + 0x1BE
        mov  cx, 4
such:   test byte [si], 0x80
        jnz  gef
        add  si, 16
        loop such
        jmp  fehler
gef:    mov  [teil], si
        mov  ax, [si + 8]
        mov  [dap + 8], ax
        mov  ax, [si + 10]
        mov  [dap + 10], ax
        mov  ah, 0x41
        mov  bx, 0x55AA
        mov  dl, [drv]
        int  0x13
        jc   chs
        cmp  bx, 0xAA55
        jne  chs
        mov  si, dap
        mov  ah, 0x42
        mov  dl, [drv]
        int  0x13
        jnc  ok
chs:    mov  si, [teil]
        mov  dh, [si + 1]
        mov  cx, [si + 2]
        mov  bx, 0x7C00
        mov  ax, 0x0201
        mov  dl, [drv]
        int  0x13
        jc   fehler
ok:     cmp  word [0x7DFE], 0xAA55
        jne  fehler
        mov  si, [teil]
        mov  dl, [drv]
        jmp  0:0x7C00
fehler: mov  si, msg
zeich:  lodsb
        or   al, al
        jz   halt
        mov  ah, 0x0E
        mov  bx, 7
        int  0x10
        jmp  zeich
halt:   jmp  halt
drv     db 0
teil    dw 0
dap     db 16, 0
        dw 1, 0x7C00, 0
        dd 0, 0
msg     db 'RD2: kein startfaehiges System', 0
        times 446 - ($ - $$) db 0
