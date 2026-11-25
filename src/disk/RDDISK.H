/* RDDISK.H - Laufwerkszugriff ueber RDDISK.MOD (fuer RDFORMAT, RD2SETUP) */
#ifndef RDDISK_H
#define RDDISK_H
#define RDD_INT 2                                 /* ModProc-Nummer: wie int86x, Positivliste */
#define RDD_MERKER 3                              /* WORD (RDAPI *)(void): 1 = RDFORMAT /W noetig */
typedef WORD (RDAPI *RDD_MERKER_F)(void);
typedef WORD (RDAPI *RDD_INT_F)(WORD nr, union REGS FAR *ein, union REGS FAR *aus, struct SREGS FAR *s);
#endif
