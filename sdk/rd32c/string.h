/* string.h - C-Laufzeit fuer RD2/32 (RD32C.LIB) */
#ifndef RD32C_STRING_H
#define RD32C_STRING_H
#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
#define _SIZE_T_DEFINED_
typedef unsigned size_t;
#endif
#ifndef NULL
#define NULL ((void *)0)
#endif
void  *memcpy(void *z, const void *q, size_t n);
void  *memmove(void *z, const void *q, size_t n);
void  *memset(void *z, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memchr(const void *p, int c, size_t n);
size_t strlen(const char *s);
char  *strcpy(char *z, const char *q);
char  *strncpy(char *z, const char *q, size_t n);
char  *strcat(char *z, const char *q);
char  *strncat(char *z, const char *q, size_t n);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
int    stricmp(const char *a, const char *b);
int    strnicmp(const char *a, const char *b, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *s, const char *t);
char  *strpbrk(const char *s, const char *t);
size_t strspn(const char *s, const char *t);
size_t strcspn(const char *s, const char *t);
char  *strtok(char *s, const char *t);
char  *strdup(const char *s);
char  *strupr(char *s);
char  *strlwr(char *s);
char  *strerror(int e);
#endif
