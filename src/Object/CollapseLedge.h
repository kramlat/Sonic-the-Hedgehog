#ifndef _COLLAPSELEDGE_H
#define _COLLAPSELEDGE_H

#include "Sst.h"


typedef struct {
    uint8_t pad0[0x10]; // 0x28-0x37 (FragmentatePlatform writes each piece's delay at own.u8[SST_COLLAPSE_DELAY] = 0x10: the objects keep their timedelay there)
    uint8_t timedelay;  // 0x38 -- delay before fragment starts to fall (also written directly by FragmentatePlatform, see its own comment)
    uint8_t pad1;       // 0x39
    uint8_t flag;       // 0x3A -- set once collapsing has started
} Own_CollapseLedge;

void Obj_CollapseLedge(Sst *self);

#endif //_COLLAPSELEDGE_H
