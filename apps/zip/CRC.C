/* CRC.C - CRC-32 fuer ZIP, in zwei 16-Bit-Haelften gerechnet (auf dem 8088 ohne 32-Bit-Schieben) */
#include "ZK.H"
static WORD tlo[256], thi[256];
void CrcInit(void)
{
    WORD i, k, lo, hi;
    if (thi[128]) return;
    for (i = 0; i < 256; i++) {
        lo = i; hi = 0;
        for (k = 0; k < 8; k++) {
            if (lo & 1) { lo = (WORD)((lo >> 1) | (hi << 15)); hi >>= 1; lo ^= 0x8320; hi ^= 0xEDB8; }
            else { lo = (WORD)((lo >> 1) | (hi << 15)); hi >>= 1; }
        }
        tlo[i] = lo; thi[i] = hi;
    }
}
DWORD CrcRechne(DWORD crc, const BYTE FAR *p, WORD n)
{
    WORD lo = (WORD)crc, hi = (WORD)(crc >> 16), x;
    while (n--) {
        x = (BYTE)(lo ^ *p++);
        lo = (WORD)(((lo >> 8) | (hi << 8)) ^ tlo[x]);
        hi = (WORD)((hi >> 8) ^ thi[x]);
    }
    return ((DWORD)hi << 16) | lo;
}
