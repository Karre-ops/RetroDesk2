/* PONG.C - beantwortet M_PING mit M_PONG, endet bei M_CLOSE */
#include "RD2.H"
short RDAPI RdMain(HINST inst, LPCSTR cmd, WORD show)
{
    MSG m; HTASK ping = (HTASK)(cmd[0] - 'A'); WORD n = 0;
    (void)inst; (void)show;
    while (GetMessage(&m)) {
        if (m.msg == M_USER + 1) { PostTask(ping, M_USER + 2, m.p1, 0); n++; }
        if (m.msg == M_CLOSE) PostQuit(n);
    }
    DbgHex("PONG: Ende, beantwortet", n);
    return (short)m.p1;
}
