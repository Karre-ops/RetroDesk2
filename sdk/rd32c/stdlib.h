/* stdlib.h - C-Laufzeit fuer RD2/32 (RD32C.LIB). Speicher aus rd32_alloc (DPMI), exit beendet RdMain. */
#ifndef RD32C_STDLIB_H
#define RD32C_STDLIB_H
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
#define _SIZE_T_DEFINED_
typedef unsigned size_t;
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
void  *malloc(size_t n);
void  *calloc(size_t n, size_t g);
void  *realloc(void *p, size_t n);
void   free(void *p);
int    atoi(const char *s);
long   atol(const char *s);
double atof(const char *s);
long   strtol(const char *s, char **e, int basis);
unsigned long strtoul(const char *s, char **e, int basis);
double strtod(const char *s, char **e);
int    abs(int v);
long   labs(long v);
div_t  div(int a, int b);
ldiv_t ldiv(long a, long b);
int    rand(void);
void   srand(unsigned s);
void   qsort(void *b, size_t n, size_t g, int (*cmp)(const void *, const void *));
void  *bsearch(const void *k, const void *b, size_t n, size_t g, int (*cmp)(const void *, const void *));
void   exit(int code);
void   abort(void);
int    atexit(void (*f)(void));
char  *getenv(const char *name);
char  *itoa(int v, char *s, int basis);
char  *ltoa(long v, char *s, int basis);
char  *ultoa(unsigned long v, char *s, int basis);
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
