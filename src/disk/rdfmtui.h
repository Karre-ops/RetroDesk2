/* rdfmtui.h - Anbindung von FreeDOS FORMAT an die RD2-Oberflaeche (RDFORMAT.C) */
void rd_einlegen(void);                           /* Ask_User_To_Insert_Disk */
void rd_bestaetigen(int format);                  /* Confirm_Hard_Drive_Formatting */
int  rd_label(char *str);                         /* Ask_Label */
void rd_prozent(unsigned long p);                 /* Display_Percentage_Formatted */
void rd_kritisch(int quelle, unsigned int code);  /* Critical_Error_Handler */
