; DPMITEST.ASM - Testprogramm fuer test/rd2v.sh (PLAN_V86 Teil 1a): 16-Bit-DPMI-Client.
; Holt den Einsprung (2F/1687h), schaltet in den Schutzmodus, prueft PE, Ring 3, INT 31h 0400h und einen
; gespiegelten BIOS-Aufruf (INT 12h), endet mit INT 21h 4C00h. Ausgabe nach COM1 (IOPL 3: OUT erlaubt).
cpu 386
org 100h
        mov  si, m_start
        call puts
        mov  ax, 4300h                ; freies XMS vor dem Start (Pruefung: Bloecke kommen beim Ende zurueck)
        int  2Fh
        cmp  al, 80h
        jne  .ohne
        mov  ax, 4310h
        int  2Fh
        mov  [xent], bx
        mov  [xent+2], es
        mov  ah, 08h
        call far [xent]
        push dx
        mov  si, m_xms
        call puts
        pop  ax
        call putdez
        mov  si, m_crlf
        call puts
.ohne:
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
        mov  ax, 0003h                ; Teil 1b: Selektoren und Speicher
        int  31h
        cmp  ax, 8
        jne  fehl
        xor  ax, ax
        mov  cx, 2
        int  31h
        jc   fehl
        mov  [sel1], ax
        mov  ax, 0501h                ; 64 KB
        mov  bx, 1
        xor  cx, cx
        int  31h
        jc   fehl
        mov  [lin], cx
        mov  [lin+2], bx
        mov  [hdl], di
        mov  [hdl+2], si
        mov  ax, 0007h
        mov  bx, [sel1]
        mov  cx, [lin+2]
        mov  dx, [lin]
        int  31h
        jc   fehl
        mov  ax, 0008h
        mov  bx, [sel1]
        xor  cx, cx
        mov  dx, 0FFFFh
        int  31h
        jc   fehl
        mov  fs, [sel1]
        mov  dword [fs:0FFFCh], 12345678h
        mov  dword [fs:0], 9ABCDEF0h
        cmp  dword [fs:0FFFCh], 12345678h
        jne  fehl
        mov  si, m_spei
        call puts
        cmp  byte [es:82h], 'A'       ; DPMITEST A: privilegierter Befehl mit belegtem Block, RD2V raeumt auf
        jne  .weiter
        mov  si, m_ausn
        call puts
        hlt
.weiter:
        mov  ax, 000Ah                ; Alias des Codes
        mov  bx, cs
        int  31h
        jc   fehl
        mov  gs, ax
        mov  al, [gs:0100h]
        cmp  al, [cs:0100h]
        jne  fehl
        mov  bx, [sel1]               ; zweiter Selektor: 2 MB Grenze, 32 Bit
        add  bx, 8
        mov  ax, 0008h
        mov  cx, 001Fh
        mov  dx, 0FFFFh
        int  31h
        jc   fehl
        movzx ebx, bx
        lsl  eax, ebx
        jnz  fehl
        cmp  eax, 1FFFFFh
        jne  fehl
        mov  ax, 0008h                ; Grenze ueber 1 MB ohne volle Seite: abgelehnt
        mov  cx, 0010h
        mov  dx, 1234h
        int  31h
        jnc  fehl
        mov  ax, 0009h
        mov  cx, 40F2h
        int  31h
        jc   fehl
        mov  ax, 0009h                ; DPL 0: abgelehnt
        mov  cx, 0092h
        int  31h
        jnc  fehl
        mov  si, m_sel
        call puts
        mov  ax, 0001h
        int  31h
        jc   fehl
        mov  ax, 0001h                ; doppelt freigeben: abgelehnt
        int  31h
        jnc  fehl
        mov  ax, 0502h
        mov  di, [hdl]
        mov  si, [hdl+2]
        int  31h
        jc   fehl
        mov  ax, 0502h                ; doppelt: abgelehnt
        int  31h
        jnc  fehl
        mov  si, m_frei
        call puts
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

putdez: mov  bx, 10                   ; AX dezimal nach COM1
        xor  cx, cx
.teil:  xor  dx, dx
        div  bx
        push dx
        inc  cx
        test ax, ax
        jnz  .teil
.aus:   pop  ax
        add  al, '0'
        mov  [ziff], al
        push cx
        mov  si, ziff
        call puts
        pop  cx
        loop .aus
        ret

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
xent:    dd 0
lin:     dd 0
hdl:     dd 0
sel1:    dw 0
ziff:    db 0, 0
m_xms:   db "DPMITEST: XMS frei ", 0
m_crlf:  db 13, 10, 0
m_spei:  db "DPMITEST: Speicher 64 KB beschrieben", 13, 10, 0
m_sel:   db "DPMITEST: Selektoren geprueft", 13, 10, 0
m_frei:  db "DPMITEST: freigegeben", 13, 10, 0
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
