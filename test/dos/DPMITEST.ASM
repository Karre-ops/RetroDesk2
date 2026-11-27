; DPMITEST.ASM - Testprogramm fuer test/rd2v.sh (PLAN_V86 Teil 1a): 16-Bit-DPMI-Client.
; Holt den Einsprung (2F/1687h), schaltet in den Schutzmodus, prueft PE, Ring 3, INT 31h 0400h und einen
; gespiegelten BIOS-Aufruf (INT 12h), endet mit INT 21h 4C00h. Ausgabe nach COM1 (IOPL 3: OUT erlaubt).
cpu 386
org 100h
        mov  si, m_start
        call puts
        mov  ax, 1687h
        int  2Fh
        test ax, ax
        jnz  kein
        mov  [ent], di
        mov  [ent+2], es
        xor  ax, ax                   ; 16-Bit-Client
        call far [ent]
        jc   fehl
        smsw ax
        test al, 1
        jz   fehl
        mov  si, m_pm
        call puts
        mov  ax, cs
        and  al, 3
        cmp  al, 3
        jne  fehl
        mov  si, m_ring
        call puts
        cmp  byte [es:82h], 'A'       ; DPMITEST A: privilegierter Befehl, RD2V muss den Client beenden
        jne  .weiter
        mov  si, m_ausn
        call puts
        hlt
.weiter:
        mov  ax, 0400h
        int  31h
        jc   fehl
        cmp  ax, 005Ah
        jne  fehl
        mov  si, m_ver
        call puts
        int  12h                      ; gespiegelt: Groesse des Hauptspeichers in KB
        cmp  ax, 500
        jb   fehl
        mov  si, m_int
        call puts
        mov  ax, 0FFFFh               ; unbekannte Funktion: CF und 8001h
        int  31h
        jnc  fehl
        cmp  ax, 8001h
        jne  fehl
        mov  si, m_unb
        call puts
        mov  si, m_ende
        call puts
        mov  ax, 4C00h
        int  21h
kein:   mov  si, m_kein
        call puts
        mov  ax, 4C02h
        int  21h
fehl:   mov  si, m_fehl
        call puts
        mov  ax, 4C01h
        int  21h

puts:   lodsb
        test al, al
        jz   .aus
        mov  ah, al
        mov  dx, 3FDh
.warte: in   al, dx
        test al, 20h
        jz   .warte
        mov  al, ah
        mov  dx, 3F8h
        out  dx, al
        jmp  puts
.aus:   ret

ent:     dd 0
m_start: db "DPMITEST: Start", 13, 10, 0
m_pm:    db "DPMITEST: Schutzmodus", 13, 10, 0
m_ring:  db "DPMITEST: Ring 3", 13, 10, 0
m_ver:   db "DPMITEST: Version 0.90", 13, 10, 0
m_int:   db "DPMITEST: INT 12h gespiegelt", 13, 10, 0
m_unb:   db "DPMITEST: unbekannte Funktion abgelehnt", 13, 10, 0
m_ausn:  db "DPMITEST: HLT in Ring 3", 13, 10, 0
m_ende:  db "DPMITEST: Ende", 13, 10, 0
m_kein:  db "DPMITEST: kein DPMI-Host", 13, 10, 0
m_fehl:  db "DPMITEST: FEHLER", 13, 10, 0
