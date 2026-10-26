/* NETTEST.C - Test fuer RDTCP.MOD (test/netz.sh). Ablauf in Schritten, jede Meldung ueber DbgPrint:
 * 1 NetStart (DHCP), 2 NetResolve "10.0.2.2" (ohne DNS), 3 TCP-Echo 10.0.2.2:7007 mit 20000 Bytes Muster,
 * 4 UDP-Echo 10.0.2.2:7008, 5 Lauschen auf 2121 (Host verbindet sich, schickt "hallo", wir antworten "welt"),
 * 6 NetResolve "example.com" (nur Hinweis, braucht Internet), dann Ende. */
#include "RD2.H"

#define N 20000U
static HWND fe; static HSOCK t, u, l, a; static WORD schritt, gesendet, empf, fehler; static DWORD summe, soll;
static BYTE buf[600];

static BYTE muster(WORD i) { return (BYTE)(i * 7 + (i >> 8)); }
static void hex(LPCSTR s, DWORD v) { DbgHex(s, v); }
static void weiter(void);
static void tcp_senden(void)
{
    WORD n, i, k;
    while (gesendet < N) {
        n = N - gesendet; if (n > sizeof buf) n = sizeof buf;
        for (i = 0; i < n; i++) buf[i] = muster(gesendet + i);
        k = NetSend(t, buf, n);
        gesendet += k;
        if (k < n) break;                         /* Puffer voll: spaeter weiter */
    }
}
static void weiter(void)
{
    NETINFO ni;
    schritt++;
    switch (schritt) {
    case 1: hex("NETTEST: NetStart", NetStart(fe)); break;
    case 2: NetCaps(&ni); hex("NETTEST: IP", ni.ip); hex("NETTEST: Resolve", NetResolve(fe, "10.0.2.2")); break;
    case 3: t = NetOpen(fe, 0x0202000AUL, 7007, NO_TCP); hex("NETTEST: TCP offen", t); break;
    case 4: u = NetOpen(fe, 0x0202000AUL, 7008, NO_UDP); hex("NETTEST: UDP offen", u); break;
    case 5: l = NetListen(fe, 2121, 0); hex("NETTEST: Lauschen", l); DbgPrint("NETTEST: lauscht"); break;
    case 6: hex("NETTEST: Resolve DNS", NetResolve(fe, "example.com")); break;
    default: hex("NETTEST: Ende, Fehler", fehler); PostMessage(fe, M_CLOSE, 0, 0); break;
    }
}
static DWORD RDAPI proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    WORD n, i;
    switch (msg) {
    case M_NETSTATE: hex("NETTEST: Netzstatus", p1); if (p1 == NS_OPEN) weiter(); else { fehler++; schritt = 99; weiter(); } return 0;
    case M_NETRESOLVED:
        hex("NETTEST: aufgeloest", p2); hex("NETTEST: Resolve Ergebnis", p1);
        if (schritt == 2 && p2 != 0x0202000AUL) fehler++;
        weiter(); return 0;
    case M_NETCONNECT:
        hex("NETTEST: verbunden", p1); hex("NETTEST: Verbindung Fehler", p2);
        if (p2) { fehler++; weiter(); return 0; }
        if (p1 == t) tcp_senden();
        if (p1 == u) { NetSend(u, "ping RD2", 8); }
        return 0;
    case M_NETDATA:
        if (p1 == t) {
            while ((n = NetRecv(t, buf, sizeof buf)) != 0) { for (i = 0; i < n; i++) { summe += buf[i]; if (buf[i] != muster(empf + i)) fehler++; } empf += n; }
            tcp_senden();
            if (empf >= N) { hex("NETTEST: Echo Bytes", empf); hex("NETTEST: Echo Fehler", fehler); NetClose(t); t = 0; weiter(); }
        } else if (p1 == u) {
            n = NetRecv(u, buf, sizeof buf); buf[n < sizeof buf ? n : sizeof buf - 1] = 0;
            DbgPrint("NETTEST: UDP Antwort"); DbgPrint((LPCSTR)buf);
            if (n != 8 || buf[0] != 'P') fehler++;
            NetClose(u); u = 0; weiter();
        } else if (p1 == a) {
            n = NetRecv(a, buf, sizeof buf); buf[n < sizeof buf ? n : sizeof buf - 1] = 0;
            DbgPrint("NETTEST: angenommen, Text"); DbgPrint((LPCSTR)buf);
            NetSend(a, "welt\r\n", 6); NetClose(a); a = 0; NetClose(l); l = 0; weiter();
        }
        return 0;
    case M_NETACCEPT: a = p1; hex("NETTEST: angenommen", p1); return 0;
    case M_NETCLOSED: hex("NETTEST: geschlossen", p1); hex("NETTEST: geschlossen Fehler", p2); return 0;
    case M_CLOSE: DestroyWindow(w); return 0;
    case M_DESTROY: PostQuit(0); return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m;
    (void)cmd; (void)show;
    (void)soll;
    wc.name = "NetTest"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RD_DEFAULT;
    RegisterClass(&wc);
    fe = CreateWindow("NetTest", "Netztest", WS_NORMAL | WS_VISIBLE, 20, 20, 300, 100, 0, 1, inst);
    if (!fe) return 1;
    DbgPrint("NETTEST: bereit");
    weiter();
    while (GetMessage(&m)) DispatchMessage(&m);
    return 0;
}
