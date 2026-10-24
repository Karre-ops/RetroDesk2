/* fdshim.h - wird vor jede FreeDOS-Datei (FORMAT, SYS) gesetzt (wcc -fi=disk/fdshim.h).
 * Leitet DOS-/BIOS-Aufrufe ueber RDDISK.MOD, Textausgabe ins Protokoll, exit() zurueck in die
 * RD2-Oberflaeche. Der FreeDOS-Quelltext bleibt dadurch fast unveraendert. */
#ifndef FDSHIM_H
#define FDSHIM_H
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <io.h>
#include <process.h>
#include <string.h>
#include <ctype.h>
int  rd_int86x(int nr, union REGS *ein, union REGS *aus, struct SREGS *s);
void rd_exit(int code);
int  rd_putch(int c);
int  rd_system(const char *cmd);
int  rd_write(int fd, const void *b, unsigned n);
int  rd_isatty(int fd);
#define int86(n, i, o)      rd_int86x(n, i, o, (struct SREGS *)0)
#define int86x(n, i, o, s)  rd_int86x(n, i, o, s)
#define intdos(i, o)        rd_int86x(0x21, i, o, (struct SREGS *)0)
#define intdosx(i, o, s)    rd_int86x(0x21, i, o, s)
#define exit(c)             rd_exit(c)
#define putch(c)            rd_putch(c)
#define system(c)           rd_system(c)
#define write(f, b, n)      rd_write(f, b, n)       /* nur Bildschirm (1, 2): ins Protokoll */
#define isatty(f)           rd_isatty(f)
#define main                fd_main
#endif
