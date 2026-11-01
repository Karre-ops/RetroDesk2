/* RD2ORD.H - Funktionsnummern als Konstanten RDO_<Name> (aus RD2API.H) */
/* SPDX-License-Identifier: GPL-2.0-or-later  (c) 2026 Karsten, siehe COPYING */
#ifndef RD2ORD_H
#define RD2ORD_H
enum {
#define RDF(n, ret, name, args) RDO_##name = n,
#include "RD2API.H"
#undef RDF
    RDO_NETHAKEN = 0x143,   /* kein Aufruf: Zeiger auf k_netpause fuer RDTCP.MOD */
    RDO_MAX = 0x160
};
#endif
