#ifndef DEVF3RP61_H
#define DEVF3RP61_H

//
#include "drvF3RP61.h"
#include "devF3RP61util.h"

//
typedef struct {
    uint8_t   conv;    // conversion specifier
    // Device for I/O
    uint8_t   device;  // device type
    uint8_t   unit;    // unit number     (0, 1, ..., 7)
    uint8_t   slot;    // slot number     (1, 2, ..., 16)
                       // or CPU number   (1, 2, 3, 4) for shared memory('r')
    uint32_t  addr;    // position number (0, ....)
    uint8_t   count;   // data width      (1, 2, or 4)
    // Source of I/O interrupt
    uint8_t   irqunit; // unit number     (0, 1, ..., 7)
    uint8_t   irqslot; // slot number     (1, 2, ..., 16)
    uint8_t   irqaddr; // position number (1, 2, ..., 32)
    //
    uint32_t  nord;    // number of elemetns to be read from or written to the PV (within the hardware limitations)
    //
    void     *buf;     // buffer for I/O
} F3RP61_DPVT;

//
#define PARSE_ERROR -1
#define OPTION_ERROR -2

// helper function(s)
long    f3rp61Init(int after);
int     f3rp61ParseLink(const struct link *, F3RP61_RW, F3RP61_ACCESS_TYPE, dbCommon *, const dbfType, const uint32_t);
int32_t f3rp61Read(const dbCommon *, const int32_t);
int32_t f3rp61Write(const dbCommon *, const int32_t);

#endif // DRVF3RP61_H
