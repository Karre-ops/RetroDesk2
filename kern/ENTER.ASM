; ENTER.ASM - Kern ruft den Einsprung eines RD2-Programms auf (Kern: Modell Small)
; short __cdecl rd_enter(WORD cs, WORD ip, void far *api, WORD hinst, WORD show, char far *cmd)
.8086
_TEXT segment word public 'CODE'
assume cs:_TEXT
public _rd_enter
_rd_enter proc near
    push bp
    mov  bp, sp
    push si
    push di
    push ds
    push es
    push word ptr [bp+4]             ; Segment
    push word ptr [bp+6]             ; Offset  -> dword [bp-12]
    mov  bx, [bp+8]
    mov  es, [bp+10]
    mov  cx, [bp+12]
    mov  dx, [bp+14]
    mov  si, [bp+16]
    mov  di, [bp+18]
    mov  ax, 5232h
    call dword ptr [bp-12]
    add  sp, 4                       ; nur noch SP benutzen (BP koennte veraendert sein)
    pop  es
    pop  ds
    pop  di
    pop  si
    pop  bp
    ret
_rd_enter endp
_TEXT ends
end
