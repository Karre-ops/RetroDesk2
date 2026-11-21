/* RD32INT.H - intern fuer RD32LIB.C, RD32PRG.C und das erzeugte RD32API.C (RD2/32, PLAN_32BIT Stufe 4b) */
#ifndef RD32INT_H
#define RD32INT_H
typedef struct { DWORD sf; void *p; DWORD n; BYTE aus; HMEM tmp; } PIN;   /* Zeiger fuer den Kern unter 1 MB */
extern DWORD rd32_tlin, rd32_base;
#pragma aux rd32_tlin "_*";
#pragma aux rd32_base "_*";
void rd32_call(void);
typedef struct { DWORD eax, ebx, ecx, edx, esi, edi, cf; } R32;
void rd32_int31(R32 *r);
#define TB ((BYTE *)(rd32_tlin - rd32_base))
#define TW(o) (*(volatile WORD *)(TB + (o)))
#define TD(o) (*(volatile DWORD *)(TB + (o)))
#define TB_NR 8
#define TB_NW 10
#define TB_RES 12
#define TB_CBSLOT 56
#define TB_CBW 58
#define TB_CBMSG 60
#define TB_CBP1 62
#define TB_CBP2 64
#define TB_THUNK 80
#define TB_THUNK2 104
#define TB_INST 96
#define TB_SHOW 98
#define TB_ARGS 128
#define TB_CMD 192
#define TB_XBUF 320
#define XBUF 3072
#define TB_KLASSE (TB_XBUF + XBUF - 64)           /* 4 Klassennamen zu 16 Zeichen, bleiben liegen */
#define TB_PROCS (TB_XBUF + XBUF - 96)          /* 8 Rueckruf-Prozeduren: 0-3 Fensterklassen, 4-7 Dialoge */
#define PROCS ((void **)(TB + TB_PROCS))
#define XFREI (XBUF - 96)
#define TB_XP 126
#define rd32_xp TW(TB_XP)                         /* belegt im Austauschpuffer (Stapel, Rueckrufe arbeiten darueber) */
void  rd32_w1(WORD v);
void  rd32_w2(DWORD v);
DWORD rd32_ruf(WORD nr);
DWORD rd32_pin(PIN *b, const void *p, DWORD n, int aus);   /* aus: 0 nur hin, 1 zurueck, 2 Text zurueck */
void  rd32_unpin(PIN *b, WORD nb);
WORD  rd32_slen(const char *s);
void *rd32_fp2p(DWORD sf);
#endif
