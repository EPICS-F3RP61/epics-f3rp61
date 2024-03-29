/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 2.0.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devF3RP61bcd.c - BCD Helper Routines for F3RP61
*
*      Author: Shuei YAMADA (KEK/J-PARC)
*      Date: 2020 Oct. 01
*/

//
#include <stdint.h>

//
#include <alarm.h>
#include <recGbl.h>

//
#include <devF3RP61bcd.h>

//
#define BCDMAX_BCD 39321 // 0x9999
#define BCDMAX_INT  9999 // 0x9999

#define BCDMIN_BCD     0 // 0x9999
#define BCDMIN_INT     0 //


uint32_t devF3RP61bcd2int(uint16_t bcd, longinRecord *precord)
{
    uint32_t base = 1;
    uint32_t dec  = 0;

    //if (bcd>BCDMAX_BCD) {
    //    recGblSetSevr(precord, HIGH_ALARM, INVALID_ALARM);
    //    return BCDMAX_INT;
    //}

    while (bcd>0) {
        int digit = bcd & 0x000f;
        if (digit <= 9) {
            dec += digit * base;
        } else {
            // overflow
            recGblSetSevr(precord, HIGH_ALARM, INVALID_ALARM);
            dec += 9 * base;
        }
        bcd >>= 4;
        base *= 10;
    }

    return dec;
}

uint16_t devF3RP61int2bcd(int32_t dec, longoutRecord *precord)
{
    if (dec<BCDMIN_INT) {
        recGblSetSevr(precord, HW_LIMIT_ALARM, INVALID_ALARM);
        return BCDMIN_BCD;
    } else if (dec>BCDMAX_INT) {
        recGblSetSevr(precord, HW_LIMIT_ALARM, INVALID_ALARM);
        return BCDMAX_BCD;
    }

    uint32_t base = 0;
    uint16_t bcd  = 0;

    while (dec>0) {
        bcd |= ((dec%10) << base);
        dec /= 10;
        base += 4;
    }

    return bcd;
}
