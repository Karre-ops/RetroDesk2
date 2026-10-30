/* ZK.H - gemeinsame Typen fuer RDZIP (Packen/Entpacken, CRC). Laeuft auch auf dem Entwicklungsrechner (test/zip_kern.sh). */
#ifndef ZK_H
#define ZK_H
#ifdef __WATCOMC__
#include "RD2.H"
#else
#include <stdint.h>
typedef uint8_t BYTE; typedef uint16_t WORD; typedef uint32_t DWORD;
#define FAR
#endif
typedef WORD (*ZLESEN)(void *u, BYTE FAR *b, WORD n);          /* liefert gelesene Bytes, 0 = Ende */
typedef WORD (*ZSCHREIB)(void *u, const BYTE FAR *b, WORD n);  /* liefert n bei Erfolg */

/* CRC-32 (wie PKZIP): Start 0xFFFFFFFF, am Ende ^ 0xFFFFFFFF */
void  CrcInit(void);
DWORD CrcRechne(DWORD crc, const BYTE FAR *p, WORD n);

/* Entpacken (Deflate, RFC 1951). fenster: 32 KB, ein: Eingabepuffer (egr Bytes). 0 = ok, sonst ZE_* */
#define ZE_DATEN  1                               /* Daten kaputt */
#define ZE_KURZ   2                               /* Daten zu kurz */
#define ZE_SCHREIB 3                              /* Schreiben fehlgeschlagen (Platte voll) */
#define ZE_ABBRUCH 4                              /* Benutzer hat abgebrochen */
WORD Inflate(ZLESEN le, ZSCHREIB sc, void *u, BYTE FAR *fenster, BYTE FAR *ein, WORD egr);

/* Packen (Deflate). Speicher: DefSpeicher() Bytes in 4 Bloecken (DefBlock(i)), vom Aufrufer besorgt.
   Liefert 0 = ok, sonst ZE_*. *aus = geschriebene Bytes */
#define DEF_FENSTER 8192U                         /* Suchfenster (Deflate erlaubt bis 32 KB) */
#define DEF_PUF0 (2U * DEF_FENSTER)               /* Daten: zwei Fenster */
#define DEF_PUF1 (2U * DEF_FENSTER)               /* prev: WORD je Fensterstelle */
#define DEF_PUF2 (2U * 4096U)                     /* head: Streuwerte */
#define DEF_SYM  8192U                            /* Symbole je Block */
#define DEF_PUF3 (3U * DEF_SYM + 4096U)           /* Symbole (3 Bytes) + Ausgabepuffer 4 KB */
WORD Deflate(ZLESEN le, ZSCHREIB sc, void *u, BYTE FAR *daten, WORD FAR *prev, WORD FAR *head, BYTE FAR *sym, DWORD *aus);
extern BYTE zk_abbruch;                           /* von aussen setzen: Packen/Entpacken hoeren auf */
#endif
