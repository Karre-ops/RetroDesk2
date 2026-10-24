/* fdglue.h - Schnittstelle zwischen der RD2-Oberflaeche (RDFDISK.C) und dem Kern von Free FDISK (fdglue.c).
 * Nur einfache Typen, damit beide Seiten ohne die Kopfdateien der anderen auskommen. */
#ifndef FDGLUE_H
#define FDGLUE_H
#define FG_PRI   1                                /* primaere Partition (DOS oder fremd) */
#define FG_EXT   2                                /* erweiterte Partition (Behaelter) */
#define FG_LOG   3                                /* logisches Laufwerk in der erweiterten */
#define FG_FREIP 4                                /* freier Platz fuer primaere/erweiterte Partitionen */
#define FG_FREIL 5                                /* freier Platz in der erweiterten Partition */
#define FG_MAX   30
typedef struct {
    unsigned char art, nr, typ, aktiv;            /* FG_*, Platz (0-3) bzw. Nummer, Partitionstyp, 1 = startet */
    char lw;                                      /* Laufwerksbuchstabe oder 0 */
    unsigned char dos;                            /* 1 = FAT (DOS kann es lesen) */
    unsigned long start, anzahl, mb;              /* Sektoren ab Plattenanfang, Groesse */
} FG_TEIL;
int  fg_start(void);                              /* liest alle Platten; liefert Anzahl (0 = keine) */
void fg_platte(int n);                            /* 0 oder 1 */
unsigned long fg_plattemb(void);
unsigned long fg_plattesek(void);
int  fg_liste(FG_TEIL *t);                        /* Eintraege nach Lage sortiert, liefert Anzahl */
unsigned long fg_maxmb(const FG_TEIL *t, int *alslog);   /* groesstes neues Laufwerk; *alslog = 1: logisch */
int  fg_anlegen(const FG_TEIL *t, unsigned long mb);     /* 0 = ok */
int  fg_loeschen(const FG_TEIL *t);               /* 0 = ok; erweiterte: samt logischen Laufwerken */
int  fg_aktiv(const FG_TEIL *t);
int  fg_geaendert(void);
int  fg_schreiben(void);                          /* 0 = ok */
extern int fg_fehler;                             /* letzter Fehler aus dem Kern (Protokoll) */
#endif
