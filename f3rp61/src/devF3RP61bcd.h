#ifndef DEVF3RP61BCD_H
#define DEVF3RP61BCD_H

#include <stdint.h>

uint32_t devF3RP61bcd2int(uint16_t bcd, void *precord);
uint16_t devF3RP61int2bcd(int32_t dec, void *precord);
//int devF3RP61bcd2int(int bcd);

#define BCDMAX_BCD 39321 // 0x9999
#define BCDMAX_INT  9999 // 0x9999

#define BCDMIN_BCD     0 // 0x9999
#define BCDMIN_INT     0 //

#endif

