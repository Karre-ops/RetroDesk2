/* RD32INT.H - intern fuer RD32LIB.C und das erzeugte RD32API.C (RD2/32, PLAN_32BIT Stufe 4b) */
#ifndef RD32INT_H
#define RD32INT_H
typedef struct { DWORD sf; void *p; DWORD n; BYTE aus; HMEM tmp; } PIN;   /* Zeiger fuer den Kern unter 1 MB */
extern WORD rd32_xp;
void  rd32_w1(WORD v);
void  rd32_w2(DWORD v);
DWORD rd32_ruf(WORD nr);
DWORD rd32_pin(PIN *b, const void *p, DWORD n, int aus);   /* aus: 0 nur hin, 1 zurueck, 2 Text zurueck */
void  rd32_unpin(PIN *b, WORD nb);
WORD  rd32_slen(const char *s);
void *rd32_fp2p(DWORD sf);
#endif
