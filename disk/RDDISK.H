/* RDDISK.H - Diskettenformate fuer RDDISK.MOD und RDFORMAT.EXE */
#ifndef RDDISK_H
#define RDDISK_H
typedef struct {
    WORD kb, zyl, koepfe, spt;                    /* 360 = 40 Zylinder, 2 Koepfe, 9 Sektoren */
    WORD sektoren;                                /* gesamt */
    BYTE media, spc, root, fsek;                  /* Medienbyte, Sektoren je Cluster, Verzeichnissektoren, FAT-Sektoren */
} DGEO;
/* ModProc-Nummern */
#define RDD_TYP    2
#define RDD_START  3
#define RDD_SPUR   4
#define RDD_SEKTOR 5
#define RDD_ENDE   6
typedef WORD (RDAPI *RDD_TYP_F)(WORD lw);
typedef WORD (RDAPI *RDD_START_F)(WORD lw, const DGEO FAR *g);
typedef WORD (RDAPI *RDD_SPUR_F)(WORD lw, WORD zyl, WORD kopf, WORD pruefen);
typedef WORD (RDAPI *RDD_SEKTOR_F)(WORD lw, WORD schreiben, WORD lba, void FAR *puf);
typedef void (RDAPI *RDD_ENDE_F)(WORD lw, WORD dos);
#endif
