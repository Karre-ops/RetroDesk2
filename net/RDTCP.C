/* RDTCP.C - TCP/IP fuer RD2 (Checkliste Punkt 8), aus RetroDesk 1.x mod/tcp.c, ohne mTCP.
 * Laeuft direkt auf dem Paketreiber: ARP, IPv4 (ohne Fragmente), ICMP-Echo, TCP (Client und Lauschen),
 * UDP, DHCP, DNS. Alle API-Funktionen kehren sofort zurueck, Ergebnisse kommen als Nachricht
 * (M_NETSTATE, M_NETRESOLVED, M_NETCONNECT, M_NETDATA, M_NETCLOSED, M_NETACCEPT).
 * Geladen von RDKX beim ersten NetStart; RdModInit traegt die Exporte in die API-Tabelle ein und
 * haengt sich ueber rd_api[0x3BF] in den Kern: 2 = Pakete bearbeiten (input_poll), 0 = Paketreiber
 * frei (RunDos, Ende von RD), 1 = wieder an. Je Verbindung 4 KB Empfang, 2 KB Senden (MemAlloc, fest).
 * IP-Adressen als DWORD: erstes Byte der Adresse im niedrigsten Byte (wie im Paket).
 * Einstellungen in RD2.INI [Netz]: DHCP=1, IP=, Maske=, Gateway=, DNS=, Paket=0x60 (0 = suchen),
 * MSS=1024, Name=RD2. Die letzte DHCP-Adresse steht in Lease= (gilt weiter, wenn der Server schweigt),
 * ein Zeitserver aus DHCP (Option 42) in NTPDHCP=. */
#include <i86.h>
#include <dos.h>
#include <conio.h>
#include <string.h>
#include "RD2.H"

#define NSLOT 4
#define SLOT  1518
#define NS    8
#define RXB   4096
#define TXB   2048
#define NDNS  4
#define F_FIN 1
#define F_SYN 2
#define F_RST 4
#define F_PSH 8
#define F_ACK 16
enum { S_FREE, S_ARP, S_SYN, S_OPEN, S_FIN, S_DONE, S_ERR, S_LISTEN, S_SYNR, S_UDP };

unsigned char __cdecl rx_buf[NSLOT * SLOT];      /* von PKT.ASM gefuellt */
volatile unsigned __cdecl rx_len[NSLOT];
unsigned __cdecl rx_plen, __cdecl rx_drop;
unsigned char __cdecl rx_head;
extern void __cdecl __far pk_recv(void);
extern FARPROC FAR *_rd2api;

typedef struct {
    BYTE st, udp, wfin, rfin, tries, dups, gem, zu, wnull, lis, mfin;
    HWND w;
    WORD lport, rport, rmss, rwin, rto, tsent, rh, rt;
    BYTE rip[4], rmac[6];
    DWORD una, nxt, end, rcv, iss;
    HMEM hm; BYTE FAR *rx; BYTE FAR *tb;
} SOCK;
static SOCK so[NS];
static struct { BYTE ip[4], mac[6]; } arpc[8];
static WORD arpn, arpw;
static struct { HWND w; WORD id, tries, t0; BYTE st; char name[64]; } dq[NDNS];
static BYTE my_ip[4], mask[4], gw[4], dns[4], my_mac[6], dh_srv[4], dh_ntp[4], lease[20];
static BYTE up, startet, pausiert, busy, dh_st, dh_tries, use_dhcp, dh_type;
static WORD nrx, pvec, h_ip = 0xFFFF, h_arp = 0xFFFF, rx_tail, nport, mss = 1024, fehler = RD_ERR_NOTSUP, dh_t0, ipid, dnsid;
static DWORD dh_xid;
static HWND warte[4];
static BYTE tx[SLOT], dhb[16];
static char ini[80], hname[16], sb[96];
static BYTE ipb[4], dmac[6];
static const BYTE bcast[6] = { 255, 255, 255, 255, 255, 255 };
static const BYTE ip_bc[4] = { 255, 255, 255, 255 };
static void (__far __pascal * FAR *haken)(WORD);
static FARPROC alt[10];

/* ---------------- Hilfen ---------------- */
static WORD ticks(void)
{
    volatile DWORD __far *p = (volatile DWORD __far *)MK_FP(0x40, 0x6C); DWORD v;
    do v = *p; while (v != *p);
    return (WORD)v;
}
static void put16(BYTE *p, WORD v) { p[0] = (BYTE)(v >> 8); p[1] = (BYTE)v; }
static WORD get16(const BYTE *p) { return (p[0] << 8) | p[1]; }
static void put32(BYTE *p, DWORD v) { put16(p, (WORD)(v >> 16)); put16(p + 2, (WORD)v); }
static DWORD get32(const BYTE *p) { return ((DWORD)get16(p) << 16) | get16(p + 2); }
static DWORD ipd(const BYTE *ip) { return (DWORD)ip[0] | ((DWORD)ip[1] << 8) | ((DWORD)ip[2] << 16) | ((DWORD)ip[3] << 24); }
static void dip(BYTE *ip, DWORD v) { ip[0] = (BYTE)v; ip[1] = (BYTE)(v >> 8); ip[2] = (BYTE)(v >> 16); ip[3] = (BYTE)(v >> 24); }
static BYTE null4(const BYTE *ip) { return !(ip[0] | ip[1] | ip[2] | ip[3]); }

unsigned csum16(const void *b, unsigned n);
#pragma aux csum16 = \
    "xor bx, bx" "shr cx, 1" "pushf" "jcxz L2" "clc" \
    "L1: lodsw" "adc bx, ax" "loop L1" "adc bx, 0" "adc bx, 0" \
    "L2: popf" "jnc L3" "lodsb" "xor ah, ah" "add bx, ax" "adc bx, 0" "adc bx, 0" \
    "L3:" parm [si] [cx] value [bx] modify [ax cx si];
static DWORD cadd(DWORD s, const BYTE *b, WORD n) { WORD v = csum16(b, n); return s + (WORD)((v << 8) | (v >> 8)); }
static WORD cfin(DWORD s) { while (s >> 16) s = (s & 0xFFFF) + (s >> 16); return (WORD)~s; }

static BYTE ip_parse(LPCSTR s, BYTE *ip)          /* "1.2.3.4" -> 1 */
{
    WORD i, v;
    for (i = 0; i < 4; i++) {
        if (*s < '0' || *s > '9') return 0;
        for (v = 0; *s >= '0' && *s <= '9'; s++) v = v * 10 + *s - '0';
        if (v > 255) return 0;
        ip[i] = (BYTE)v;
        if (i < 3 && *s++ != '.') return 0;
    }
    return *s == 0 || *s == ' ' || *s == '/';
}
static char *ipstr(char *o, const BYTE *ip)
{
    WORD i, v;
    for (i = 0; i < 4; i++) {
        v = ip[i];
        if (v >= 100) *o++ = (char)('0' + v / 100);
        if (v >= 10) *o++ = (char)('0' + v / 10 % 10);
        *o++ = (char)('0' + v % 10);
        *o++ = (char)(i < 3 ? '.' : 0);
    }
    return o - 1;
}
static void post(HWND w, WORD m, WORD p1, DWORD p2) { if (w) PostMessage(w, m, p1, p2); }

/* ---------------- Paketreiber ---------------- */
/* Aufruf des Paketreibers ohne int86x (das geht im Modul mit SS != DS nicht): pushf + call far auf den Vektor */
static WORD ra, rb, rc, rd, rsi, rdi, res; static BYTE rcf; static DWORD pvp;
static BYTE pk(void)
{
    _asm {
        push bp
        push si
        push di
        push es
        push ds
        mov  ax, res
        mov  es, ax
        mov  bx, rb
        mov  cx, rc
        mov  dx, rd
        mov  si, rsi
        mov  di, rdi
        mov  ax, ra
        pushf
        call dword ptr pvp
        pushf
        push ax
        mov  bp, sp
        mov  ds, [bp+4]
        pop  ax
        mov  ra, ax
        pop  ax
        and  al, 1
        mov  rcf, al
        mov  rb, bx
        mov  rc, cx
        mov  rd, dx
        pop  ds
        pop  es
        pop  di
        pop  si
        pop  bp
    }
    return !rcf;
}
static WORD dseg(void) { return FP_SEG((void __far *)tx); }
static WORD pk_access(WORD typ)
{
    static BYTE t[2][2]; static WORD k;
    BYTE *tp = t[k++ & 1];
    put16(tp, typ);
    ra = 0x0201; rb = 0xFFFF; rd = 0; rsi = (unsigned)tp; rc = 2;
    res = FP_SEG(pk_recv); rdi = FP_OFF(pk_recv);
    return pk() ? ra : 0xFFFF;
}
static void pk_release(WORD h) { if (h == 0xFFFF) return; ra = 0x0300; rb = h; pk(); }
static void pk_send(WORD n)
{
    if (n < 60) { memset(tx + n, 0, 60 - n); n = 60; }
    ra = 0x0400; rsi = (unsigned)tx; rc = n; pk();
}
static WORD find_pkt(WORD want)
{
    WORD i; char __far *p;
    for (i = want ? want : 0x60; i <= (want ? want : 0x80); i++) {
        p = *(char __far * __far *)MK_FP(0, i * 4);
        if (p && _fmemcmp(p + 3, "PKT DRVR", 8) == 0) { pvp = (DWORD)p; return i; }
    }
    return 0;
}
static BYTE pk_an(void)                           /* Typen IP und ARP anmelden, MAC holen */
{
    if ((h_ip = pk_access(0x0800)) == 0xFFFF || (h_arp = pk_access(0x0806)) == 0xFFFF) { pk_release(h_ip); h_ip = h_arp = 0xFFFF; return 0; }
    ra = 0x0600; rb = h_ip; res = dseg(); rdi = (unsigned)my_mac; rc = 6;
    return pk();
}
static void pk_aus(void) { pk_release(h_ip); pk_release(h_arp); h_ip = h_arp = 0xFFFF; }

/* ---------------- Ethernet, ARP, IP ---------------- */
static void eth(const BYTE *dm, WORD typ) { memcpy(tx, dm, 6); memcpy(tx + 6, my_mac, 6); put16(tx + 12, typ); }
static void arp_send(WORD op, const BYTE *tm, const BYTE *tip)
{
    BYTE *a = tx + 14;
    eth(op == 1 ? bcast : tm, 0x0806);
    put16(a, 1); put16(a + 2, 0x0800); a[4] = 6; a[5] = 4; put16(a + 6, op);
    memcpy(a + 8, my_mac, 6); memcpy(a + 14, my_ip, 4);
    if (op == 1) memset(a + 18, 0, 6); else memcpy(a + 18, tm, 6);
    memcpy(a + 24, tip, 4);
    pk_send(42);
}
static void arp_put(const BYTE *ip, const BYTE *mac, BYTE neu)
{
    WORD i;
    for (i = 0; i < arpn; i++) if (!memcmp(arpc[i].ip, ip, 4)) { memcpy(arpc[i].mac, mac, 6); return; }
    if (!neu) return;
    if (arpn < 8) i = arpn++; else { i = arpw; arpw = (arpw + 1) & 7; }
    memcpy(arpc[i].ip, ip, 4); memcpy(arpc[i].mac, mac, 6);
}
static void arp_in(const BYTE *a)
{
    if (get16(a) != 1 || get16(a + 2) != 0x0800) return;
    if (!memcmp(a + 24, my_ip, 4)) { arp_put(a + 14, a + 8, 1); if (get16(a + 6) == 1) arp_send(2, a + 8, a + 14); }
    else arp_put(a + 14, a + 8, 0);
}
static const BYTE *hop(const BYTE *ip)            /* naechster Sprung: eigenes Netz direkt, sonst Gateway */
{
    WORD i;
    for (i = 0; i < 4; i++) if ((ip[i] ^ my_ip[i]) & mask[i]) return gw;
    return ip;
}
static BYTE mac_von(const BYTE *ip, BYTE *mac, BYTE fragen)   /* 1 = bekannt; sonst ARP-Anfrage */
{
    const BYTE *h; WORD i;
    if (!memcmp(ip, ip_bc, 4)) { memcpy(mac, bcast, 6); return 1; }
    h = hop(ip);
    if (null4(h)) return 0;
    for (i = 0; i < arpn; i++) if (!memcmp(arpc[i].ip, h, 4)) { memcpy(mac, arpc[i].mac, 6); return 1; }
    if (fragen) arp_send(1, 0, h);
    return 0;
}
static void ip_out(const BYTE *ip, const BYTE *mac, BYTE proto, WORD n)
{
    BYTE *h = tx + 14;
    eth(mac, 0x0800);
    h[0] = 0x45; h[1] = 0; put16(h + 2, 20 + n); put16(h + 4, ++ipid); put16(h + 6, 0x4000);
    h[8] = 64; h[9] = proto; h[10] = h[11] = 0;
    memcpy(h + 12, my_ip, 4); memcpy(h + 16, ip, 4);
    put16(h + 10, cfin(cadd(0, h, 20)));
    pk_send(34 + n);
}
static void udp_out(const BYTE *ip, const BYTE *mac, WORD sp, WORD dp, WORD n)   /* Daten ab tx + 42 */
{
    BYTE *u = tx + 34; DWORD s;
    put16(u, sp); put16(u + 2, dp); put16(u + 4, 8 + n); u[6] = u[7] = 0;
    s = cadd(cadd(0, my_ip, 4), ip, 4) + 17 + 8 + n;
    s = cfin(cadd(s, u, 8 + n)); put16(u + 6, (WORD)s ? (WORD)s : 0xFFFF);
    ip_out(ip, mac, 17, 8 + n);
}

/* ---------------- Einstellungen, DHCP ---------------- */
static WORD hexzahl(LPCSTR s)
{
    WORD v = 0;
    if (s[0] == '0' && (s[1] | 32) == 'x') s += 2;
    for (;; s++) {
        if (*s >= '0' && *s <= '9') v = v * 16 + *s - '0';
        else if ((*s | 32) >= 'a' && (*s | 32) <= 'f') v = v * 16 + (*s | 32) - 'a' + 10;
        else return v;
    }
}
static void ini_ip(LPCSTR k, BYTE *ip) { ProfileGetStr(ini, "Netz", k, "", sb, sizeof sb); if (!ip_parse(sb, ip)) memset(ip, 0, 4); }
static void lease_lesen(void)                     /* Lease=IP Maske Gateway DNS Server */
{
    char *p = sb; WORD i;
    ProfileGetStr(ini, "Netz", "Lease", "", sb, sizeof sb);
    memset(lease, 0, sizeof lease);
    for (i = 0; i < 5 && *p; i++) {
        if (!ip_parse(p, lease + i * 4)) { memset(lease, 0, sizeof lease); return; }
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
    }
}
static void lease_schreiben(void)
{
    char *p = sb;
    p = ipstr(p, my_ip); *p++ = ' '; p = ipstr(p, mask); *p++ = ' '; p = ipstr(p, gw); *p++ = ' ';
    p = ipstr(p, dns); *p++ = ' '; ipstr(p, dh_srv);
    ProfileSetStr(ini, "Netz", "Lease", sb);
    if (!null4(dh_ntp)) { ipstr(sb, dh_ntp); ProfileSetStr(ini, "Netz", "NTPDHCP", sb); }
}
static void dhcp_send(BYTE typ, const BYTE *want, const BYTE *srv)
{
    BYTE *b = tx + 42, *o; WORD n;
    memset(b, 0, 300);
    b[0] = 1; b[1] = 1; b[2] = 6;
    put32(b + 4, dh_xid); put16(b + 10, 0x8000);
    memcpy(b + 28, my_mac, 6);
    b[236] = 99; b[237] = 130; b[238] = 83; b[239] = 99;
    o = b + 240;
    *o++ = 53; *o++ = 1; *o++ = typ;
    *o++ = 61; *o++ = 7; *o++ = 1; memcpy(o, my_mac, 6); o += 6;
    if (want) { *o++ = 50; *o++ = 4; memcpy(o, want, 4); o += 4; }
    if (srv)  { *o++ = 54; *o++ = 4; memcpy(o, srv, 4); o += 4; }
    if (hname[0]) { n = strlen(hname); *o++ = 12; *o++ = (BYTE)n; memcpy(o, hname, n); o += n; }
    *o++ = 55; *o++ = 4; *o++ = 1; *o++ = 3; *o++ = 6; *o++ = 42;
    *o = 255;
    memset(my_ip, 0, 4);
    udp_out(ip_bc, bcast, 68, 67, 300);
    dh_t0 = ticks();
}
static void dhcp_in(const BYTE *b, WORD n)
{
    const BYTE *o, *e = b + n; BYTE t = 0;
    if (n < 244 || b[0] != 2 || get32(b + 4) != dh_xid || memcmp(b + 28, my_mac, 6)) return;
    if (b[236] != 99 || b[237] != 130 || b[238] != 83 || b[239] != 99) return;
    memcpy(dhb, b + 16, 4); memset(dhb + 4, 0, 12);
    for (o = b + 240; o < e && *o != 255; o += 2 + o[1]) {
        if (!*o) { o--; continue; }
        if (o + 2 > e || o + 2 + o[1] > e) break;
        if (o[1] >= 4) {
            if (*o == 1) memcpy(dhb + 4, o + 2, 4);
            else if (*o == 3) memcpy(dhb + 8, o + 2, 4);
            else if (*o == 6) memcpy(dhb + 12, o + 2, 4);
            else if (*o == 54 && dh_st == 1) memcpy(dh_srv, o + 2, 4);
            else if (*o == 42) memcpy(dh_ntp, o + 2, 4);
        }
        if (*o == 53 && o[1]) t = o[2];
    }
    dh_type = t;
}
static void fertig(WORD e)                        /* Start beendet: alle Wartenden benachrichtigen */
{
    WORD i;
    startet = 0; fehler = e; up = e == 0;
    if (!up) pk_aus();
    for (i = 0; i < 4; i++) if (warte[i]) { post(warte[i], M_NETSTATE, up ? NS_OPEN : NS_ERROR, e); warte[i] = 0; }
    if (up) { nport = 1024 + (ticks() & 0x3FFF); DbgPrint("RDTCP: Netz bereit"); ipstr(sb, my_ip); DbgPrint(sb); }
    else { DbgHex("RDTCP: Netz Fehler", e); DbgHex("RDTCP: Pakete", nrx); DbgHex("RDTCP: verworfen", rx_drop); DbgHex("RDTCP: Treiber", pvec); }
}
static void dhcp_takt(void)                       /* 1 DISCOVER gesendet, 2 REQUEST gesendet, 3 REQUEST alte Adresse */
{
    WORD t = ticks() - dh_t0;
    if (dh_type == 2 && dh_st == 1) { memcpy(ipb, dhb, 4); dh_st = 2; dh_tries = 0; dh_type = 0; dhcp_send(3, ipb, dh_srv); return; }
    if (dh_type == 5 && (dh_st == 2 || dh_st == 3)) {
        memcpy(my_ip, dhb, 4); memcpy(mask, dhb + 4, 4); memcpy(gw, dhb + 8, 4); memcpy(dns, dhb + 12, 4);
        if (!mask[0]) mask[0] = mask[1] = mask[2] = 255;
        dh_st = 0; lease_schreiben(); fertig(0); return;
    }
    if (dh_type == 6) { dh_st = 1; dh_tries = 0; dh_type = 0; dhcp_send(1, 0, 0); return; }   /* NAK: neu anfragen */
    dh_type = 0;
    if (t < (dh_tries < 2 ? 36 : 72)) return;
    if (++dh_tries < 4) { if (dh_st == 1) dhcp_send(1, 0, 0); else if (dh_st == 2) dhcp_send(3, ipb, dh_srv); else dhcp_send(3, lease, 0); return; }
    if (dh_st == 3) { dh_st = 1; dh_tries = 0; dhcp_send(1, 0, 0); return; }
    dh_st = 0;
    if (!null4(lease)) {                          /* Server schweigt: alte Adresse gilt weiter */
        memcpy(my_ip, lease, 4); memcpy(mask, lease + 4, 4); memcpy(gw, lease + 8, 4); memcpy(dns, lease + 12, 4);
        fertig(0); return;
    }
    fertig(RD_ERR_TIMEOUT);
}

/* ---------------- TCP ---------------- */
static WORD rfrei(SOCK *c) { return RXB - (c->rh - c->rt); }
static WORD rwin(SOCK *c) { WORD f = rfrei(c); return f < mss ? 0 : f; }
static void tcp_out(SOCK *c, BYTE fl, WORD n, DWORD seq)   /* n Bytes ab seq aus dem Sendepuffer */
{
    BYTE *t = tx + 34; WORD hl = (fl & F_SYN) ? 24 : 20, i, w; DWORD s;
    put16(t, c->lport); put16(t + 2, c->rport); put32(t + 4, seq);
    put32(t + 8, (fl & F_ACK) ? c->rcv : 0);
    t[12] = (BYTE)((hl / 4) << 4); t[13] = fl;
    w = c->udp ? 0 : rwin(c); c->wnull = w == 0;
    put16(t + 14, w); t[16] = t[17] = t[18] = t[19] = 0;
    if (fl & F_SYN) { t[20] = 2; t[21] = 4; put16(t + 22, mss); }
    if (n) {
        i = (WORD)seq & (TXB - 1);
        if (n > TXB - i) { _fmemcpy(t + hl, c->tb + i, TXB - i); _fmemcpy(t + hl + (TXB - i), c->tb, n - (TXB - i)); }
        else _fmemcpy(t + hl, c->tb + i, n);
    }
    s = cadd(cadd(0, my_ip, 4), c->rip, 4) + 6 + hl + n;
    put16(t + 16, cfin(cadd(s, t, hl + n)));
    ip_out(c->rip, c->rmac, 6, hl + n);
}
static void senden(SOCK *c)                       /* was das Fenster erlaubt; danach ggf. FIN */
{
    WORD n;
    if (c->st != S_OPEN) return;
    while (c->nxt != c->end) {
        DWORD fl = c->nxt - c->una;
        if (fl >= c->rwin) break;
        n = (WORD)(c->end - c->nxt);
        if (n > c->rmss) n = c->rmss;
        if (n > c->rwin - fl) n = (WORD)(c->rwin - fl);
        if (c->una == c->nxt) c->tsent = ticks();
        tcp_out(c, F_ACK | F_PSH, n, c->nxt); c->nxt += n;
    }
    if (c->wfin && c->nxt == c->end) {
        c->st = S_FIN; c->tsent = ticks(); c->tries = 0;
        tcp_out(c, F_ACK | F_FIN, 0, c->end); c->nxt = c->end + 1;
    }
}
static void wiederholen(SOCK *c)
{
    c->tsent = ticks();
    if (c->st == S_SYN) tcp_out(c, F_SYN, 0, c->iss);
    else if (c->st == S_SYNR) tcp_out(c, F_SYN | F_ACK, 0, c->iss);
    else if (c->st == S_OPEN && c->una != c->nxt) { c->nxt = c->una; senden(c); }
    else if (c->st == S_FIN) {
        if ((long)(c->end - c->una) > 0) {        /* Daten vor dem FIN fehlen noch */
            WORD n = (WORD)(c->end - c->una); if (n > c->rmss) n = c->rmss;
            tcp_out(c, F_ACK | F_PSH, n, c->una);
        } else tcp_out(c, F_ACK | F_FIN, 0, c->end);
    }
}
static void frei(SOCK *c)
{
    if (c->hm) { MemUnlock(c->hm); MemFree(c->hm); }
    memset(c, 0, sizeof *c);
}
static SOCK *neu(HWND w, BYTE udp)
{
    WORD i; SOCK *c; HMEM h;
    for (i = 0; i < NS && so[i].st != S_FREE; i++) ;
    if (i == NS) { SetLastError(RD_ERR_BUSY); return 0; }
    if (!(h = MemAlloc(RXB + TXB, MEM_FIXED | MEM_SHARE))) { SetLastError(RD_ERR_NOMEM); return 0; }
    c = &so[i]; memset(c, 0, sizeof *c);
    c->hm = h; c->rx = (BYTE FAR *)MemLock(h); c->tb = c->rx + RXB; c->w = w; c->udp = udp;
    if (++nport < 1024 || nport > 60000U) nport = 1024;
    c->lport = nport; c->rmss = 536; c->rto = 18; c->rwin = 0;
    return c;
}
static WORD hnd(SOCK *c) { return (WORD)(c - so) + 1; }
static void schliessen_melden(SOCK *c, WORD e) { if (!c->mfin) { c->mfin = 1; post(c->w, M_NETCLOSED, hnd(c), e); } }
static void fehl(SOCK *c, WORD e)
{
    BYTE war = c->st;
    c->st = S_ERR;
    if (war == S_ARP || war == S_SYN) post(c->w, M_NETCONNECT, hnd(c), e); else schliessen_melden(c, e);
    if (c->zu) frei(c);
}
static void verbunden(SOCK *c)
{
    if (c->udp) { c->st = S_UDP; post(c->w, M_NETCONNECT, hnd(c), 0); return; }
    c->iss = ((DWORD)ticks() << 16) | c->lport; c->una = c->iss; c->nxt = c->end = c->iss + 1;
    c->st = S_SYN; c->tries = 0; wiederholen(c);
}
static void rx_rein(SOCK *c, const BYTE *d, WORD k)
{
    WORD i = c->rh & (RXB - 1);
    if (k > RXB - i) { _fmemcpy(c->rx + i, d, RXB - i); _fmemcpy(c->rx, d + (RXB - i), k - (RXB - i)); }
    else _fmemcpy(c->rx + i, d, k);
    c->rh += k;
}
static void melde_daten(SOCK *c) { if (!c->gem && c->rh != c->rt) { c->gem = 1; post(c->w, M_NETDATA, hnd(c), (DWORD)(WORD)(c->rh - c->rt)); } }
static void tcp_in(const BYTE *f, const BYTE *ih, const BYTE *t, WORD n)
{
    DWORD s, seq, ack; WORD hl, dl, i, k; BYTE fl; SOCK *c, *l = 0; const BYTE *d;
    if (n < 20) return;
    s = cadd(0, ih + 12, 8) + 6 + n;
    if (cfin(cadd(s, t, n))) return;
    for (i = 0; i < NS; i++) {
        c = &so[i];
        if (c->st == S_LISTEN && get16(t + 2) == c->lport) l = c;
        if ((c->st >= S_SYN && c->st <= S_FIN) || c->st == S_SYNR)
            if (!c->udp && get16(t) == c->rport && get16(t + 2) == c->lport && !memcmp(ih + 12, c->rip, 4)) break;
    }
    seq = get32(t + 4); ack = get32(t + 8); hl = (t[12] >> 4) * 4; fl = t[13];
    if (hl < 20 || hl > n) return;
    if (i == NS) {                                /* neue Verbindung an einen Lauscher */
        if (!l || (fl & (F_SYN | F_ACK | F_RST)) != F_SYN || !(c = neu(l->w, 0))) return;
        memcpy(c->rip, ih + 12, 4); memcpy(c->rmac, f + 6, 6); c->rport = get16(t); c->lport = l->lport;
        c->lis = (BYTE)hnd(l); c->rcv = seq + 1; c->rwin = get16(t + 14);
        for (k = 20; k + 3 < hl; ) { if (!t[k]) break; if (t[k] == 1) { k++; continue; } if (t[k] == 2 && t[k + 1] == 4) c->rmss = get16(t + k + 2); if (t[k + 1] < 2) break; k += t[k + 1]; }
        if (c->rmss > mss) c->rmss = mss;
        c->iss = ((DWORD)ticks() << 16) ^ seq; c->una = c->iss; c->nxt = c->end = c->iss + 1;
        c->st = S_SYNR; wiederholen(c); return;
    }
    d = t + hl; dl = n - hl;
    if (fl & F_RST) { fehl(c, RD_ERR_NET); return; }
    if (c->st == S_SYN) {
        if ((fl & (F_SYN | F_ACK)) != (F_SYN | F_ACK) || ack != c->nxt) return;
        for (k = 20; k + 3 < hl; ) { if (!t[k]) break; if (t[k] == 1) { k++; continue; } if (t[k] == 2 && t[k + 1] == 4) c->rmss = get16(t + k + 2); if (t[k + 1] < 2) break; k += t[k + 1]; }
        if (c->rmss > mss) c->rmss = mss;
        c->rcv = seq + 1; c->una = ack; c->rwin = get16(t + 14);
        c->st = S_OPEN; c->tries = 0; c->rto = (ticks() - c->tsent) * 2 + 4;
        tcp_out(c, F_ACK, 0, c->nxt);
        post(c->w, M_NETCONNECT, hnd(c), 0);
        senden(c); return;
    }
    if (c->st == S_SYNR) {
        if (!(fl & F_ACK) || ack != c->nxt) return;
        c->st = S_OPEN; c->una = ack; c->tries = 0; c->rwin = get16(t + 14);
        post(c->w, M_NETACCEPT, hnd(c), c->lis);
    }
    if (fl & F_ACK) {
        if ((long)(ack - c->una) > 0 && (long)(ack - c->nxt) <= 0) {
            BYTE rec = c->tries;
            if (!rec) c->rto = (ticks() - c->tsent) * 2 + 4;
            if (c->rto > 128) c->rto = 128;
            c->una = ack; c->tries = 0; c->dups = 0; c->tsent = ticks();
        } else if (ack == c->una && !dl && c->una != c->nxt && c->st == S_OPEN && ++c->dups == 3) wiederholen(c);
        c->rwin = get16(t + 14);
    }
    if (dl || (fl & F_FIN)) {
        s = c->rcv - seq;
        if ((long)s > 0 && s < dl && !c->rfin) { d += (WORD)s; dl -= (WORD)s; seq = c->rcv; }
        if (seq == c->rcv && !c->rfin) {
            k = dl < rfrei(c) ? dl : rfrei(c);
            if (k) { rx_rein(c, d, k); c->rcv += k; melde_daten(c); }
            if ((fl & F_FIN) && k == dl) { c->rcv++; c->rfin = 1; schliessen_melden(c, 0); }
        }
        tcp_out(c, F_ACK, 0, c->st == S_FIN ? c->end + 1 : c->nxt);
    }
    if (c->st == S_FIN && c->una == c->end + 1 && c->rfin) { c->st = S_DONE; if (c->zu) frei(c); }
    else senden(c);
}
static void udp_in(const BYTE *ih, const BYTE *u, WORD n)
{
    WORD i, dp = get16(u + 2), m;
    if (n < 8) return;
    m = get16(u + 4); if (m < 8 || m > n) return;
    m -= 8;
    if (dp == 68 && dh_st) { dhcp_in(u + 8, m); return; }
    for (i = 0; i < NDNS; i++)                    /* DNS-Antwort */
        if (dq[i].st && dp == 0xF000 + i && m >= 12 && get16(u + 8) == dq[i].id) {
            const BYTE *b = u + 8, *p = b + 12, *e = b + m; WORD an = get16(b + 6), k;
            if ((b[3] & 15) || !an) { post(dq[i].w, M_NETRESOLVED, RD_ERR_NOTFOUND, 0); dq[i].st = 0; return; }
            while (p < e && *p) { if (*p >= 0xC0) { p++; break; } p += *p + 1; }
            p += 5;                               /* Ende des Namens, Typ, Klasse */
            for (k = 0; k < an && p + 12 <= e; k++) {
                if (*p >= 0xC0) p += 2; else { while (p < e && *p) p += *p + 1; p++; }
                if (p + 10 > e) break;
                if (get16(p) == 1 && get16(p + 8) == 4 && p + 14 <= e) { post(dq[i].w, M_NETRESOLVED, 0, ipd(p + 10)); dq[i].st = 0; return; }
                p += 10 + get16(p + 8);
            }
            post(dq[i].w, M_NETRESOLVED, RD_ERR_NOTFOUND, 0); dq[i].st = 0; return;
        }
    for (i = 0; i < NS; i++) {
        SOCK *c = &so[i];
        if (c->st == S_UDP && dp == c->lport && rfrei(c) >= m + 2) {
            static BYTE l[2]; l[0] = (BYTE)m; l[1] = (BYTE)(m >> 8);
            rx_rein(c, l, 2); rx_rein(c, u + 8, m); melde_daten(c); return;
        }
    }
    (void)ih;
}
static void ip_in(const BYTE *f, WORD len)
{
    const BYTE *h = f + 14; WORD hl, tl;
    if (len < 34 || (h[0] >> 4) != 4) return;
    hl = (h[0] & 15) * 4; tl = get16(h + 2);
    if (hl < 20 || tl < hl || tl > len - 14) return;
    if (dh_st) { if (h[9] == 17) udp_in(h, h + hl, tl - hl); return; }
    if (memcmp(h + 16, my_ip, 4) && memcmp(h + 16, ip_bc, 4)) return;
    if (get16(h + 6) & 0x3FFF) return;
    if (cfin(cadd(0, h, hl))) return;
    arp_put(h + 12, f + 6, 0);
    if (h[9] == 6) tcp_in(f, h, h + hl, tl - hl);
    else if (h[9] == 17) udp_in(h, h + hl, tl - hl);
    else if (h[9] == 1 && h[hl] == 8 && tl - hl <= SLOT - 34) {
        BYTE *o = tx + 34; WORD m = tl - hl;
        memcpy(o, h + hl, m); o[0] = 0; o[2] = o[3] = 0;
        put16(o + 2, cfin(cadd(0, o, m)));
        memcpy(ipb, h + 12, 4); memcpy(dmac, f + 6, 6);
        ip_out(ipb, dmac, 1, m);
    }
}

/* ---------------- DNS ---------------- */
static void dns_send(WORD i)
{
    BYTE *q = tx + 42, *p; const char *s = dq[i].name; WORD n;
    put16(q, dq[i].id); put16(q + 2, 0x0100); put16(q + 4, 1); put16(q + 6, 0); put16(q + 8, 0); put16(q + 10, 0);
    p = q + 12;
    while (*s) { BYTE *l = p++; n = 0; while (*s && *s != '.') { *p++ = *s++; n++; } *l = (BYTE)n; if (*s) s++; }
    *p++ = 0; put16(p, 1); put16(p + 2, 1); p += 4;
    udp_out(dns, dmac, 0xF000 + i, 53, (WORD)(p - q));
}
static void dns_takt(void)
{
    WORD i;
    for (i = 0; i < NDNS; i++) {
        if (!dq[i].st) continue;
        if (dq[i].st == 1) { if (mac_von(dns, dmac, 0)) { dq[i].st = 2; dq[i].tries = 0; dns_send(i); dq[i].t0 = ticks(); } else if (ticks() - dq[i].t0 > 9) { if (++dq[i].tries > 4) { post(dq[i].w, M_NETRESOLVED, RD_ERR_NET, 0); dq[i].st = 0; } else { mac_von(dns, dmac, 1); dq[i].t0 = ticks(); } } continue; }
        if (ticks() - dq[i].t0 < 36) continue;
        if (++dq[i].tries > 3) { post(dq[i].w, M_NETRESOLVED, RD_ERR_TIMEOUT, 0); dq[i].st = 0; continue; }
        mac_von(dns, dmac, 0); dns_send(i); dq[i].t0 = ticks();
    }
}

/* ---------------- Abfrage (aus input_poll des Kerns) ---------------- */
static void takt(void)
{
    WORD n, i; SOCK *c;
    while ((n = rx_len[rx_tail]) != 0) {
        BYTE *f = rx_buf + rx_tail * SLOT; WORD typ = get16(f + 12);
        if (typ == 0x0806 && n >= 42) arp_in(f + 14);
        else if (typ == 0x0800) ip_in(f, n);
        rx_len[rx_tail] = 0; rx_tail = (rx_tail + 1) % NSLOT; nrx++;
    }
    if (dh_st) { dhcp_takt(); return; }
    if (!up) return;
    dns_takt();
    for (i = 0; i < NS; i++) {
        c = &so[i];
        if (c->st == S_FREE) continue;
        if (c->w && !IsWindow(c->w)) {            /* Programm ist weg: Verbindung abbauen */
            c->w = 0; c->zu = 1;
            if (c->st == S_OPEN) { c->wfin = 1; senden(c); } else if (c->st != S_FIN) { frei(c); continue; }
        }
        if (c->st == S_FIN && c->una == c->end + 1) {   /* FIN bestaetigt: hoechstens 10 s auf das der Gegenseite warten */
            if (ticks() - c->tsent > 182) { c->st = S_DONE; if (c->zu) frei(c); }
            continue;
        }
        if (c->st == S_ARP) {
            if (mac_von(c->rip, c->rmac, 0)) { verbunden(c); continue; }
            if (ticks() - c->tsent < 9) continue;
            if (++c->tries > 4) { fehl(c, RD_ERR_NET); continue; }
            mac_von(c->rip, c->rmac, 1); c->tsent = ticks(); continue;
        }
        if (c->st != S_SYN && c->st != S_SYNR && c->st != S_OPEN && c->st != S_FIN) continue;
        if (c->st == S_OPEN && c->una == c->nxt) { if (c->nxt != c->end || c->wfin) senden(c); continue; }
        if (ticks() - c->tsent < c->rto) continue;
        if (++c->tries > 8) { fehl(c, RD_ERR_TIMEOUT); continue; }
        c->rto = c->rto < 64 ? c->rto * 2 : 128;
        wiederholen(c);
    }
}
static void RDEXPORT n_Haken(WORD art)
{
    if (art == 2) { if (!busy && !pausiert && (up || dh_st)) { busy = 1; takt(); busy = 0; } return; }
    if (art == 0) { if (h_ip != 0xFFFF) { pk_aus(); pausiert = 1; } return; }
    if (art == 1 && pausiert) { pausiert = 0; if (!pk_an()) { up = 0; fehler = RD_ERR_NET; } }
}
static void sperre(void) { while (busy) TaskYield(); busy = 1; }
static void frei_s(void) { busy = 0; }
static SOCK *sock(HSOCK s) { return s >= 1 && s <= NS && so[s - 1].st != S_FREE ? &so[s - 1] : 0; }

/* ---------------- API ---------------- */
static WORD RDEXPORT n_NetCaps(NETINFO FAR *ni)
{
    if (ni) {
        ni->ip = ipd(my_ip); ni->maske = ipd(mask); ni->gateway = ipd(gw); ni->dns = ipd(dns);
        _fmemcpy(ni->mac, my_mac, 6); ni->flags = (use_dhcp ? NI_DHCP : 0) | (up ? NI_UP : 0);
    }
    if (startet) return RD_ERR_BUSY;
    return up ? 0 : fehler;
}
static WORD RDEXPORT n_NetStart(HWND w)
{
    WORD i; SYSINFO FAR *si;
    static SYSINFO s;
    if (up) { post(w, M_NETSTATE, NS_OPEN, 0); return 0; }
    if (startet) { for (i = 0; i < 4; i++) if (!warte[i] || warte[i] == w) { warte[i] = w; break; } return 0; }
    si = &s; KernInfo(si);
    _fstrcpy(ini, si->home); if (ini[0] && ini[strlen(ini) - 1] != '\\') strcat(ini, "\\"); strcat(ini, "RD2.INI");
    use_dhcp = ProfileGetInt(ini, "Netz", "DHCP", 1) != 0;
    mss = (WORD)ProfileGetInt(ini, "Netz", "MSS", 1024); if (mss < 256) mss = 256; if (mss > 1460) mss = 1460;
    ProfileGetStr(ini, "Netz", "Name", "RD2", hname, sizeof hname);
    ProfileGetStr(ini, "Netz", "Paket", "0", sb, sizeof sb); pvec = hexzahl(sb);
    if (!use_dhcp) {
        ini_ip("IP", my_ip); ini_ip("Maske", mask); ini_ip("Gateway", gw); ini_ip("DNS", dns);
        if (null4(my_ip) || null4(mask)) { fehler = RD_ERR_FORMAT; SetLastError(fehler); return fehler; }
    }
    if (!(pvec = find_pkt(pvec))) { fehler = RD_ERR_NOTFOUND; SetLastError(fehler); DbgPrint("RDTCP: kein Paketreiber"); return fehler; }
    if (!pk_an()) { fehler = RD_ERR_BUSY; SetLastError(fehler); return fehler; }
    memset(warte, 0, sizeof warte); warte[0] = w; startet = 1; arpn = 0;
    if (!use_dhcp) { fertig(0); return 0; }
    lease_lesen(); memset(dh_ntp, 0, 4);
    dh_xid = ((DWORD)ticks() << 16) ^ get32(my_mac + 2); dh_tries = 0; dh_type = 0;
    if (!null4(lease)) { dh_st = 3; dhcp_send(3, lease, 0); } else { dh_st = 1; dhcp_send(1, 0, 0); }
    return 0;
}
static WORD RDEXPORT n_NetResolve(HWND w, LPCSTR host)
{
    WORD i, n;
    if (!up) { SetLastError(RD_ERR_NET); return RD_ERR_NET; }
    if (ip_parse(host, ipb)) { post(w, M_NETRESOLVED, 0, ipd(ipb)); return 0; }
    for (i = 0; i < NDNS && dq[i].st; i++) ;
    n = _fstrlen(host);
    if (i == NDNS || n > 63 || null4(dns)) { SetLastError(RD_ERR_BUSY); return RD_ERR_BUSY; }
    sperre();
    dq[i].w = w; _fstrcpy(dq[i].name, host); dq[i].id = ++dnsid ^ ticks(); dq[i].tries = 0; dq[i].t0 = ticks();
    if (mac_von(dns, dmac, 1)) { dq[i].st = 2; dns_send(i); } else dq[i].st = 1;
    frei_s(); return 0;
}
static HSOCK RDEXPORT n_NetOpen(HWND w, DWORD ip, WORD port, WORD flags)
{
    SOCK *c;
    if (!up) { SetLastError(RD_ERR_NET); return 0; }
    sperre();
    if (!(c = neu(w, (BYTE)(flags == NO_UDP)))) { frei_s(); return 0; }
    dip(c->rip, ip); c->rport = port;
    c->st = S_ARP; c->tsent = ticks();
    if (mac_von(c->rip, c->rmac, 1)) verbunden(c);
    frei_s(); return hnd(c);
}
static HSOCK RDEXPORT n_NetListen(HWND w, WORD port, WORD flags)
{
    SOCK *c;
    (void)flags;
    if (!up) { SetLastError(RD_ERR_NET); return 0; }
    sperre();
    if ((c = neu(w, 0)) != 0) { if (port) c->lport = port; c->st = S_LISTEN; }
    frei_s(); return c ? hnd(c) : 0;
}
static WORD RDEXPORT n_NetSend(HSOCK s, const void FAR *buf, WORD n)
{
    SOCK *c = sock(s); WORD k, i, a;
    if (!c) { SetLastError(RD_ERR_HANDLE); return 0; }
    sperre();
    if (c->st == S_UDP) {                         /* ein Datagramm */
        if (n > SLOT - 42) n = SLOT - 42;
        _fmemcpy(tx + 42, buf, n); udp_out(c->rip, c->rmac, c->lport, c->rport, n);
        frei_s(); return n;
    }
    if (c->st != S_OPEN || c->wfin) { frei_s(); return 0; }
    a = TXB - 1 - (WORD)(c->end - c->una);         /* frei im Sendepuffer */
    k = n < a ? n : a;
    i = (WORD)c->end & (TXB - 1);
    if (k > TXB - i) { _fmemcpy(c->tb + i, buf, TXB - i); _fmemcpy(c->tb, (const BYTE FAR *)buf + (TXB - i), k - (TXB - i)); }
    else _fmemcpy(c->tb + i, buf, k);
    c->end += k;
    senden(c);
    frei_s(); return k;
}
static WORD RDEXPORT n_NetRecv(HSOCK s, void FAR *buf, WORD n)
{
    SOCK *c = sock(s); WORD k, i, j; BYTE w0;
    if (!c) { SetLastError(RD_ERR_HANDLE); return 0; }
    sperre();
    c->gem = 0;
    if (c->udp) {
        if (c->rh == c->rt) { frei_s(); return 0; }
        i = c->rt & (RXB - 1); k = c->rx[i] | (c->rx[(i + 1) & (RXB - 1)] << 8); c->rt += 2;
        for (j = 0; j < k; j++) { if (j < n) ((BYTE FAR *)buf)[j] = c->rx[(c->rt + j) & (RXB - 1)]; }
        c->rt += k; if (k > n) k = n;
        melde_daten(c); frei_s(); return k;
    }
    k = c->rh - c->rt; if (k > n) k = n;
    i = c->rt & (RXB - 1);
    if (k > RXB - i) { _fmemcpy(buf, c->rx + i, RXB - i); _fmemcpy((BYTE FAR *)buf + (RXB - i), c->rx, k - (RXB - i)); }
    else _fmemcpy(buf, c->rx + i, k);
    w0 = c->wnull; c->rt += k;
    if (w0 && rwin(c) && (c->st == S_OPEN || c->st == S_FIN)) tcp_out(c, F_ACK, 0, c->st == S_FIN ? c->end + 1 : c->nxt);   /* Fenster wieder auf */
    melde_daten(c);
    frei_s(); return k;
}
static WORD RDEXPORT n_NetClose(HSOCK s)
{
    SOCK *c = sock(s);
    if (!c) return RD_ERR_HANDLE;
    sperre();
    c->zu = 1; c->w = 0;
    if (c->st == S_OPEN) { c->wfin = 1; senden(c); }
    else if (c->st == S_SYN || c->st == S_SYNR) { tcp_out(c, F_RST | F_ACK, 0, c->nxt); frei(c); }
    else if (c->st != S_FIN) frei(c);
    frei_s(); return 0;
}
static WORD RDEXPORT n_NetState(HSOCK s)
{
    SOCK *c = sock(s);
    if (!c) return NS_CLOSED;
    switch (c->st) {
    case S_ARP: case S_SYN: case S_SYNR: return NS_CONNECTING;
    case S_OPEN: return c->rfin && c->rh == c->rt ? NS_CLOSING : NS_OPEN;
    case S_FIN: return NS_CLOSING;
    case S_ERR: return NS_ERROR;
    case S_DONE: return NS_CLOSED;
    }
    return NS_OPEN;
}
static DWORD RDEXPORT n_NetLocalIP(void) { return up ? ipd(my_ip) : 0; }
static void RDEXPORT n_Ende(void) { WORD i; for (i = 0; i < NS; i++) if (so[i].st) frei(&so[i]); pk_aus(); }

static struct { WORD n; FARPROC f[12]; } ext = { 12, {
    (FARPROC)n_Ende, (FARPROC)n_NetCaps, (FARPROC)n_NetStart, (FARPROC)n_NetResolve, (FARPROC)n_NetOpen,
    (FARPROC)n_NetListen, (FARPROC)n_NetSend, (FARPROC)n_NetRecv, (FARPROC)n_NetClose, (FARPROC)n_NetState,
    (FARPROC)n_NetLocalIP, (FARPROC)n_Haken } };
void FAR * RDAPI RdModInit(void)
{
    WORD i;
    for (i = 0; i < 10; i++) { alt[i] = _rd2api[0x380 + i]; _rd2api[0x380 + i] = ext.f[1 + i]; }   /* API 0x380..0x389 */
    haken = (void (__far __pascal * FAR *)(WORD))_rd2api[0x3BF];
    if (haken) *haken = n_Haken;
    return &ext;
}
