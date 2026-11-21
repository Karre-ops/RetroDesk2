/* stdio.h - C-Laufzeit fuer RD2/32 (RD32C.LIB). Dateien ueber die RD2-API, stdout/stderr ins RD2-Protokoll (COM1). */
#ifndef RD32C_STDIO_H
#define RD32C_STDIO_H
#include <stdarg.h>
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
#define _SIZE_T_DEFINED_
typedef unsigned size_t;
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif
typedef struct _rd32_file FILE;
#define EOF (-1)
#define BUFSIZ 1024
#define FILENAME_MAX 80
#define FOPEN_MAX 16
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
extern FILE *const stdin, *const stdout, *const stderr;
FILE  *fopen(const char *name, const char *mode);
int    fclose(FILE *f);
size_t fread(void *p, size_t g, size_t n, FILE *f);
size_t fwrite(const void *p, size_t g, size_t n, FILE *f);
int    fgetc(FILE *f);
int    fputc(int c, FILE *f);
#define getc(f) fgetc(f)
#define putc(c, f) fputc(c, f)
int    getchar(void);
int    putchar(int c);
int    ungetc(int c, FILE *f);
char  *fgets(char *s, int n, FILE *f);
int    fputs(const char *s, FILE *f);
int    puts(const char *s);
int    fseek(FILE *f, long pos, int art);
long   ftell(FILE *f);
void   rewind(FILE *f);
int    feof(FILE *f);
int    ferror(FILE *f);
void   clearerr(FILE *f);
int    fflush(FILE *f);
int    remove(const char *name);
int    rename(const char *alt, const char *neu);
int    printf(const char *fmt, ...);
int    fprintf(FILE *f, const char *fmt, ...);
int    sprintf(char *s, const char *fmt, ...);
int    snprintf(char *s, size_t n, const char *fmt, ...);
int    vprintf(const char *fmt, va_list a);
int    vfprintf(FILE *f, const char *fmt, va_list a);
int    vsprintf(char *s, const char *fmt, va_list a);
int    vsnprintf(char *s, size_t n, const char *fmt, va_list a);
int    sscanf(const char *s, const char *fmt, ...);
#endif
