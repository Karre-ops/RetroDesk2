/* ERZEUGT von tools/mkglyphs.py - nicht von Hand aendern */
static void RDEXPORT d_glyphs(short x, short y, const WORD FAR *offs, WORD fseg, WORD h, WORD n, WORD fg, WORD bg, WORD rop)
{
    WORD row = ROW(y) + ((WORD)x >> 3), tof = FP_OFF(offs), ts = FP_SEG(offs); BYTE sh = (BYTE)(x & 7), op;
    (void)bg;
    if (!n || !h) return;
    op = (BYTE)(rop == ROP_COPY ? (fg ? 4 : 5) : rop == ROP_XOR ? 3 : fg ? 1 : 2);
#if defined(VGAM)
    _asm {
        push si
        push di
        push es
        push ds
        mov  dx, row
        mov  cl, sh
        mov  ax, VSEG
        mov  es, ax
        cld
        push bp
        mov  al, op
        xor  ah, ah
        push ax
        push tof
        push ts
        push n
        push h
        push fseg
        push ax
        mov  bp, sp
        cmp  byte ptr [bp+12], 1
        jne  v1
        or   cl, cl
        jnz  u1x
        jmp  a1
    u1x: jmp  u1
    v1:
        cmp  byte ptr [bp+12], 2
        jne  v2
        or   cl, cl
        jnz  u2x
        jmp  a2
    u2x: jmp  u2
    v2:
        cmp  byte ptr [bp+12], 3
        jne  v3
        or   cl, cl
        jnz  u3x
        jmp  a3
    u3x: jmp  u3
    v3:
        cmp  byte ptr [bp+12], 4
        jne  v4
        or   cl, cl
        jnz  u4x
        jmp  a4
    u4x: jmp  u4
    v4:
        cmp  byte ptr [bp+12], 5
        jne  v5
        or   cl, cl
        jnz  u5x
        jmp  a5
    u5x: jmp  u5
    v5:
        jmp  ende
    a1:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a1r: lodsb
        or   al, al
        jz   s1
        or   es:[di], al
    s1:
        add  di, ZADD
        dec  ch
        jnz  a1r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a1
        jmp  ende
    u1:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    u1r: lodsb
        or   al, al
        jz   z2
        xor  ah, ah
        ror  ax, cl
        or   es:[di], ax
    z2:
        add  di, ZADD
        dec  ch
        jnz  u1r
        inc  dx
        dec  word ptr [bp+6]
        jnz  u1
        jmp  ende
    a2:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a2r: lodsb
        or   al, al
        jz   s3
        not  al
        and  es:[di], al
    s3:
        add  di, ZADD
        dec  ch
        jnz  a2r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a2
        jmp  ende
    u2:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    u2r: lodsb
        or   al, al
        jz   z4
        xor  ah, ah
        ror  ax, cl
        not  ax
        and  es:[di], ax
    z4:
        add  di, ZADD
        dec  ch
        jnz  u2r
        inc  dx
        dec  word ptr [bp+6]
        jnz  u2
        jmp  ende
    a3:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a3r: lodsb
        or   al, al
        jz   s5
        xor  es:[di], al
    s5:
        add  di, ZADD
        dec  ch
        jnz  a3r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a3
        jmp  ende
    u3:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    u3r: lodsb
        or   al, al
        jz   z6
        xor  ah, ah
        ror  ax, cl
        xor  es:[di], ax
    z6:
        add  di, ZADD
        dec  ch
        jnz  u3r
        inc  dx
        dec  word ptr [bp+6]
        jnz  u3
        jmp  ende
    a4:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a4r: lodsb
        mov  es:[di], al
    s7:
        add  di, ZADD
        dec  ch
        jnz  a4r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a4
        jmp  ende
    u4:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  [bp+0], si
        mov  ah, 0FFh
        shr  ah, cl
        not  ah
    f4r: lodsb
        shr  al, cl
        and  es:[di], ah
        or   es:[di], al
        add  di, ZADD
        dec  ch
        jnz  f4r
        inc  dx
        dec  word ptr [bp+6]
        jz   l4
    m4:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  bx, [bp+0]
        mov  [bp+0], si
    m4r: mov  ah, [bx]
        inc  bx
        lodsb
        shr  ax, cl
        mov  es:[di], al
    s8:
        add  di, ZADD
        dec  ch
        jnz  m4r
        inc  dx
        dec  word ptr [bp+6]
        jnz  m4
    l4: mov  bx, [bp+0]
        mov  di, dx
        mov  ch, [bp+4]
        neg  cl
        add  cl, 8
        mov  ah, 0FFh
        shl  ah, cl
        not  ah
    l4r: mov  al, [bx]
        inc  bx
        shl  al, cl
        and  es:[di], ah
        or   es:[di], al
        add  di, ZADD
        dec  ch
        jnz  l4r
        jmp  ende
    a5:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a5r: lodsb
        not  al
        mov  es:[di], al
    s9:
        add  di, ZADD
        dec  ch
        jnz  a5r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a5
        jmp  ende
    u5:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  [bp+0], si
        mov  ah, 0FFh
        shr  ah, cl
    f5r: lodsb
        shr  al, cl
        or   es:[di], ah
        xor  es:[di], al
        add  di, ZADD
        dec  ch
        jnz  f5r
        inc  dx
        dec  word ptr [bp+6]
        jz   l5
    m5:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  bx, [bp+0]
        mov  [bp+0], si
    m5r: mov  ah, [bx]
        inc  bx
        lodsb
        shr  ax, cl
        not  al
        mov  es:[di], al
    s10:
        add  di, ZADD
        dec  ch
        jnz  m5r
        inc  dx
        dec  word ptr [bp+6]
        jnz  m5
    l5: mov  bx, [bp+0]
        mov  di, dx
        mov  ch, [bp+4]
        neg  cl
        add  cl, 8
        mov  ah, 0FFh
        shl  ah, cl
    l5r: mov  al, [bx]
        inc  bx
        shl  al, cl
        or   es:[di], ah
        xor  es:[di], al
        add  di, ZADD
        dec  ch
        jnz  l5r
        jmp  ende
    ende: add  sp, 14
        pop  bp
        pop  ds
        pop  es
        pop  di
        pop  si
    }
#else
    _asm {
        push si
        push di
        push es
        push ds
        mov  dx, row
        mov  cl, sh
        mov  ax, VSEG
        mov  es, ax
        cld
        push bp
        mov  al, op
        xor  ah, ah
        push ax
        push tof
        push ts
        push n
        push h
        push fseg
        push ax
        mov  bp, sp
        cmp  byte ptr [bp+12], 1
        jne  v1
        or   cl, cl
        jnz  u1x
        jmp  a1
    u1x: jmp  u1
    v1:
        cmp  byte ptr [bp+12], 2
        jne  v2
        or   cl, cl
        jnz  u2x
        jmp  a2
    u2x: jmp  u2
    v2:
        cmp  byte ptr [bp+12], 3
        jne  v3
        or   cl, cl
        jnz  u3x
        jmp  a3
    u3x: jmp  u3
    v3:
        cmp  byte ptr [bp+12], 4
        jne  v4
        or   cl, cl
        jnz  u4x
        jmp  a4
    u4x: jmp  u4
    v4:
        cmp  byte ptr [bp+12], 5
        jne  v5
        or   cl, cl
        jnz  u5x
        jmp  a5
    u5x: jmp  u5
    v5:
        jmp  ende
    a1:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a1r: lodsb
        or   al, al
        jz   s1
        or   es:[di], al
    s1:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w2
        sub  di, ZSUB
    w2:
        dec  ch
        jnz  a1r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a1
        jmp  ende
    u1:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    u1r: lodsb
        or   al, al
        jz   z3
        xor  ah, ah
        ror  ax, cl
        or   es:[di], ax
    z3:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w4
        sub  di, ZSUB
    w4:
        dec  ch
        jnz  u1r
        inc  dx
        dec  word ptr [bp+6]
        jnz  u1
        jmp  ende
    a2:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a2r: lodsb
        or   al, al
        jz   s5
        not  al
        and  es:[di], al
    s5:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w6
        sub  di, ZSUB
    w6:
        dec  ch
        jnz  a2r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a2
        jmp  ende
    u2:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    u2r: lodsb
        or   al, al
        jz   z7
        xor  ah, ah
        ror  ax, cl
        not  ax
        and  es:[di], ax
    z7:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w8
        sub  di, ZSUB
    w8:
        dec  ch
        jnz  u2r
        inc  dx
        dec  word ptr [bp+6]
        jnz  u2
        jmp  ende
    a3:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a3r: lodsb
        or   al, al
        jz   s9
        xor  es:[di], al
    s9:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w10
        sub  di, ZSUB
    w10:
        dec  ch
        jnz  a3r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a3
        jmp  ende
    u3:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    u3r: lodsb
        or   al, al
        jz   z11
        xor  ah, ah
        ror  ax, cl
        xor  es:[di], ax
    z11:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w12
        sub  di, ZSUB
    w12:
        dec  ch
        jnz  u3r
        inc  dx
        dec  word ptr [bp+6]
        jnz  u3
        jmp  ende
    a4:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a4r: lodsb
        mov  es:[di], al
    s13:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w14
        sub  di, ZSUB
    w14:
        dec  ch
        jnz  a4r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a4
        jmp  ende
    u4:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  [bp+0], si
        mov  ah, 0FFh
        shr  ah, cl
        not  ah
    f4r: lodsb
        shr  al, cl
        and  es:[di], ah
        or   es:[di], al
        add  di, ZADD
        cmp  di, ZLIM
        jb   w15
        sub  di, ZSUB
    w15:
        dec  ch
        jnz  f4r
        inc  dx
        dec  word ptr [bp+6]
        jz   l4
    m4:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  bx, [bp+0]
        mov  [bp+0], si
    m4r: mov  ah, [bx]
        inc  bx
        lodsb
        shr  ax, cl
        mov  es:[di], al
    s16:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w17
        sub  di, ZSUB
    w17:
        dec  ch
        jnz  m4r
        inc  dx
        dec  word ptr [bp+6]
        jnz  m4
    l4: mov  bx, [bp+0]
        mov  di, dx
        mov  ch, [bp+4]
        neg  cl
        add  cl, 8
        mov  ah, 0FFh
        shl  ah, cl
        not  ah
    l4r: mov  al, [bx]
        inc  bx
        shl  al, cl
        and  es:[di], ah
        or   es:[di], al
        add  di, ZADD
        cmp  di, ZLIM
        jb   w18
        sub  di, ZSUB
    w18:
        dec  ch
        jnz  l4r
        jmp  ende
    a5:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
    a5r: lodsb
        not  al
        mov  es:[di], al
    s19:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w20
        sub  di, ZSUB
    w20:
        dec  ch
        jnz  a5r
        inc  dx
        dec  word ptr [bp+6]
        jnz  a5
        jmp  ende
    u5:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  [bp+0], si
        mov  ah, 0FFh
        shr  ah, cl
    f5r: lodsb
        shr  al, cl
        or   es:[di], ah
        xor  es:[di], al
        add  di, ZADD
        cmp  di, ZLIM
        jb   w21
        sub  di, ZSUB
    w21:
        dec  ch
        jnz  f5r
        inc  dx
        dec  word ptr [bp+6]
        jz   l5
    m5:
        mov  ds, [bp+8]
        mov  si, [bp+10]
        mov  si, [si]
        add  word ptr [bp+10], 2
        mov  ds, [bp+2]
        mov  di, dx
        mov  ch, [bp+4]
        mov  bx, [bp+0]
        mov  [bp+0], si
    m5r: mov  ah, [bx]
        inc  bx
        lodsb
        shr  ax, cl
        not  al
        mov  es:[di], al
    s22:
        add  di, ZADD
        cmp  di, ZLIM
        jb   w23
        sub  di, ZSUB
    w23:
        dec  ch
        jnz  m5r
        inc  dx
        dec  word ptr [bp+6]
        jnz  m5
    l5: mov  bx, [bp+0]
        mov  di, dx
        mov  ch, [bp+4]
        neg  cl
        add  cl, 8
        mov  ah, 0FFh
        shl  ah, cl
    l5r: mov  al, [bx]
        inc  bx
        shl  al, cl
        or   es:[di], ah
        xor  es:[di], al
        add  di, ZADD
        cmp  di, ZLIM
        jb   w24
        sub  di, ZSUB
    w24:
        dec  ch
        jnz  l5r
        jmp  ende
    ende: add  sp, 14
        pop  bp
        pop  ds
        pop  es
        pop  di
        pop  si
    }
#endif
}
