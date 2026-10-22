;SWAP.ASM - RunDos, Kernseite: Kern anhalten, Rest (RDSWAP.BIN) laufen lassen, danach hier weiter.
.8086
_TEXT segment word public 'CODE'
assume cs:_TEXT
S_RET   equ 20
S_SS    equ 24
S_SP    equ 26
S_DATA  equ 316
; void rd_swaprun(WORD seg): Kern anhalten, Vorlage in seg laufen lassen, danach hier weiter
public _rd_swaprun
_rd_swaprun proc near
        push bp
        mov  bp, sp
        push si
        push di
        push ds
        push es
        mov  es, [bp+4]
        mov  es:[S_SS], ss
        mov  es:[S_SP], sp
        mov  word ptr es:[S_RET], offset sw_back
        mov  es:[S_RET+2], cs
        push es
        mov  ax, S_DATA
        push ax
        retf
sw_back:
        pop  es
        pop  ds
        pop  di
        pop  si
        pop  bp
        ret
_rd_swaprun endp
_TEXT ends
end
