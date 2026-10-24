/* rdsysw.h - RD2-Anbindung fuer FreeDOS SYS (disk/fdsys/sys.c, nur mit -dRD2).
 * Dateien laufen ueber den RD2-Kern (FileOpen usw.), Speicher ueber MemAlloc, Laufwerk ueber
 * RDDISK.MOD. Umsetzung in disk/RDFORMAT.C (kennt RD2.H; sys.c bleibt bei den FreeDOS-Typen). */
#ifndef RDSYSW_H
#define RDSYSW_H
int  rd_fopen(const char *name, int flags, ...);
int  rd_fread(int fd, void *b, unsigned n);
int  rd_fclose(int fd);
long rd_fseek(int fd, long pos, int art);
long rd_flen(int fd);
int  rd_fdel(const char *name);
int  rd_ffirst(const char *name, unsigned long *groesse);   /* 0 = vorhanden */
unsigned rd_getftime(int fd, unsigned *datum, unsigned *zeit);
unsigned rd_setftime(int fd, unsigned datum, unsigned zeit);
unsigned rd_dosalloc(unsigned paras, unsigned *seg);
unsigned rd_dosfree(unsigned seg);
unsigned rd_diskfree(unsigned lw, struct _diskfree_t *df);
#undef  open
#define open            rd_fopen
#define read            rd_fread
#define close           rd_fclose
#define lseek           rd_fseek
#define filelength      rd_flen
#define unlink          rd_fdel
#define _dos_getftime   rd_getftime
#define _dos_setftime   rd_setftime
#define _dos_allocmem   rd_dosalloc
#define _dos_freemem    rd_dosfree
#define _dos_getdiskfree rd_diskfree
#endif
