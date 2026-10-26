/* Host-Test fuer den Rechenkern: bash test/rechner_kern.sh */
#include <stdio.h>
#include <string.h>
#include "../apps/rechner/RKERN.H"
static int fails;
static void t(const char *keys, const char *want)
{
    const char *p;
    calc_reset();
    for (p = keys; *p; p++) calc_key(*p == 'b' ? '\b' : *p);
    if (strcmp(calc_disp, want)) { printf("FEHLER %-14s -> %s (erwartet %s)\n", keys, calc_disp, want); fails++; }
    else printf("ok     %-14s -> %s\n", keys, calc_disp);
}
int main(void)
{
    t("12+30=", "42");
    t("12/7=", "1.714285714");
    t("2+3*4=", "20");          /* Taschenrechner: von links nach rechts */
    t("9S", "3");
    t("2S", "1.414213562");
    t("200+10%", "20");
    t("200+10%=", "220");
    t("5N", "-5");
    t("5N+8=", "3");
    t("7/0=", "Fehler");
    t("7/0=3", "3");
    t("1.5*4=", "6");
    t("123b", "12");
    t("10-=", "10");
    t("3+=", "3");               /* fehlender Operand: Wert bleibt */
    t("99999999*99999999=", "9.9999998e+15");
    t("4N S", "Fehler");
    t("0.1+0.2=", "0.3");
    t("..5+1=", "1.5");
    printf("%d Fehler\n", fails);
    return fails != 0;
}
