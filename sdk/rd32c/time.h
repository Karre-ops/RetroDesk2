/* time.h - C-Laufzeit fuer RD2/32 (RD32C.LIB): Uhr ueber TimeGet, clock in ms seit RD2-Start */
#ifndef RD32C_TIME_H
#define RD32C_TIME_H
typedef long time_t;
typedef unsigned long clock_t;
#define CLOCKS_PER_SEC 1000
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
clock_t clock(void);
time_t  time(time_t *t);
double  difftime(time_t a, time_t b);
struct tm *localtime(const time_t *t);
time_t  mktime(struct tm *t);
#endif
