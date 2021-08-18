#ifndef DEVF3RP61BCD_H
#define DEVF3RP61BCD_H

#include <stdint.h>
#include <longinRecord.h>
#include <longoutRecord.h>

uint32_t devF3RP61bcd2int(uint16_t bcd, longinRecord *precord);
uint16_t devF3RP61int2bcd(int32_t dec, longoutRecord *precord);

#endif
