; OBEN.COM - Testprogramm fuer test/rundos.sh (PLAN_DOS Schritt 1, 10.11.2026): verhaelt sich wie
; manche Spiele und Entpacker, ignoriert die DOS-Zuteilung und beschreibt den ganzen Speicher
; hinter sich bis A000h. Endet mit Code 42. Lag der RunDos-Rest dort, kam RD2 nicht zurueck.
        org  100h
        mov  ax, cs
        add  ax, 1000h                   ; hinter dem eigenen 64-KB-Segment
        cld
fill:   cmp  ax, 0A000h
        jae  done
        mov  es, ax
        xor  di, di
        mov  cx, 8
        push ax
        mov  ax, 0CCCCh
        rep  stosw
        pop  ax
        inc  ax
        jmp  fill
done:   mov  ax, 4C2Ah
        int  21h
