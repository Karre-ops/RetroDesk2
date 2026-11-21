/* WUERFEL.C - RD3D (PLAN_32BIT Schritt 5a): Selbsttest (Z-Puffer, Alpha, Nebel) und drehender Wuerfel mit Textur,
 * Gouraud-Licht, Nebel und einer halb durchsichtigen Seite. Befehlszeile: L = VESA linear 640x480, F = im Fenster,
 * sonst Vollbild 13h; A dazu = nach 40 Bildern beenden. Esc beendet, Alt+Tab parkt (Vollbild). */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#include "RD3D.H"

static R3D *r; static BYTE tex[64 * 64]; static BYTE ende, geparkt, voll;
static const R3DVEC ecke[8] = { {FX(-1),FX(-1),FX(-1)}, {FX(1),FX(-1),FX(-1)}, {FX(1),FX(1),FX(-1)}, {FX(-1),FX(1),FX(-1)},
                                {FX(-1),FX(-1),FX(1)},  {FX(1),FX(-1),FX(1)},  {FX(1),FX(1),FX(1)},  {FX(-1),FX(1),FX(1)} };
static const BYTE seite[6][4] = { {0,1,2,3}, {5,4,7,6}, {4,0,3,7}, {1,5,6,2}, {3,2,6,7}, {4,5,1,0} };
static const R3DVEC normal[6] = { {0,0,FX(-1)}, {0,0,FX(1)}, {FX(-1),0,0}, {FX(1),0,0}, {0,FX(1),0}, {0,FX(-1),0} };

static void punkt(FIX x, FIX y, FIX z, FIX s, R3DVERT *v) { v->x = x; v->y = y; v->z = z; v->u = v->v = 0; v->s = s; }
static void selbsttest(void)                      /* drei Pruefungen im Rueckpuffer, vor dem ersten Bild */
{
    R3DVERT a, b, c; BYTE p1, p2, q1, q2; WORD mx = r->w / 2, my = r->h / 2;
    R3dClear(r, 0); R3dZustand(r, R3S_ZTEST | R3S_ZWRITE);
    R3dFarbe(r, 0x4F);                            /* fern, Rampe 4 */
    punkt(FX(0), FX(0), 50000, FX(15), &a); punkt(FX(r->w), FX(0), 50000, FX(15), &b); punkt(FX(mx), FX(r->h), 50000, FX(15), &c);
    R3dTri(r, &a, &b, &c);
    R3dFarbe(r, 0x1F);                            /* nah, Rampe 1 */
    punkt(FX(0), FX(r->h), 1000, FX(15), &a); punkt(FX(r->w), FX(r->h), 1000, FX(15), &b); punkt(FX(mx), FX(0), 1000, FX(15), &c);
    R3dTri(r, &a, &b, &c);
    R3dFarbe(r, 0x4F);                            /* noch einmal fern: darf das nahe nicht ueberdecken */
    punkt(FX(0), FX(0), 50000, FX(15), &a); punkt(FX(r->w), FX(0), 50000, FX(15), &b); punkt(FX(mx), FX(r->h), 50000, FX(15), &c);
    R3dTri(r, &a, &b, &c);
    DbgPrint(R3dPunkt(r, mx, my) == 0x1F ? "WUERFEL: Z-Test ok" : "WUERFEL: Z-Test FEHLER");
    R3dClear(r, 0); R3dZustand(r, R3S_ALPHA); R3dFarbe(r, 0x2F);
    punkt(FX(-10), FX(-10), 100, FX(15), &a); punkt(FX(r->w * 3), FX(-10), 100, FX(15), &b); punkt(FX(-10), FX(r->h * 3), 100, FX(15), &c);
    R3dTri(r, &a, &b, &c);
    p1 = R3dPunkt(r, 10, 10); p2 = R3dPunkt(r, 11, 10);
    DbgPrint(p1 == 0x2F && p2 == 0 ? "WUERFEL: Alpha-Raster ok" : "WUERFEL: Alpha-Raster FEHLER");
    R3dNebel(r, 0, 0, 0, 10000, 60000); R3dPalette(r);
    R3dClear(r, 0); R3dZustand(r, R3S_NEBEL); R3dFarbe(r, 0x3F);
    punkt(FX(-10), FX(-10), 2000, FX(15), &a); punkt(FX(r->w * 3), FX(-10), 2000, FX(15), &b); punkt(FX(-10), FX(r->h * 3), 2000, FX(15), &c);
    R3dTri(r, &a, &b, &c); q1 = R3dPunkt(r, 5, 5);
    R3dClear(r, 0);
    punkt(FX(-10), FX(-10), 45000, FX(15), &a); punkt(FX(r->w * 3), FX(-10), 45000, FX(15), &b); punkt(FX(-10), FX(r->h * 3), 45000, FX(15), &c);
    R3dTri(r, &a, &b, &c); q2 = R3dPunkt(r, 5, 5);
    DbgPrint(q1 == 0x3F && (q2 & 0xF0) == 0x30 && (q2 & 15) > 0 && (q2 & 15) < 12 ? "WUERFEL: Nebel ok" : "WUERFEL: Nebel FEHLER");
}
static void bild(WORD w)
{
    R3DMAT m; R3DVEC p[8], n; R3DVERT v[8]; WORD i, k; int ok[8];
    static const FIX uv[4][2] = { {0, 0}, {FX(63), 0}, {FX(63), FX(63)}, {0, FX(63)} };
    R3dDreh(&m, w, (WORD)(w * 2 / 3), (WORD)(w / 3)); R3dVersch(&m, 0, 0, FX(4));
    for (i = 0; i < 8; i++) { R3dXform(&m, &ecke[i], &p[i]); ok[i] = R3dProj(r, &p[i], &v[i], FX(8)); }
    R3dClear(r, 0x80);                            /* Rampe 8 dunkel: Hintergrund */
    for (k = 0; k < 6; k++) {
        const BYTE *s = seite[k]; R3DVERT q[4]; FIX licht; __int64 kr;
        if (!ok[s[0]] || !ok[s[1]] || !ok[s[2]] || !ok[s[3]]) continue;
        kr = (__int64)(v[s[1]].x - v[s[0]].x) * (v[s[2]].y - v[s[0]].y) - (__int64)(v[s[2]].x - v[s[0]].x) * (v[s[1]].y - v[s[0]].y);
        if (k != 1 && kr >= 0) continue;          /* Rueckseite weg (die durchsichtige Seite 1 immer zeichnen) */
        m.m[0][3] = m.m[1][3] = m.m[2][3] = 0; R3dXform(&m, &normal[k], &n);
        licht = FX(4) + FXMUL(-n.z, FX(9)) + FXMUL(n.y, FX(2));   /* Licht von vorn, etwas von oben */
        if (licht < FX(2)) licht = FX(2);
        if (licht > FX(15)) licht = FX(15);
        for (i = 0; i < 4; i++) { q[i] = v[s[i]]; q[i].u = uv[i][0]; q[i].v = uv[i][1]; q[i].s = licht - (FIX)i * 0x6000; }
        R3dZustand(r, R3S_ZTEST | R3S_ZWRITE | R3S_TEXTUR | R3S_GOURAUD | R3S_NEBEL | (k == 1 ? R3S_ALPHA : 0));
        R3dTri(r, &q[0], &q[1], &q[2]); R3dTri(r, &q[0], &q[2], &q[3]);
    }
    R3dFlip(r);
}
static DWORD RDCALLBACK proc(HWND w, WORD msg, WORD p1, DWORD p2)
{
    switch (msg) {
    case M_SCREENLOST: geparkt = 1; DbgPrint("WUERFEL: SCREENLOST"); return 0;
    case M_SCREENBACK: ScreenRestore(); R3dPalette(r); geparkt = 0; DbgPrint("WUERFEL: SCREENBACK"); return 0;
    case M_KEY: if (p1 == K_ESC) { ende = 1; return 0; } break;
    case M_DESTROY: PostQuit(0); return 0;
    case M_PAINT: { PAINT ps; BeginPaint(w, &ps); EndPaint(w, &ps); } return 0;
    }
    return DefWindowProc(w, msg, p1, p2);
}
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    WNDCLASS wc; MSG m; HWND w; WORD art = R3D_VOLL13, x, y, a = 0; DWORD t0 = 0; BYTE auto_ = 0; const char *c;
    (void)show;
    for (c = cmd; *c; c++) { if (*c == 'L') art = R3D_VOLLLFB; if (*c == 'F') art = R3D_FENSTER; if (*c == 'A') auto_ = 1; }
    voll = (BYTE)(art != R3D_FENSTER);
    wc.name = "Wuerfel"; wc.proc = proc; wc.stil = 0; wc.daten = 0; wc.inst = inst; wc.symbol = 0; wc.hinter = RGB(0, 0, 0);
    if (!RegisterClass(&wc)) return 1;
    w = CreateWindow("Wuerfel", "RD3D Wuerfel", WS_TITLE | WS_BORDER | WS_VISIBLE, 40, 40, voll ? 200 : 268, voll ? 60 : 228, 0, 0, inst);
    if (!w || (voll && !ScreenAcquire(w))) { DbgHex("WUERFEL: Fenster/Vollbild Fehler", LastError()); return 2; }
    if (!(r = R3dOpen(art, w, 256, 192))) { DbgPrint("WUERFEL: R3dOpen Fehler"); if (voll) ScreenRelease(w); DestroyWindow(w); return 3; }
    { char z[48] = "WUERFEL: Treiber "; WORD i = 17; const char *q = R3dTreiber(r); while (*q && i < 46) z[i++] = *q++; z[i] = 0; DbgPrint(z); }
    DbgHex("WUERFEL: Breite", r->w);
    for (y = 0; y < 64; y++) for (x = 0; x < 64; x++)   /* Schachbrett: Rampe 12 (rot) und 14 (gelb), Rand dunkler */
        tex[y * 64 + x] = (BYTE)(((x >> 3) ^ (y >> 3)) & 1 ? 0xC0 | (x == 0 || y == 0 ? 8 : 15) : 0xE0 | (x == 0 || y == 0 ? 8 : 13));
    R3dRampe(r, 8, 40, 40, 90); R3dRampe(r, 12, 255, 60, 40); R3dRampe(r, 14, 255, 230, 60);
    selbsttest();
    R3dNebel(r, 40, 40, 90, 20000, FX(1)); R3dPalette(r); R3dTextur(r, tex, 6);
    t0 = TickCount();
    while (!ende) {
        while (PeekMessage(&m, 1)) { if (m.msg == M_QUIT) { ende = 1; break; } DispatchMessage(&m); }
        if (ende) break;
        if (geparkt) { WaitMessage(); continue; }
        bild(a); a = (WORD)(a + 12);
        if (r->bilder == 20) { DbgHex("WUERFEL: 20 Bilder in ms", TickCount() - t0); DbgPrint("WUERFEL: gezeichnet"); }
        if (auto_ && r->bilder >= 40) ende = 1;
        TaskYield();
    }
    DbgHex("WUERFEL: Bilder", r->bilder);
    R3dClose(r);
    if (voll) ScreenRelease(w);
    DestroyWindow(w);
    while (GetMessage(&m)) DispatchMessage(&m);
    DbgPrint("WUERFEL: Ende");
    return 0;
}
