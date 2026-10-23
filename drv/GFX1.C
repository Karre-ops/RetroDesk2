/* GFX1.C - Grafiktreiber fuer 1 Bit pro Pixel: CGA 640x200, Hercules 720x348, VGA 640x480 mono.
 * Eine Quelle, drei Treiber: mit -DCGA, -DHGC oder -DVGAM bauen.
 * Treiber kennen kein Clipping: RDGDI ruft nur mit gueltigen Koordinaten.
 * Farbe 0 = schwarz, 1 = weiss. Muster (8 Bytes) geben die Bits direkt vor. */
#include <i86.h>
#include <conio.h>
#include <string.h>
#include "RD2DRV.H"

#if defined(CGA)
#  define W 640
#  define H 200
#  define BPR 80
#  define VSEG 0xB800
#  define ROW(y) ((((WORD)(y) >> 1) * 80) + (((WORD)(y) & 1) << 13))
#  define NAME "CGA 640x200"
#  define ASPY 240
#elif defined(HGC)
#  define W 720
#  define H 348
#  define BPR 90
#  define VSEG 0xB000
#  define ROW(y) ((((WORD)(y) >> 2) * 90) + (((WORD)(y) & 3) << 13))
#  define NAME "Hercules 720x348"
#  define ASPY 155
#elif defined(VGAM)
#  define W 640
#  define H 480
#  define BPR 80
#  define VSEG 0xA000
#  define ROW(y) ((WORD)(y) * 80)
#  define NAME "VGA 640x480 mono"
#  define ASPY 100
#else
#  error CGA, HGC oder VGAM angeben
#endif

#define VP(x, y) ((BYTE __far *)MK_FP(VSEG, ROW(y) + ((WORD)(x) >> 3)))

static BYTE altmodus;

/* INT 10h direkt: int86() der Watcom-Bibliothek legt REGS auf den Stack und braucht SS = DS,
   Treiber laufen aber mit dem Stack des Aufrufers (-zu) */
WORD int10(WORD ax);
#pragma aux int10 = "push bp" "int 10h" "pop bp" parm [ax] value [ax] modify [bx cx dx si di es]
WORD equip(void);
#pragma aux equip = "int 11h" value [ax]
static BYTE bios_mode(void) { return (BYTE)(int10(0x0F00) & 0x7F); }
static void bios_set(BYTE m) { int10(m); }

#ifdef HGC
static void hgc(int gr)
{
    static const BYTE crt[2][12] = {
        { 0x61, 0x50, 0x52, 0x0F, 0x19, 0x06, 0x19, 0x19, 0x02, 0x0D, 0x0B, 0x0C },   /* Text */
        { 0x35, 0x2D, 0x2E, 0x07, 0x5B, 0x02, 0x57, 0x57, 0x02, 0x03, 0x00, 0x00 } }; /* 720x348 */
    WORD i;
    outp(0x3BF, gr);                              /* Grafik erlauben, nur Seite 0 */
    outp(0x3B8, gr ? 0x02 : 0x20);                /* Bild aus beim Umprogrammieren */
    for (i = 0; i < 12; i++) { outp(0x3B4, i); outp(0x3B5, crt[gr][i]); }
    outp(0x3B8, gr ? 0x0A : 0x28);
    *(BYTE __far *)MK_FP(0x40, 0x49) = (BYTE)(gr ? 6 : 7);   /* Maustreiber: 6 = Hercules-Grafik */
}
#endif

static WORD RDEXPORT d_init(WORD param)
{
    (void)param;
    altmodus = bios_mode();
#if defined(HGC)
    {   /* Hercules: Monochrom-Textmodus und Bit 7 von 3BAh (Strahlruecklauf) wechselt */
        WORD i; BYTE a;
        if (altmodus != 7) return RD_ERR_NOTSUP;
        a = (BYTE)(inp(0x3BA) & 0x80);
        for (i = 0; i < 0x8000U; i++) if ((inp(0x3BA) & 0x80) != a) return 0;
        return RD_ERR_NOTSUP;
    }
#elif defined(VGAM)
    if ((BYTE)int10(0x1A00) != 0x1A) return RD_ERR_NOTSUP;
#else
    /* CGA: VGA/MCGA kann Modus 6 immer; sonst Farbkarte laut BIOS und ein 6845 auf 3D4h
       (Hercules/MDA allein haben keinen) */
    if ((BYTE)int10(0x1A00) != 0x1A) {
        BYTE alt;
        if ((equip() & 0x30) == 0x30) return RD_ERR_NOTSUP;
        outp(0x3D4, 0x0F); alt = (BYTE)inp(0x3D5);
        outp(0x3D5, 0x5A); if ((BYTE)inp(0x3D5) != 0x5A) { outp(0x3D5, alt); return RD_ERR_NOTSUP; }
        outp(0x3D5, alt);
    }
#endif
    if (altmodus > 3 && altmodus != 7) altmodus = 3;
    return 0;
}
static void RDEXPORT d_done(void) { }

static WORD RDEXPORT d_modes(GFXMODE FAR *m, WORD max)
{
    if (max) {
        m->w = W; m->h = H; m->bpp = 1; m->farben = 2;
        m->aspx = 100; m->aspy = ASPY; m->zeilenbytes = BPR; m->flags = 0;
    }
    return 1;
}

static WORD RDEXPORT d_setmode(WORD nr)
{
    (void)nr;
#if defined(HGC)
    hgc(1);
    _fmemset(MK_FP(VSEG, 0), 0, 0x8000U);
#elif defined(VGAM)
    bios_set(0x11);
#else
    bios_set(0x06);
#endif
    return 0;
}

static void RDEXPORT d_textmode(void)
{
#ifdef HGC
    hgc(0);
    bios_set(7);
#else
    bios_set(altmodus);
#endif
}

/* ein Byte mit Maske m und Wert v nach Rasteroperation schreiben */
static void wr(BYTE __far *p, BYTE m, BYTE v, WORD rop)
{
    switch (rop) {
    case ROP_COPY: *p = (BYTE)((*p & ~m) | (v & m)); break;
    case ROP_OR:   *p |= (BYTE)(v & m); break;
    case ROP_AND:  *p &= (BYTE)(v | ~m); break;
    case ROP_XOR:  *p ^= (BYTE)(v & m); break;
    case ROP_NOT:  *p ^= m; break;
    }
}

static void RDEXPORT d_pixel(short x, short y, WORD farbe, WORD rop)
{
    wr(VP(x, y), (BYTE)(0x80 >> (x & 7)), (BYTE)(farbe ? 0xFF : 0), rop);
}

static WORD RDEXPORT d_getpixel(short x, short y)
{
    return (*VP(x, y) >> (7 - (x & 7))) & 1;
}


/* Mittelteil einer Zeile in Assembler: ch = Bytes, cl = Verschiebung, bl = voriges
   Quellbyte; Ziel es:di, Quelle dx:si. Je Art eine Schleife (Text, Symbole, Masken). */
void m_cpy(BYTE __far *d, const BYTE __far *s, WORD n_sh, BYTE prev, BYTE inv);
#pragma aux m_cpy = "push ds" "mov ds,dx" \
    "c1: lodsb" "mov ah,bl" "mov bl,al" "shr ax,cl" "xor al,bh" "stosb" "dec ch" "jnz c1" \
    "pop ds" parm [es di] [dx si] [cx] [bl] [bh] modify [ax bx cx si di]
void m_or(BYTE __far *d, const BYTE __far *s, WORD n_sh, BYTE prev);
#pragma aux m_or = "push ds" "mov ds,dx" \
    "o1: lodsb" "mov ah,bl" "mov bl,al" "shr ax,cl" "or es:[di],al" "inc di" "dec ch" "jnz o1" \
    "pop ds" parm [es di] [dx si] [cx] [bl] modify [ax bx cx si di]
void m_andn(BYTE __far *d, const BYTE __far *s, WORD n_sh, BYTE prev);
#pragma aux m_andn = "push ds" "mov ds,dx" \
    "a1: lodsb" "mov ah,bl" "mov bl,al" "shr ax,cl" "not al" "and es:[di],al" "inc di" "dec ch" "jnz a1" \
    "pop ds" parm [es di] [dx si] [cx] [bl] modify [ax bx cx si di]
void m_xor(BYTE __far *d, const BYTE __far *s, WORD n_sh, BYTE prev);
#pragma aux m_xor = "push ds" "mov ds,dx" \
    "x1: lodsb" "mov ah,bl" "mov bl,al" "shr ax,cl" "xor es:[di],al" "inc di" "dec ch" "jnz x1" \
    "pop ds" parm [es di] [dx si] [cx] [bl] modify [ax bx cx si di]
void m_set(BYTE __far *d, WORD n, BYTE v);
#pragma aux m_set = "rep stosb" parm [es di] [cx] [al] modify [cx di]

void m_xorb(BYTE __far *d, WORD n, BYTE v);
#pragma aux m_xorb = "jcxz e" "l: xor es:[di],al" "inc di" "loop l" "e:" parm [es di] [cx] [al] modify [cx di]
void m_orb(BYTE __far *d, WORD n, BYTE v);
#pragma aux m_orb = "jcxz e" "l: or es:[di],al" "inc di" "loop l" "e:" parm [es di] [cx] [al] modify [cx di]
void m_andb(BYTE __far *d, WORD n, BYTE v);
#pragma aux m_andb = "jcxz e" "l: and es:[di],al" "inc di" "loop l" "e:" parm [es di] [cx] [al] modify [cx di]

/* Rechteck am Stueck (wie RD1 rows()): Zeilenschleife ganz in Assembler, die Zeilenadresse
   wird weitergezaehlt (CGA/HGC: Baenke zu 2000h, VGA: linear). COPY und XOR/NOT schnell,
   OR/AND selten: in C. */
/* Naechste Zeile im Assembler: dx += ZADD, ab ZLIM dx -= ZSUB. Kein #if im _asm-Block
   (Watcom nimmt dort sonst still den #else-Zweig), daher feste Zahlen je Karte. */
#if defined(VGAM)
#  define ZADD 80
#  define ZLIM 0FFFFh
#  define ZSUB 0
#elif defined(CGA)
#  define ZADD 2000h
#  define ZLIM 4000h
#  define ZSUB 3FB0h
#else
#  define ZADD 2000h
#  define ZLIM 8000h
#  define ZSUB 7FA6h
#endif
static BYTE pt[8], b_lm, b_rm, b_one, b_xor;
static WORD b_row, b_col, b_mid, b_n, b_y;
static void RDEXPORT d_box(short x0, short y0, short x1, short y1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    WORD b0 = (WORD)x0 >> 3, n = ((WORD)x1 >> 3) - b0, y, i;
    BYTE lm = (BYTE)(0xFF >> (x0 & 7)), rm = (BYTE)(0xFF << (7 - (x1 & 7))), v = (BYTE)(farbe ? 0xFF : 0);
    if (!n) lm &= rm;
    if (rop == ROP_NOT) { v = 0xFF; rop = ROP_XOR; mu = 0; }
    if (rop == ROP_OR || rop == ROP_AND) {
        for (y = (WORD)y0; y <= (WORD)y1; y++) {
            BYTE __far *p = VP(x0, y);
            if (mu) v = mu[y & 7];
            wr(p, lm, v, rop);
            for (i = 1; i < n; i++) wr(p + i, 0xFF, v, rop);
            if (n) wr(p + n, rm, v, rop);
        }
        return;
    }
    if (!n && !mu) {                              /* eine Spalte (senkrechte Linie): zwei Befehle je Zeile */
        b_row = ROW(y0) + b0; b_n = (WORD)(y1 - y0 + 1);
        b_lm = (BYTE)(rop == ROP_XOR ? 0xFF : ~lm); b_rm = (BYTE)(v & lm);   /* (p UND b_lm) ODER/XOR b_rm */
        b_xor = (BYTE)(rop == ROP_XOR);
        _asm {
            push di
            push es
            mov  ax, VSEG
            mov  es, ax
            mov  di, b_row
            mov  cx, b_n
            mov  al, b_lm
            mov  ah, b_rm
            cmp  b_xor, 0
            jne  sx
        sa: and  es:[di], al
            or   es:[di], ah
            add  di, ZADD
            cmp  di, ZLIM
            jb   s1
            sub  di, ZSUB
        s1: loop sa
            jmp  se
        sx: xor  es:[di], ah
            add  di, ZADD
            cmp  di, ZLIM
            jb   s3
            sub  di, ZSUB
        s3: loop sx
        se: pop  es
            pop  di
        }
        return;
    }
    for (i = 0; i < 8; i++) pt[i] = mu ? mu[i] : v;
    b_lm = lm; b_rm = rm; b_one = (BYTE)!n; b_xor = (BYTE)(rop == ROP_XOR);
    b_row = ROW(y0); b_col = b0; b_mid = n ? n - 1 : 0; b_n = (WORD)(y1 - y0 + 1); b_y = (WORD)y0;
    _asm {
        push si
        push di
        push es
        cld
        mov  ax, VSEG
        mov  es, ax
        mov  dx, b_row
        mov  si, b_y
    zl: mov  bx, si
        and  bx, 7
        mov  al, pt[bx]
        mov  di, dx
        add  di, b_col
        cmp  b_xor, 0
        jne  zx
        mov  ah, es:[di]
        xor  ah, al
        and  ah, b_lm
        xor  es:[di], ah
        cmp  b_one, 0
        jne  zn
        inc  di
        mov  cx, b_mid
        mov  ah, al
        shr  cx, 1
        rep  stosw
        adc  cx, cx
        rep  stosb
        mov  ah, es:[di]
        xor  ah, al
        and  ah, b_rm
        xor  es:[di], ah
        jmp  zn
    zx: mov  ah, al
        and  ah, b_lm
        xor  es:[di], ah
        cmp  b_one, 0
        jne  zn
        inc  di
        mov  cx, b_mid
        mov  ah, al
        shr  cx, 1
        jnc  zq
        xor  es:[di], al
        inc  di
    zq: jcxz zr
    zm: xor  es:[di], ax
        inc  di
        inc  di
        loop zm
    zr: and  al, b_rm
        xor  es:[di], al
    zn: inc  si
        add  dx, ZADD
        cmp  dx, ZLIM
        jb   zw
        sub  dx, ZSUB
    zw:
        dec  b_n
        jz   ze
        jmp  zl
    ze: pop  es
        pop  di
        pop  si
    }
}
/* waagerechte Linie = Rechteck mit einer Zeile */
static void RDEXPORT d_span(short y, short x0, short x1, WORD farbe, const BYTE FAR *mu, WORD rop)
{
    d_box(x0, y, x1, y, farbe, mu, rop);
}

/* Mauszeiger 16x16: mb = 32 Bytes UND-Maske, dann 32 Bytes XOR (je Zeile 2 Bytes).
   Hintergrund (3 Bytes je Zeile) merkt sich der Treiber, cursorhide(1) stellt ihn wieder her. */
static BYTE csv[16][3], c_on, c_h, c_nb;
static WORD c_x, c_y;
static void crest(void)
{
    WORD r, k;
    if (!c_on) return;
    for (r = 0; r < c_h; r++) { BYTE __far *p = VP(c_x, c_y + r); for (k = 0; k < c_nb; k++) p[k] = csv[r][k]; }
    c_on = 0;
}
static void RDEXPORT d_cursorhide(BOOL aus) { if (aus) crest(); }
static void RDEXPORT d_cursor(short x, short y, const BYTE FAR *mb)
{
    WORD r, k, sh = x & 7;
    crest();
    c_x = (WORD)x; c_y = (WORD)y;
    c_h = (BYTE)(H - y < 16 ? H - y : 16);
    c_nb = (BYTE)(BPR - ((WORD)x >> 3) < 3 ? BPR - ((WORD)x >> 3) : 3);
    for (r = 0; r < c_h; r++) {
        BYTE __far *p = VP(x, y + r);
        DWORD a = (0xFF0000FFUL | ((DWORD)((mb[2 * r] << 8) | mb[2 * r + 1]) << 8)) >> sh;
        DWORD o = ((DWORD)((mb[32 + 2 * r] << 8) | mb[33 + 2 * r]) << 8) >> sh;
        BYTE A[3], X[3];
        A[0] = (BYTE)(a >> 16); A[1] = (BYTE)(a >> 8); A[2] = (BYTE)a;
        X[0] = (BYTE)(o >> 16); X[1] = (BYTE)(o >> 8); X[2] = (BYTE)o;
        for (k = 0; k < c_nb; k++) { csv[r][k] = p[k]; p[k] = (BYTE)((p[k] & A[k]) ^ X[k]); }
    }
    c_on = 1;
}

void m_mov(BYTE __far *d, const BYTE __far *s, WORD n);
#pragma aux m_mov = "push ds" "mov ds,dx" "rep movsb" "pop ds" parm [es di] [dx si] [cx] modify [cx si di]

/* ausgerichtete Bloecke: n Zeilen zu k Bytes zwischen Schirm (Zeilen weiterzaehlen) und
   Speicher (Abstand st). hin = 1: Speicher -> Schirm. */
static WORD k_row, k_k, k_n, k_st, k_off, k_seg; static BYTE k_hin;
static void blk(WORD x, WORD y, WORD k, WORD n, const BYTE __far *m, WORD st, BYTE hin)
{
    k_row = ROW(y) + (x >> 3); k_k = k; k_n = n; k_st = st; k_off = FP_OFF(m); k_seg = FP_SEG(m); k_hin = hin;
    if (!k || !n) return;
    _asm {
        push si
        push di
        push es
        push ds
        push bp
        cld
        mov  dx, k_row
        mov  bx, k_off
        mov  ax, k_st
        push ax
        mov  ax, k_n
        push ax
        mov  cx, k_k
        push cx
        cmp  k_hin, 0
        mov  ax, k_seg
        je   gq
        cmp  k_hin, 2            ; Flags ueberstehen mov/push/pop
        mov  es, ax              ; Speicher -> Schirm: ds:si = Speicher, es:di = Schirm
        mov  ax, VSEG
        push es
        mov  es, ax
        pop  ds
        je   il
    hl: mov  si, bx
        mov  di, dx
        pop  cx
        push cx
        shr  cx, 1
        rep  movsw
        adc  cx, cx
        rep  movsb
        call nx
        jnz  hl
        jmp  fe
    il: mov  si, bx              ; hin = 2: invertiert (schwarzer Text auf weiss)
        mov  di, dx
        pop  cx
        push cx
    i1: lodsb
        not  al
        stosb
        loop i1
        call nx
        jnz  il
        jmp  fe
    gq: mov  es, ax              ; Schirm -> Speicher: ds:si = Schirm, es:di = Speicher
        mov  ax, VSEG
        mov  ds, ax
    gl: mov  si, dx
        mov  di, bx
        pop  cx
        push cx
        shr  cx, 1
        rep  movsw
        adc  cx, cx
        rep  movsb
        call nx
        jnz  gl
        jmp  fe
    nx: mov  bp, sp              ; [bp+2] k, [bp+4] n, [bp+6] st
        add  bx, [bp+6]
        add  dx, ZADD
        cmp  dx, ZLIM
        jb   nw
        sub  dx, ZSUB
    nw:
        dec  word ptr [bp+4]
        ret
    fe: add  sp, 6
        pop  bp
        pop  ds
        pop  es
        pop  di
        pop  si
    }
}

/* Kopieren (auch invertiert) mit beliebiger Verschiebung, Raender eingeschlossen, alle Zeilen
   in Assembler. Rahmen auf dem Stapel (ds zeigt in der Schleife auf die Quelle). */
static WORD q_row, q_soff, q_sseg, q_st, q_n, q_mid, q_mask, q_ext; static BYTE q_sh, q_inv;
static void putq(void)
{
    _asm {
        push si
        push di
        push es
        push ds
        push bp
        cld
        mov  ax, q_soff
        push ax
        mov  ax, q_ext
        push ax
        mov  ax, q_mask
        push ax
        mov  ax, q_mid
        push ax
        mov  ax, q_n
        push ax
        mov  ax, q_st
        push ax
        mov  bp, sp
        mov  dx, q_row
        mov  cl, q_sh
        mov  bh, q_inv
        mov  ax, VSEG
        mov  es, ax
        mov  ax, q_sseg
        mov  ds, ax
    rl: mov  si, [bp+10]
        mov  di, dx
        push dx                  ; dx dient in der Mitte als Zwischenspeicher
        xor  bl, bl
        lodsb
        mov  ah, bl
        mov  bl, al
        shr  ax, cl
        xor  al, bh
        cmp  word ptr [bp+4], 0FFFFh
        jne  r2
        mov  ah, [bp+6]
        and  ah, [bp+7]
        call mw
        jmp  rn
    r2: mov  ah, [bp+6]
        call mw
        mov  ch, [bp+4]
        or   ch, ch
        jz   r3
        cmp  byte ptr [bp+9], 0  ; ODER/UND/XOR: eigene Schleifen
        je   rc
        jmp  rm
    rc: test ch, 1
        jz   r7
        lodsb                    ; ungerade: ein Byte vorweg
        mov  ah, bl
        mov  bl, al
        shr  ax, cl
        xor  al, bh
        stosb
        dec  ch
        jz   r3
    r7: shr  ch, 1
    r4: lodsb                    ; je zwei Bytes, als Wort in den Bildspeicher
        mov  ah, bl
        mov  bl, al
        shr  ax, cl
        xor  al, bh
        mov  dl, al
        lodsb
        mov  ah, bl
        mov  bl, al
        shr  ax, cl
        xor  al, bh
        mov  ah, al
        mov  al, dl
        stosw
        dec  ch
        jnz  r4
    r3: cmp  byte ptr [bp+8], 0
        je   r5
        xor  al, al
        jmp  r6
    r5: lodsb
    r6: mov  ah, bl
        shr  ax, cl
        xor  al, bh
        mov  ah, [bp+7]
        call mw
        jmp  rn
    mw: cmp  byte ptr [bp+9], 0  ; Randbyte al unter Maske ah schreiben
        jne  mo
        mov  ch, es:[di]
        xor  ch, al
        and  ch, ah
        xor  es:[di], ch
        inc  di
        ret
    mo: cmp  byte ptr [bp+9], 2
        jne  m1
        not  ah                  ; UND: Maske aussen mit Einsen
        or   al, ah
        and  es:[di], al
        inc  di
        ret
    m1: and  al, ah
        cmp  byte ptr [bp+9], 1
        jne  m3
        or   es:[di], al
        inc  di
        ret
    m3: xor  es:[di], al
        inc  di
        ret
    rm:
        cmp  byte ptr [bp+9], 2  ; ganze Bytes verknuepfen (auch ausgerichtet; Text geht ueber d_glyphs)
        je   ra
        jb   ro
    rz: lodsb
        mov  ah, bl
        mov  bl, al
        shr  ax, cl
        xor  al, bh
        xor  es:[di], al
        inc  di
        dec  ch
        jnz  rz
        jmp  r3
    ro: lodsb
        mov  ah, bl
        mov  bl, al
        shr  ax, cl
        xor  al, bh
        or   es:[di], al
        inc  di
        dec  ch
        jnz  ro
        jmp  r3
    ra: lodsb
        mov  ah, bl
        mov  bl, al
        shr  ax, cl
        xor  al, bh
        and  es:[di], al
        inc  di
        dec  ch
        jnz  ra
        jmp  r3
    rn: pop  dx
        mov  ax, [bp+0]
        add  [bp+10], ax
        add  dx, ZADD
        cmp  dx, ZLIM
        jb   rw
        sub  dx, ZSUB
    rw:
        dec  word ptr [bp+2]
        jz   rx
        jmp  rl
    rx: add  sp, 12
        pop  bp
        pop  ds
        pop  es
        pop  di
        pop  si
    }
}

/* 1-Bit-Quelle (Bit 7 = links) an x,y bringen. Wert v = Quelle (bg gesetzt: invertiert),
   ROP_MASK: gesetzte Bits in fg. Einfarbig und ROP_NOT gehen an d_box, alles andere an
   blk (ausgerichtet kopieren) oder putq (Assembler). */
static void RDEXPORT d_put(short x, short y, short w, short h, const BYTE FAR *src, WORD srcbytes,
                        WORD srcbpp, WORD rop, WORD fg, WORD bg)
{
    WORD sh = x & 7, nb = (sh + w + 7) >> 3, op;
    BYTE rm = (BYTE)(0xFF << (7 - ((x + w - 1) & 7))), inv = (BYTE)(bg ? 0xFF : 0);
    (void)srcbpp;
    if (h <= 0 || w <= 0) return;
    if (rop == ROP_MASK) { op = fg ? 1 : 2; inv = (BYTE)(fg ? 0 : 0xFF); }   /* ODER b / UND ~b */
    else if (rop == ROP_NOT || !fg == !bg) { d_box(x, y, x + w - 1, y + h - 1, fg, 0, rop); return; }
    else if (rop > ROP_XOR) return;
    else op = rop;                                /* ROP_COPY..ROP_XOR = 0..3 wie in putq */
    if (!op && !sh && rm == 0xFF) { blk(x, y, nb, h, src, srcbytes, (BYTE)(inv ? 2 : 1)); return; }
    q_row = ROW(y) + ((WORD)x >> 3); q_soff = FP_OFF(src); q_sseg = FP_SEG(src); q_st = srcbytes; q_n = (WORD)h;
    q_mid = nb == 1 ? 0xFFFF : nb - 2; q_mask = (BYTE)(0xFF >> sh) | ((WORD)rm << 8);
    q_ext = (nb > ((w + 7) >> 3)) | (op << 8); q_sh = (BYTE)sh; q_inv = inv;
    putq();
}

/* Zeichen direkt aus der Schrift, spaltenweise (ein Zeichen nach dem anderen, alle Zeilen),
   leere Glyphenzeilen werden uebersprungen. ror legt die verschobene Zeile auf zwei Bytes. */
static WORD g_toff, g_tseg, g_fseg, g_row, g_h, g_n; static BYTE g_sh, g_op;
static void gly(void)
{
    _asm {
        push si
        push di
        push es
        push ds
        push bp
        mov  ax, g_toff
        push ax
        mov  ax, g_tseg
        push ax
        mov  ax, g_n
        push ax
        mov  ax, g_h
        push ax
        mov  ax, g_fseg
        push ax
        mov  bp, sp
        mov  dx, g_row
        mov  cl, g_sh
        mov  ch, g_op
        mov  ax, VSEG
        mov  es, ax
        cld
    gc: mov  ds, [bp+6]
        mov  si, [bp+8]
        mov  si, [si]
        add  word ptr [bp+8], 2
        mov  ds, [bp+0]
        mov  di, dx
        push dx
        mov  dx, [bp+2]
        cmp  ch, 2
        je   ga
        ja   gx
    go: lodsb
        or   al, al
        jz   go2
        xor  ah, ah
        ror  ax, cl
        or   es:[di], ax
    go2: add di, ZADD
        cmp  di, ZLIM
        jb   go3
        sub  di, ZSUB
    go3: dec dx
        jnz  go
        jmp  gn
    ga: lodsb
        or   al, al
        jz   ga2
        xor  ah, ah
        ror  ax, cl
        not  ax
        and  es:[di], ax
    ga2: add di, ZADD
        cmp  di, ZLIM
        jb   ga3
        sub  di, ZSUB
    ga3: dec dx
        jnz  ga
        jmp  gn
    gx: lodsb
        or   al, al
        jz   gx2
        xor  ah, ah
        ror  ax, cl
        xor  es:[di], ax
    gx2: add di, ZADD
        cmp  di, ZLIM
        jb   gx3
        sub  di, ZSUB
    gx3: dec dx
        jnz  gx
    gn: pop  dx
        inc  dx
        dec  word ptr [bp+4]
        jz   ge
        jmp  gc
    ge: add  sp, 10
        pop  bp
        pop  ds
        pop  es
        pop  di
        pop  si
    }
}
static void RDEXPORT d_glyphs(short x, short y, const WORD FAR *offs, WORD seg, WORD h, WORD n, WORD fg, WORD bg, WORD rop)
{
    (void)bg;
    if (!n || !h) return;
    g_row = ROW(y) + ((WORD)x >> 3); g_toff = FP_OFF(offs); g_tseg = FP_SEG(offs); g_fseg = seg;
    g_h = h; g_n = n; g_sh = (BYTE)(x & 7); g_op = (BYTE)(rop == ROP_XOR ? 3 : fg ? 1 : 2);
    gly();
}

static void RDEXPORT d_get(short x, short y, short w, short h, BYTE FAR *dst, WORD dstbytes)
{
    WORD sh = x & 7, sb = (w + 7) >> 3, i;
    BYTE rm = (BYTE)(0xFF << ((8 - (w & 7)) & 7));
    if (!sh && rm == 0xFF) { blk(x, y, sb, h, dst, dstbytes, 0); return; }
    for (; h > 0; h--, y++, dst += dstbytes) {
        BYTE __far *p = VP(x, y);
        if (!sh) { m_mov(dst, p, sb); dst[sb - 1] &= rm; continue; }
        for (i = 0; i < sb; i++) dst[i] = (BYTE)((p[i] << sh) | (sh ? p[i + 1] >> (8 - sh) : 0));
        dst[sb - 1] &= rm;
    }
}

static GFXDRV tab = {
    { { 'R', 'D', 'R', 'V' }, DRV_GFX, 0x0100, sizeof(GFXDRV), NAME, d_init, d_done },
    d_modes, d_setmode, d_textmode,
    d_span, d_pixel, d_getpixel, d_put, d_get,
    0, 0, 0, d_glyphs, 0, d_cursor, d_cursorhide, 0, d_box
};

FARPROC RDEXPORT DrvTab(void) { return (FARPROC)(void FAR *)&tab; }
static struct { WORD n; FARPROC f[1]; } ex = { 1, { (FARPROC)DrvTab } };
void FAR * RDAPI RdModInit(void) { return &ex; }
