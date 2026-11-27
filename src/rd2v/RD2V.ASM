; RD2V.ASM - 32-Bit-Unterbau von RetroDesk 2 ab 386 (PLAN_V86), GPL v2 oder spaeter.
; Plan-Teil 1a: residenter DPMI-Host (Grundgeruest). INT 2Fh 1687h liefert den Einsprung, der Einsprung schaltet
; den Aufrufer in den Schutzmodus (Ring 3, 16-Bit-Selektoren fuer CS, DS, SS und PSP in der LDT).
; Software-Interrupts des Clients laufen ueber #GP (alle Gates DPL 0): INT 31h erledigt RD2V selbst, alle anderen
; werden in den Realmodus gespiegelt (Register hin und zurueck, Rechenflags zurueck). IRQs ebenso; der Master-PIC
; liegt im Schutzmodus auf 50h, im Realmodus wieder auf 08h. Ausnahmen beenden den Client (INT 21h 4CFFh).
; Noch nicht: Paging, Speicher (0501h), weitere DPMI-Funktionen, 32-Bit-Clients (Plan-Teil 1b bis 1d).
cpu 386
org 100h
LDTN equ 16
PICPM equ 50h

section .text
bits 16
start:  jmp install

; ---------------------------------------------------------------- residente Daten
align 8
gdt:      dq 0                                          ; 00
g_code32: dw 0FFFFh, 0
          db 0, 9Ah, 40h, 0                             ; 08 Host-Code 32 Bit (Basis = Host)
g_data32: dw 0FFFFh, 0
          db 0, 92h, 40h, 0                             ; 10 Host-Daten
g_flat:   dw 0FFFFh, 0
          db 0, 92h, 0CFh, 0                            ; 18 flach 4 GB
g_code16: dw 0FFFFh, 0
          db 0, 9Ah, 00h, 0                             ; 20 Host-Code 16 Bit (Rueckweg)
g_data16: dw 0FFFFh, 0
          db 0, 92h, 00h, 0                             ; 28 Host-Daten 16 Bit (Rueckweg)
g_tss:    dw 103, 0
          db 0, 89h, 0, 0                               ; 30 TSS
g_ldt:    dw LDTN*8-1, 0
          db 0, 82h, 0, 0                               ; 38 LDT
gdt_end:
gdtr:     dw gdt_end-gdt-1
          dd 0
pm_idtr:  dw 256*8-1
          dd 0
rm_idtr:  dw 3FFh
          dd 0
align 8
ldt:      times LDTN dq 0
align 4
tss:      times 104 db 0

old2f:    dd 0
active:   db 0
align 4
hesp:     dd 0
rvec:     dd 0
r_eax:    dd 0
r_ebx:    dd 0
r_ecx:    dd 0
r_edx:    dd 0
r_esi:    dd 0
r_edi:    dd 0
r_ebp:    dd 0
r_fl:     dw 0
c_ip:     dw 0
c_cs:     dw 0
c_ds:     dw 0
c_ss:     dw 0
c_sp:     dw 0
c_psp:    dw 0
m_ausn:   db "RD2V: Ausnahme, Client beendet", 13, 10, 0

; ---------------------------------------------------------------- PIC umlegen (16 und 32 Bit gleiche Bytes)
%macro PICBASE 1
        in   al, 21h
        mov  ah, al
        mov  al, 11h
        out  20h, al
        mov  al, %1
        out  21h, al
        mov  al, 04h
        out  21h, al
        mov  al, 01h
        out  21h, al
        mov  al, ah
        out  21h, al
%endmacro

; ---------------------------------------------------------------- Realmodus: INT 2Fh
int2f:  cmp  ax, 1687h
        jne  .weiter
        xor  ax, ax
        mov  bx, 1                    ; 32-Bit-Clients (ab Plan-Teil 1d)
        mov  cl, 3
        mov  dx, 005Ah                ; DPMI 0.90
        xor  si, si                   ; keine Host-Daten im Client
        push cs
        pop  es
        mov  di, entry
        iret
.weiter: jmp far [cs:old2f]

; ---------------------------------------------------------------- Realmodus: Einsprung (far call des Clients)
entry:  cmp  byte [cs:active], 0
        je   .frei
        stc
        retf                          ; nur ein Client gleichzeitig (Plan-Teil 1a)
.frei:  mov  [cs:c_ss], ss
        mov  [cs:c_ds], ds
        push bp
        mov  bp, sp
        mov  ax, [bp+2]
        mov  [cs:c_ip], ax
        mov  ax, [bp+4]
        mov  [cs:c_cs], ax
        pop  bp
        add  sp, 4
        mov  [cs:c_sp], sp
        mov  ah, 62h
        int  21h
        mov  [cs:c_psp], bx
        cli
        mov  ax, cs
        mov  ds, ax
        movzx eax, word [c_cs]
        shl  eax, 4
        mov  di, ldt+8
        mov  dl, 0FAh                 ; Code 16 Bit, DPL 3
        call mkdesc16
        movzx eax, word [c_ds]
        shl  eax, 4
        mov  di, ldt+16
        mov  dl, 0F2h                 ; Daten, DPL 3
        call mkdesc16
        movzx eax, word [c_ss]
        shl  eax, 4
        mov  di, ldt+24
        call mkdesc16
        movzx eax, word [c_psp]
        shl  eax, 4
        mov  di, ldt+32
        call mkdesc16
        mov  byte [active], 1
        o32 lgdt [gdtr]
        o32 lidt [pm_idtr]
        PICBASE PICPM
        mov  eax, cr0
        or   al, 1
        mov  cr0, eax
        jmp  dword 08h:pm_entry

mkdesc16:                             ; DI = Deskriptor, EAX = Basis, DL = Zugriff; Grenze FFFFh, Bytes, 16 Bit
        mov  word [di], 0FFFFh
        mov  [di+2], ax
        shr  eax, 16
        mov  [di+4], al
        mov  [di+5], dl
        mov  byte [di+6], 0
        mov  [di+7], ah
        ret

; ---------------------------------------------------------------- Schutzmodus
bits 32
pm_entry:
        mov  ax, 10h
        mov  ds, ax
        mov  es, ax
        mov  ss, ax
        mov  esp, hstk_top
        and  byte [g_tss+5], 0FDh     ; Busy-Bit loeschen, sonst #GP bei LTR
        mov  ax, 30h
        ltr  ax
        mov  ax, 38h
        lldt ax
        xor  eax, eax
        mov  fs, ax
        mov  gs, ax
        push dword 1Fh                ; SS (LDT 3, RPL 3)
        movzx eax, word [c_sp]
        push eax
        push dword 3202h              ; IF, IOPL 3 (Ports frei, PLAN_V86)
        push dword 0Fh                ; CS (LDT 1, RPL 3)
        movzx eax, word [c_ip]
        push eax
        mov  ax, 27h                  ; ES = PSP
        mov  es, ax
        mov  ax, 17h                  ; DS
        mov  ds, ax
        xor  eax, eax
        iretd

; Rahmen auf dem Host-Stapel (EBP): gs 0, fs 4, es 8, ds 12, edi 16, esi 20, ebp 24, esp 28, ebx 32, edx 36,
; ecx 40, eax 44, vec 48, err 52, eip 56, cs 60, eflags 64, uesp 68, uss 72
%assign i 0
%rep 32
excstub_ %+ i:
%if i = 8 || (i >= 10 && i <= 14) || i = 17
        push dword i
%else
        push dword 0
        push dword i
%endif
        jmp  exc_common
%assign i i+1
%endrep
unkstub: push dword 0
        push dword 0FFh
        jmp  exc_common
%assign i 0
%rep 8
irqm_ %+ i: push dword 0
        push dword 8+i
        jmp  irq_common
%assign i i+1
%endrep
%assign i 0
%rep 8
irqs_ %+ i: push dword 0
        push dword 70h+i
        jmp  irq_common
%assign i i+1
%endrep

exc_common:
        pushad
        push ds
        push es
        push fs
        push gs
        mov  ax, 10h
        mov  ds, ax
        mov  es, ax
        mov  ebp, esp
        mov  eax, [ebp+48]
        cmp  eax, 13
        jne  .fehler
        test byte [ebp+60], 3
        jz   .fehler
        movzx eax, word [ebp+60]
        call selbase
        add  eax, [ebp+56]
        mov  bx, 18h
        mov  es, bx
        mov  dx, [es:eax]
        mov  bx, 10h
        mov  es, bx
        cmp  dl, 0CDh                 ; INT nn
        jne  .fehler
        add  dword [ebp+56], 2
        movzx eax, dh
        cmp  al, 31h
        je   dpmi31
        cmp  al, 21h
        jne  .spiegeln
        cmp  byte [ebp+45], 4Ch       ; Ende des Clients
        jne  .spiegeln
        mov  byte [active], 0
.spiegeln:
        mov  [rvec], eax
        call reflect
        jmp  iret_out
.fehler:                              ; Ausnahme: melden (COM1), Client mit Code FFh beenden
        mov  esi, m_ausn
.melde: lodsb
        test al, al
        jz   .gemeldet
        mov  ah, al
        mov  dx, 3FDh
.warte: in   al, dx
        test al, 20h
        jz   .warte
        mov  al, ah
        mov  dx, 3F8h
        out  dx, al
        jmp  .melde
.gemeldet:
        mov  dword [ebp+44], 4CFFh
        mov  byte [active], 0
        mov  dword [rvec], 21h
        call reflect                  ; kehrt nicht zurueck
iret_out:
        pop  gs
        pop  fs
        pop  es
        pop  ds
        popad
        add  esp, 8
        iretd

irq_common:
        pushad
        push ds
        push es
        push fs
        push gs
        mov  ax, 10h
        mov  ds, ax
        mov  es, ax
        mov  ebp, esp
        mov  eax, [ebp+48]
        mov  [rvec], eax
        call reflect
        jmp  iret_out

selbase:                              ; EAX = Selektor -> EAX = Basis
        push ebx
        push edx
        mov  ebx, eax
        and  ebx, 0FFF8h
        test al, 4
        jz   .gdt
        add  ebx, ldt
        jmp  .lesen
.gdt:   add  ebx, gdt
.lesen: mov  eax, [ebx+2]
        and  eax, 00FFFFFFh
        movzx edx, byte [ebx+7]
        shl  edx, 24
        or   eax, edx
        pop  edx
        pop  ebx
        ret

dpmi31: mov  ax, [ebp+44]
        cmp  ax, 0400h                ; Version
        jne  .unbekannt
        mov  word [ebp+44], 005Ah
        mov  word [ebp+32], 0001h
        mov  byte [ebp+40], 3
        mov  word [ebp+36], 0870h
        and  byte [ebp+64], 0FEh
        jmp  iret_out
.unbekannt:
        mov  word [ebp+44], 8001h
        or   byte [ebp+64], 1
        jmp  iret_out

reflect:                              ; INT [rvec] im Realmodus mit den Registern aus dem Rahmen (EBP)
        mov  [hesp], esp
        mov  eax, [ebp+44]
        mov  [r_eax], eax
        mov  eax, [ebp+32]
        mov  [r_ebx], eax
        mov  eax, [ebp+40]
        mov  [r_ecx], eax
        mov  eax, [ebp+36]
        mov  [r_edx], eax
        mov  eax, [ebp+20]
        mov  [r_esi], eax
        mov  eax, [ebp+16]
        mov  [r_edi], eax
        mov  eax, [ebp+24]
        mov  [r_ebp], eax
        mov  ax, [ebp+64]
        mov  [r_fl], ax
        mov  ax, 28h
        mov  ds, ax
        mov  es, ax
        mov  fs, ax
        mov  gs, ax
        mov  ss, ax
        jmp  20h:p16
bits 16
p16:    mov  eax, cr0
        and  al, 0FEh
        mov  cr0, eax
        db   0EAh
        dw   rmpart
rmseg1: dw   0
rmpart: mov  ax, cs
        mov  ds, ax
        mov  ss, ax
        mov  sp, rmstk_top
        o32 lidt [rm_idtr]
        PICBASE 08h
        xor  ax, ax
        mov  es, ax
        mov  bx, [rvec]
        shl  bx, 2
        push word [r_fl]              ; Flags fuer das IRET des Handlers
        push cs
        push word rm_ret
        push word [es:bx+2]
        push word [es:bx]
        push cs
        pop  es
        mov  eax, [r_eax]
        mov  ebx, [r_ebx]
        mov  ecx, [r_ecx]
        mov  edx, [r_edx]
        mov  esi, [r_esi]
        mov  edi, [r_edi]
        mov  ebp, [r_ebp]
        retf                          ; wie INT: IF und TF aus
rm_ret: cli
        pushf
        pop  word [cs:r_fl]
        mov  [cs:r_eax], eax
        mov  [cs:r_ebx], ebx
        mov  [cs:r_ecx], ecx
        mov  [cs:r_edx], edx
        mov  [cs:r_esi], esi
        mov  [cs:r_edi], edi
        mov  [cs:r_ebp], ebp
        mov  ax, cs
        mov  ds, ax
        PICBASE PICPM
        o32 lidt [pm_idtr]
        mov  eax, cr0
        or   al, 1
        mov  cr0, eax
        jmp  dword 08h:pm_back
bits 32
pm_back:
        mov  ax, 10h
        mov  ds, ax
        mov  es, ax
        mov  ss, ax
        mov  esp, [hesp]
        lea  ebp, [esp+4]
        mov  eax, [r_eax]
        mov  [ebp+44], eax
        mov  eax, [r_ebx]
        mov  [ebp+32], eax
        mov  eax, [r_ecx]
        mov  [ebp+40], eax
        mov  eax, [r_edx]
        mov  [ebp+36], eax
        mov  eax, [r_esi]
        mov  [ebp+20], eax
        mov  eax, [r_edi]
        mov  [ebp+16], eax
        mov  eax, [r_ebp]
        mov  [ebp+24], eax
        mov  ax, [r_fl]
        and  ax, 08D5h                ; CF PF AF ZF SF OF vom Realmodus-Handler
        mov  dx, [ebp+64]
        and  dx, 0F72Ah
        or   dx, ax
        mov  [ebp+64], dx
        ret

bits 16
align 4
idt:      times 256 dq 0
exctab:
%assign i 0
%rep 32
          dd excstub_ %+ i
%assign i i+1
%endrep
irqtab:
%assign i 0
%rep 8
          dd irqm_ %+ i
%assign i i+1
%endrep
%assign i 0
%rep 8
          dd irqs_ %+ i
%assign i i+1
%endrep
align 4
hstk:     times 2048 db 0
hstk_top:
rmstk:    times 2048 db 0
rmstk_top:
resident_end:

; ---------------------------------------------------------------- Installation (wird nicht resident)
install:
        mov  si, m_titel
        call puts
        pushf                         ; 8086: Bits 12-15 immer gesetzt
        pop  ax
        and  ax, 0FFFh
        push ax
        popf
        pushf
        pop  ax
        and  ax, 0F000h
        cmp  ax, 0F000h
        je   .kein386
        mov  ax, 7000h                ; 286: Bits 12-14 im Realmodus immer 0
        push ax
        popf
        pushf
        pop  ax
        test ax, 7000h
        jz   .kein386
        smsw ax
        test al, 1
        jnz  .v86
        mov  ax, 1687h
        int  2Fh
        test ax, ax
        jz   .schon
        mov  ax, cs
        mov  [rmseg1], ax
        movzx eax, ax
        shl  eax, 4
        mov  ebx, eax
        mov  di, g_code32
        call setbase
        mov  di, g_data32
        call setbase
        mov  di, g_code16
        call setbase
        mov  di, g_data16
        call setbase
        lea  eax, [ebx+tss]
        mov  di, g_tss
        call setbase
        lea  eax, [ebx+ldt]
        mov  di, g_ldt
        call setbase
        lea  eax, [ebx+gdt]
        mov  [gdtr+2], eax
        lea  eax, [ebx+idt]
        mov  [pm_idtr+2], eax
        mov  dword [tss+4], hstk_top
        mov  word [tss+8], 10h
        mov  word [tss+102], 104
        xor  cx, cx                   ; IDT: alles nach unkstub, dann Ausnahmen und IRQs
.alle:  mov  eax, unkstub
        call setgate
        inc  cx
        cmp  cx, 256
        jb   .alle
        xor  cx, cx
.exc:   mov  si, cx
        shl  si, 2
        mov  eax, [exctab+si]
        call setgate
        inc  cx
        cmp  cx, 32
        jb   .exc
        xor  dx, dx
.irq:   mov  si, dx
        shl  si, 2
        mov  eax, [irqtab+si]
        mov  cx, dx
        add  cx, PICPM
        cmp  dx, 8
        jb   .setz
        add  cx, 70h-PICPM-8
.setz:  call setgate
        inc  dx
        cmp  dx, 16
        jb   .irq
        mov  ax, 352Fh
        int  21h
        mov  [old2f], bx
        mov  [old2f+2], es
        mov  dx, int2f
        mov  ax, 252Fh
        int  21h
        mov  si, m_ok
        call puts
        mov  es, [2Ch]
        mov  ah, 49h
        int  21h
        mov  dx, (resident_end-start+100h+15)/16
        mov  ax, 3100h
        int  21h
.kein386: mov si, m_386
        jmp  .ende
.v86:   mov  si, m_v86
        jmp  .ende
.schon: mov  si, m_schon
.ende:  call puts
        mov  ax, 4C01h
        int  21h

setbase:                              ; DI = Deskriptor, EAX = Basis (EBX bleibt)
        push eax
        mov  [di+2], ax
        shr  eax, 16
        mov  [di+4], al
        mov  [di+7], ah
        pop  eax
        ret
setgate:                              ; CX = Vektor, EAX = Offset im Host-Code
        push di
        mov  di, cx
        shl  di, 3
        add  di, idt
        mov  [di], ax
        mov  word [di+2], 08h
        mov  byte [di+4], 0
        mov  byte [di+5], 8Eh         ; 32-Bit-Interrupt-Gate, DPL 0
        shr  eax, 16
        mov  [di+6], ax
        pop  di
        ret
puts:                                 ; DS:SI, Ende 0; auf den Bildschirm (DOS) und nach COM1
        lodsb
        test al, al
        jz   .aus
        push ax
        mov  dl, al
        mov  ah, 2
        int  21h
        mov  dx, 3FDh
.warte: in   al, dx
        test al, 20h
        jz   .warte
        pop  ax
        mov  dx, 3F8h
        out  dx, al
        jmp  puts
.aus:   ret

m_titel: db "RD2V 0.1 (PLAN_V86 Teil 1a)", 13, 10, 0
m_ok:    db "RD2V: DPMI-Host geladen", 13, 10, 0
m_386:   db "RD2V: braucht einen 386", 13, 10, 0
m_v86:   db "RD2V: Prozessor schon im V86-Modus (EMM386, QEMM?), nicht geladen", 13, 10, 0
m_schon: db "RD2V: schon ein DPMI-Host aktiv, nicht geladen", 13, 10, 0
