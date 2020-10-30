/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Reseach Organization (KEK)
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

uint32_t devF3RP61bcd2int(uint16_t bcd, void *precord)
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

uint16_t devF3RP61int2bcd(int32_t dec, void *precord)
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
