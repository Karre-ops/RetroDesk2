/* RD2ORD.H - Funktionsnummern als Konstanten RDO_<Name> (aus RD2API.H) */
#ifndef RD2ORD_H
#define RD2ORD_H
enum {
#define RDF(n, ret, name, args) RDO_##name = n,
#include "RD2API.H"
#undef RDF
    RDO_MAX = 0x400
};
#endif
