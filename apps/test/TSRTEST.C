/* TSRTEST.C - bleibt resident (Audit M10): biegt INT 60h um, holt einen weiteren Block und
 * beendet sich mit INT 21h/31h. RunDos muss es wieder entfernen. test/rundos.sh */
#include <dos.h>
static void __interrupt __far neu60(void) { }
int main(void)
{
    unsigned seg;
    _dos_allocmem(64, &seg);                      /* zweiter Block, gehoert dem TSR */
    _dos_setvect(0x60, neu60);
    _dos_keep(0, 0x400);                          /* 16 KB resident */
    return 0;
}
