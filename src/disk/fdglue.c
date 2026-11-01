/* fdglue.c - Kern von Free FDISK (pcompute.c, pdiskio.c, GPL v2) fuer die RD2-Oberflaeche RDFDISK.
 * Wird wie die FreeDOS-Dateien mit src/disk/fdshim.h uebersetzt; die Oberflaeche sieht nur fdglue.h.
 * Ersetzt main.c/fdiskio.c von FDISK: Voreinstellungen (ohne FDISK.INI), Liste der Teile, Anlegen nach
 * DOS-Regel (erste DOS-Partition primaer und aktiv, weitere als logische Laufwerke in einer erweiterten). */
#define MAIN                                      /* main.h: flags, arg, user_defined_chs_settings hier anlegen */
#include "main.h"
#include "pcompute.h"
#include "pdiskio.h"
#include <stdarg.h>
#include "../fdglue.h"
#define MAXFAT16NORM 2047                         /* wie pcompute.c: groesste FAT16-Partition (MB) */

int fg_fehler;
static int platte;

void Pause(void) { }                              /* FDISK-Rueckfragen im Textmodus: entfallen */

int fg_start(void)
{
    int i;
    memset(&flags, 0, sizeof flags);
    flags.dla = DLA_MSDOS;
    flags.drive_number = 0x80;
    flags.flag_sector = 0;                        /* keine FDISK-Merker in Sektor 2 schreiben */
    flags.lba_marker = TRUE;
    flags.set_any_pri_part_active = TRUE;
    flags.total_number_hard_disks = 255;
    flags.use_extended_int_13 = TRUE;
    for (i = 0; i < MAX_DISKS; i++) user_defined_chs_settings[i].defined = FALSE;
    Determine_DOS_Version();
    /* Immer wie DOS 6: nur FAT12/FAT16 (bis 2 GB je Laufwerk), keine LBA-Typen 0Eh/0Fh, keine INT-13h-
       Erweiterungen. So bleibt die Platte auch fuer MS-DOS 5 lesbar (SYS A: nach dem Tausch des Kerns);
       der FreeDOS-Kern meldet sich als DOS 7.10, das allein reicht nicht. Platten ueber 8 GB: nur der
       per CHS erreichbare Teil. */
    flags.version = COMP_SIX;
    flags.fat32 = FALSE;
    Initialize_LBA_Structures();
    if ((fg_fehler = Read_Partition_Tables()) != 0) return 0;
    Determine_Drive_Letters();
    return flags.maximum_drive_number >= 0x80 ? flags.maximum_drive_number - 0x7F : 0;   /* hoechste Nummer 80h.. -> Anzahl */
}
void fg_platte(int n) { platte = n; flags.drive_number = 0x80 + n; }
unsigned long fg_plattemb(void) { return part_table[platte].disk_size_mb; }
unsigned long fg_plattesek(void) { return part_table[platte].disk_size_sect; }
int fg_geaendert(void) { return flags.partitions_have_changed; }

static unsigned long lba(Partition_Table *d, unsigned long c, unsigned long h, unsigned long s) { return chs_to_lba(d, c, h, s); }
static unsigned long zylsek(Partition_Table *d) { return (d->total_head + 1) * d->total_sect; }

int fg_liste(FG_TEIL *t)
{
    Partition_Table *d = &part_table[platte];
    Partition *p;
    int n = 0, i, j;
    FG_TEIL x;
    if (!d->usable) return 0;
    Determine_Free_Space();
    Determine_Drive_Letters();
    for (i = 0; i < 4 && n < FG_MAX; i++) {
        p = &d->pri_part[i];
        if (!p->num_type) continue;
        t[n].art = Is_Supp_Ext_Part(p->num_type) ? FG_EXT : FG_PRI;
        t[n].nr = (unsigned char)i; t[n].typ = (unsigned char)p->num_type; t[n].aktiv = (p->active_status & 0x80) != 0;
        t[n].lw = t[n].art == FG_PRI ? drive_lettering_buffer[platte][i] : 0;
        t[n].dos = (unsigned char)IsRecognizedFatPartition(p->num_type);
        t[n].start = p->rel_sect; t[n].anzahl = p->num_sect; t[n].mb = p->size_in_MB;
        n++;
    }
    if (d->ptr_ext_part && d->ext_usable) for (i = 0; i < MAX_LOGICAL_DRIVES && n < FG_MAX; i++) {
        p = &d->log_drive[i];
        if (!p->num_type) continue;
        t[n].art = FG_LOG; t[n].nr = (unsigned char)i; t[n].typ = (unsigned char)p->num_type; t[n].aktiv = 0;
        t[n].lw = drive_lettering_buffer[platte][4 + i];
        t[n].dos = (unsigned char)IsRecognizedFatPartition(p->num_type);
        /* M8: Lage aus der EMBR-Kette (rel_sect), nicht aus CHS (ab 1024 Zylindern Hoechstwerte) */
        t[n].start = d->ptr_ext_part->rel_sect + (i ? d->next_ext[i - 1].rel_sect : 0) + p->rel_sect;
        t[n].anzahl = p->num_sect; t[n].mb = p->size_in_MB;
        n++;
    }
    if (d->pri_free_space && n < FG_MAX && Max_Pri_Part_Size_In_MB(EXTENDED) > 0) {
        t[n].art = FG_FREIP; t[n].nr = 0; t[n].typ = 0; t[n].aktiv = 0; t[n].lw = 0; t[n].dos = 0;
        t[n].start = lba(d, d->free_start_cyl, 0, 1); t[n].anzahl = d->pri_free_space * zylsek(d);
        t[n].mb = Convert_Cyl_To_MB(d->pri_free_space, d->total_head + 1, d->total_sect);
        if (t[n].start == 0) t[n].start = d->total_sect;   /* Zylinder 0: ab Spur 1 */
        n++;
    }
    if (d->ptr_ext_part && d->ext_usable && d->ext_free_space && n < FG_MAX && Max_Log_Part_Size_In_MB() > 0) {
        t[n].art = FG_FREIL; t[n].nr = 0; t[n].typ = 0; t[n].aktiv = 0; t[n].lw = 0; t[n].dos = 0;
        t[n].start = lba(d, d->log_start_cyl, 0, 1); t[n].anzahl = d->ext_free_space * zylsek(d);
        t[n].mb = Convert_Cyl_To_MB(d->ext_free_space, d->total_head + 1, d->total_sect);
        n++;
    }
    for (i = 1; i < n; i++) for (j = i; j > 0; j--) {       /* nach Lage; logische hinter ihrer erweiterten */
        unsigned long a = t[j - 1].start * 2 + (t[j - 1].art == FG_EXT ? 0 : 1), b = t[j].start * 2 + (t[j].art == FG_EXT ? 0 : 1);
        if (a <= b) break;
        x = t[j]; t[j] = t[j - 1]; t[j - 1] = x;
    }
    return n;
}

static int dos_primaer(Partition_Table *d)
{
    int i;
    for (i = 0; i < 4; i++) if (d->pri_part[i].num_type && Is_Dos_Part(d->pri_part[i].num_type)) return 1;
    return 0;
}
unsigned long fg_maxmb(const FG_TEIL *t, int *alslog)
{
    Partition_Table *d = &part_table[platte];
    *alslog = 1;
    if (t->art == FG_FREIL) return Max_Log_Part_Size_In_MB();
    if (t->art != FG_FREIP) return 0;
    if (!dos_primaer(d)) { *alslog = 0; return Max_Pri_Part_Size_In_MB(PRIMARY); }
    if (d->ptr_ext_part) return 0;                /* nur eine erweiterte Partition */
    { unsigned long m = Max_Pri_Part_Size_In_MB(EXTENDED); return m > MAXFAT16NORM && !flags.fat32 ? MAXFAT16NORM : m; }
}
int fg_anlegen(const FG_TEIL *t, unsigned long mb)
{
    Partition_Table *d = &part_table[platte];
    int alslog, slot; unsigned long m = fg_maxmb(t, &alslog);
    if (!m) return fg_fehler = 1;
    if (mb > m) mb = m;
    if (!alslog) {
        slot = Create_Primary_Partition(Partition_Type_To_Create(mb, 0), mb);
        if (slot < 0 || slot > 3) return fg_fehler = 2;
        Set_Active_Partition_If_None_Is_Active(slot);
        return 0;
    }
    if (t->art == FG_FREIP) {                     /* erweiterte ueber den ganzen freien Platz, darin das Laufwerk */
        slot = Create_Primary_Partition(5, Max_Pri_Part_Size_In_MB(EXTENDED));
        if (slot < 0 || slot > 3 || !d->ptr_ext_part) return fg_fehler = 3;
        m = Max_Log_Part_Size_In_MB();
        if (mb > m) mb = m;
    }
    if (Create_Logical_Drive(Partition_Type_To_Create(mb, 0), mb) != 0) return fg_fehler = 4;
    return 0;
}
int fg_loeschen(const FG_TEIL *t)
{
    Partition_Table *d = &part_table[platte];
    int i, e;
    if (t->art == FG_PRI) return fg_fehler = Delete_Primary_Partition(t->nr);
    if (t->art == FG_LOG) return fg_fehler = Delete_Logical_Drive(t->nr);
    if (t->art != FG_EXT) return fg_fehler = 1;
    for (i = MAX_LOGICAL_DRIVES - 1; i >= 0; i--)
        if (d->log_drive[i].num_type && (e = Delete_Logical_Drive(i)) != 0) return fg_fehler = e;
    return fg_fehler = Delete_Extended_Partition();
}
int fg_aktiv(const FG_TEIL *t) { return fg_fehler = (t->art == FG_PRI && Set_Active_Partition(t->nr)) ? 0 : 1; }
int fg_schreiben(void) { return fg_fehler = Write_Partition_Tables(); }

/* Textausgabe des Kerns (nur Fehlerfaelle): ins RD2-Protokoll */
void con_print(const char *s) { while (*s) rd_putch(*s++); }
int con_printf(const char *f, ...)                /* nur Fehlerfaelle: Formatzeile unveraendert (vsprintf zoege stdio und cstart herein) */
{
    con_print(f);
    return 0;
}
