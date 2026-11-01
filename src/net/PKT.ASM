; RDTCP (RD2): Empfangsroutine (aus RD1 mod/pkt.asm) fuer den Packet-Treiber (wird vom Treiber gerufen)
; Ring aus NSLOT Puffern. rx_len[i] != 0: Puffer i voll, C-Seite leert ihn.
.8086
NSLOT   equ 4
SLOT    equ 1518
DGROUP  group _DATA, _BSS

PKT_TEXT segment word public 'CODE'
        assume cs:PKT_TEXT
        public _pk_recv
; Aufruf 1: AX=0, CX=Laenge -> ES:DI Puffer (0:0 = verwerfen)
; Aufruf 2: AX=1, Puffer gefuellt
_pk_recv proc far
        push ds
        push bx
        push dx
        mov  bx, DGROUP
        mov  ds, bx
        assume ds:DGROUP
        mov  bl, _rx_head
        xor  bh, bh
        or   ax, ax
        jnz  zwei
        cmp  cx, SLOT
        ja   weg
        cmp  cx, 14                 ; kein Ethernet-Kopf: weg (Laenge 0 wuerde den Ring blockieren)
        jb   weg
        shl  bx, 1
        cmp  word ptr _rx_len[bx], 0
        jne  weg
        shr  bx, 1
        mov  _rx_plen, cx
        mov  ax, SLOT
        mul  bx
        add  ax, offset DGROUP:_rx_buf
        mov  di, ax
        mov  ax, ds
        mov  es, ax
        jmp  short raus
weg:    xor  di, di
        mov  es, di
        inc  word ptr _rx_drop
        jmp  short raus
zwei:   mov  ax, _rx_plen
        shl  bx, 1
        mov  _rx_len[bx], ax
        shr  bx, 1
        inc  bl
        cmp  bl, NSLOT
        jb   z2
        xor  bl, bl
z2:     mov  _rx_head, bl
raus:   pop  dx
        pop  bx
        pop  ds
        retf
_pk_recv endp
PKT_TEXT ends

_DATA   segment word public 'DATA'
_DATA   ends
_BSS    segment word public 'BSS'
_BSS    ends
        extrn _rx_buf:byte, _rx_len:word, _rx_head:byte, _rx_plen:word, _rx_drop:word
        end
