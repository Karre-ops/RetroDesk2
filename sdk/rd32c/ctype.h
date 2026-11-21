/* ctype.h - C-Laufzeit fuer RD2/32 (RD32C.LIB), ASCII */
#ifndef RD32C_CTYPE_H
#define RD32C_CTYPE_H
int isalpha(int c); int isdigit(int c); int isalnum(int c); int isspace(int c); int isupper(int c); int islower(int c);
int isxdigit(int c); int ispunct(int c); int isprint(int c); int iscntrl(int c); int isgraph(int c);
int toupper(int c); int tolower(int c);
#endif
