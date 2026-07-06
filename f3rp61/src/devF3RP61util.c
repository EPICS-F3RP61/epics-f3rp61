/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 2.0.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devF3RP61util.c - Helper Routines for F3RP61
*
*      Author: Shuei YAMADA (KEK/J-PARC)
*      Date: 2020-10-01
*/

//
#include <devF3RP61util.h>

//////////////////////////////////////////////////////////////////////////
//
// check conversion specifier validity
// return: 0= OK, -1=NG
//
int f3rp61CheckConversion(F3RP61_ACCESS_TYPE type, const dbfType ftvl, const char conv)
{
    if (type == kBit) {
        // we don't check ftvl for bi/bo records
        if (conv == 'W') {            // Dummy for Word access
        } else {
            return -1;
        }
    } else { // kWord
        if (0) {
        } else if (ftvl == DBF_DOUBLE) {
            if (conv == 'W') {        // Dummy for Word access
            } else if (conv == 'U') { // Unsigned integer
            } else if (conv == 'L') { // Long word
            } else if (conv == 'F') { // Single precision floating point
            } else if (conv == 'D') { // Double precision floating point
            } else {
                return -1;
            }
        } else if (ftvl == DBF_FLOAT) {
            if (conv == 'W') {        // Dummy for Word access
            } else if (conv == 'U') { // Unsigned integer
            } else if (conv == 'L') { // Long word
            } else if (conv == 'F') { // Single precision floating point
            //} else if (conv == 'D') { // Double precision floating point
            } else {
                return -1;
            }
        } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
            if (conv == 'W') {        // Dummy for Word access
            } else if (conv == 'B') { // Binary Coded Decimal format
            } else if (conv == 'U') { // Unsigned integer
            } else if (conv == 'L') { // Long word
#if 0
            } else if (conv == 'X') { // Long word access for XP01/XP02 modules (might be supported in the future)
                if (dtyp == kCPU) {
                    // F3RP71 native APS does not support long-word access
                    return -1;
                }
#endif
            //} else if (conv == 'F') { // Single precision floating point
            //} else if (conv == 'D') { // Double precision floating point
            } else {
                return -1;
            }
        } else if (ftvl == DBF_SHORT || ftvl == DBF_USHORT) {
            if (conv == 'W') {        // Dummy for Word access
            } else if (conv == 'U') { // Unsigned integer
            } else if (conv == 'B') { // Binary Coded Decimal format
            //} else if (conv == 'L') { // Long word
            //} else if (conv == 'F') { // Single precision floating point
            //} else if (conv == 'D') { // Double precision floating point
            } else {
                return -1;
            }
        } else if (ftvl == DBF_STRING) {
            if (conv == 'W') {        // Dummy for Word access
            } else {
                return -1;
            }
        } else { //ftvl == DBF_CHAR || ftvl == DBF_UCHAR
            return -1;
        }
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// check if address range is suitable for the hardware
// return: the number of elements that can be read/written (i.e. NORD)
//
int32_t f3rp61CheckAddrRange(dbCommon *prec, F3RP61_ACCESS_TYPE type, const int addr, int32_t count, const int32_t nelm, const int limit)
{

    const int   width = (type==kBit) ? count*16 : count;
    const char *dev   = (type==kBit) ? "Relay"  : "Register";
    const char *unit  = (type==kBit) ? "bits"   : "words";

    //
    const int end = addr + (width * nelm) - 1;
    if (limit < end) {
        errlogPrintf("%s: %s : %s number %d-%d (%d %s x %d) exceeds the module limit of %d\n", __func__, prec->name, dev, addr, end, width, unit, nelm, limit);
        return 0;
    }

    //
    int32_t nord = (limit - addr + 1) / width;
    if (nord < nelm) {
        errlogPrintf("%s: %s : Warning: NORD shrinked to %d : %s number %d-%d (%d %s x %d) exceeds the module limit of %d\n", __func__, prec->name, nord, dev, addr, end, width, unit, nelm, limit);
        return nord;
    } else {
        return nelm;
    }
}

//////////////////////////////////////////////////////////////////////////
//
#define BCDMAX_BCD 39321 // 0x9999
#define BCDMAX_INT  9999 // 0x9999

#define BCDMIN_BCD     0 // 0x9999
#define BCDMIN_INT     0 //

#define BCD_OVERFLOW -1

// converts bcd to deciaml
static uint16_t bcd2ushort(uint16_t bcd, int32_t *overflow)
{
    *overflow = 0;
    uint16_t base = 1;
    uint16_t dec = 0;

    //fprintf(stderr, "%s : %5d(0x%04x)\n", __func__, bcd, bcd);
    if (bcd>BCDMAX_BCD) {
        *overflow = BCD_OVERFLOW;
        return BCDMAX_INT;
    }

    while (bcd>0) {
        uint16_t digit = bcd & 0x000f;
        //fprintf(stderr, "    %d %d %d (overflow:0x%0x)\n", bcd, digit, base, *overflow);
        if (digit <= 9) {
            dec += digit * base;
        } else {
            // overflow; this may not happen
            *overflow = BCD_OVERFLOW;
            dec += 9 * base;
        }
        bcd >>= 4;
        base *= 10;
    }

    return dec;
}

// converts decimal to bcd
static uint16_t ushort2bcd(uint16_t dec, int32_t *overflow)
{
    if (dec<BCDMIN_INT) {
        // underflow; this may not happen for unsigned value
        *overflow = BCD_OVERFLOW;
        return BCDMIN_BCD;
    } else if (dec>BCDMAX_INT) {
        *overflow = BCD_OVERFLOW;
        return BCDMAX_BCD;
    }

    uint16_t base = 0;
    uint16_t bcd = 0;

    while (dec>0) {
        bcd |= ((dec%10) << base);
        dec /= 10;
        base += 4;
    }

    return bcd;
}

//////////////////////////////////////////////////////////////////////////
//

// read data from buf and fill to val
// return: 0= success, -1= overflow/underflow in bcd2ushort
int32_t devF3RP61buf2long(void *buf, int32_t *val, const int8_t conv, int32_t nord)
{
    int32_t ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    } else if (conv == 'X') { // long word access for XP01/XP02 modules (might be supported in the future)
        ulong *ldata = buf;
        for (int32_t i=0; i<nord; i++) {
#if defined(__powerpc__)
            ulong tmp = ldata[i];
            val[i] = (tmp >> 16) | (tmp << 16); // we need word-swap for F3RP61
#else
            val[i] = ldata[i];
#endif
        }

    } else if (conv == 'L') {
        for (int32_t i=0; i<nord; i++) {
            val[i] = (wdata[2*i+1]<<16) | wdata[2*i+0];
        }

    } else if (conv == 'B') {
        int32_t overflow = 0;
        for (int32_t i=0; i<nord; i++) {
            val[i] = bcd2ushort(wdata[i], &overflow);
            ret |= overflow;
        }

    } else if (conv == 'U') {
        for (int32_t i=0; i<nord; i++) {
            val[i] = (uint16_t)wdata[i];
        }

    } else { // conv == 'W'
        for (int32_t i=0; i<nord; i++) {
            val[i] = (int16_t)wdata[i];
        }
    }

    return ret;
}

// read data from val and fill to buf
// return: 0= success, -1= overflow/underflow in ushort2bcd
int32_t devF3RP61long2buf(int32_t *val, void *buf, const int8_t conv, int32_t nord)
{
    int ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    } else if (conv == 'X') { // long word access for XP01/XP02 modules (might be supported in the future)
        ulong *ldata = buf;
        for (int32_t i=0; i<nord; i++) {
#if defined(__powerpc__)
            //ulong val = (ulong)val[i];
            ulong tmp = val[i];
            ldata[i] = (tmp >> 16) | (tmp << 16); // we need word-swap for F3RP61
#else
            ldata[i] = (uint32_t)val[i];
#endif
        }

    } else if (conv == 'L') {
        for (int32_t i=0; i<nord; i++) {
            int32_t lval = val[i];
            wdata[2*i + 0] = (uint16_t)(lval>> 0);
            wdata[2*i + 1] = (uint16_t)(lval>>16);
        }

    } else if (conv == 'B') {
        int32_t overflow = 0;
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = ushort2bcd(val[i], &overflow);
            ret |= overflow;
        }

    } else if (conv == 'U') {
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = (uint16_t)val[i];
        }

    } else {// conv == 'W'
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = (int16_t)val[i];
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////
//

// read data from buf and fill to val
// return: 0= success, -1= overflow/underflow in bcd2ushort
int32_t devF3RP61buf2ulong(void *buf, uint32_t *val, const int8_t conv, int32_t nord)
{
    int32_t ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    } else if (conv == 'X') { // long word access for XP01/XP02 modules (might be supported in the future)
        ulong *ldata = buf;
        for (int32_t i=0; i<nord; i++) {
#if defined(__powerpc__)
            ulong tmp = ldata[i];
            val[i] = (tmp >> 16) | (tmp << 16); // we need word-swap for F3RP61
#else
            val[i] = ldata[i];
#endif
        }

    } else if (conv == 'L') {
        for (int32_t i=0; i<nord; i++) {
            val[i] = (wdata[2*i+1]<<16) | wdata[2*i+0];
        }

    } else if (conv == 'B') {
        int32_t overflow = 0;
        for (int32_t i=0; i<nord; i++) {
            val[i] = bcd2ushort(wdata[i], &overflow);
            ret |= overflow;
        }

    } else if (conv == 'U') {
        for (int32_t i=0; i<nord; i++) {
            val[i] = (uint16_t)wdata[i];
        }

    } else { // conv == 'W'
        for (int32_t i=0; i<nord; i++) {
            val[i] = (int16_t)wdata[i];
        }
    }

    return ret;
}

// read data from val and fill to buf
// return: 0= success, -1= overflow/underflow in ushort2bcd
int32_t devF3RP61ulong2buf(uint32_t *val, void *buf, const int8_t conv, int32_t nord)
{
    int ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    } else if (conv == 'X') { // long word access for XP01/XP02 modules (might be supported in the future)
        ulong *ldata = buf;
        for (int32_t i=0; i<nord; i++) {
#if defined(__powerpc__)
            //ulong val = (ulong)val[i];
            ulong tmp = val[i];
            ldata[i] = (tmp >> 16) | (tmp << 16); // we need word-swap for F3RP61
#else
            ldata[i] = (uint32_t)val[i];
#endif
        }

    } else if (conv == 'L') {
        for (int32_t i=0; i<nord; i++) {
            int32_t lval = val[i];
            wdata[2*i + 0] = (uint16_t)(lval>> 0);
            wdata[2*i + 1] = (uint16_t)(lval>>16);
        }

    } else if (conv == 'B') {
        int32_t overflow = 0;
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = ushort2bcd(val[i], &overflow);
            ret |= overflow;
        }

    } else if (conv == 'U') {
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = (uint16_t)val[i];
        }

    } else {// conv == 'W'
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = (int16_t)val[i];
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////
//

// read data from buf and fill to val
// return: 0= success, -1= overflow/underflow in bcd2ushort
int32_t devF3RP61buf2short(void *buf, int16_t *val, const int8_t conv, int32_t nord)
{
    int32_t ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    //} else if (conv == 'X') {
    //} else if (conv == 'L') {
    } else if (conv == 'B') {
        int32_t overflow = 0;
        for (int32_t i=0; i<nord; i++) {
            val[i] = bcd2ushort(wdata[i], &overflow);
            ret |= overflow;
        }

    } else {// conv == 'U' || conv == 'W'
        for (int32_t i=0; i<nord; i++) {
            val[i] = (uint16_t)wdata[i];
        }
    }

    return ret;
}

// read data from val and fill to buf
// return: 0= success, -1= overflow/underflow in ushort2bcd
int32_t devF3RP61short2buf(int16_t *val, void *buf, const int8_t conv, int32_t nord)
{
    int ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    //} else if (conv == 'X') {
    //} else if (conv == 'L') {
    } else if (conv == 'B') {
        int32_t overflow = 0;
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = ushort2bcd(val[i], &overflow);
            ret |= overflow;
        }

    } else {// conv == 'U' || conv == 'W'
        for (int32_t i=0; i<nord; i++) {
            wdata[i] = (uint16_t)val[i];
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////
//

// read data from buf and fill to val
// return: 0= success, 2= no conversion
int32_t devF3RP61buf2double(void *buf, double *val, const int8_t conv, int32_t nord)
{
    int ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    } else if (conv == 'D') {
        for (uint32_t i=0; i<nord; i++) {
            double tmp;
            uint64_t w0 = wdata[4*i + 0];
            uint64_t w1 = wdata[4*i + 1];
            uint64_t w2 = wdata[4*i + 2];
            uint64_t w3 = wdata[4*i + 3];
            uint64_t lval = (w3<<48) | (w2<<32) | (w1<<16) | w0;
            memcpy(&tmp, &lval, sizeof(double));
            val[i] = tmp;
        }
        ret = 2;
    } else if (conv == 'F') {
        for (uint32_t i=0; i<nord; i++) {
            float tmp;
            uint32_t w0 = wdata[2*i + 0];
            uint32_t w1 = wdata[2*i + 1];
            uint32_t lval = (w1<<16) | w0;
            memcpy(&tmp, &lval, sizeof(float));
            val[i] = tmp;
        }
        ret = 2;
    } else if (conv == 'L') {
        for (uint32_t i=0; i<nord; i++) {
            uint32_t w0 = wdata[2*i + 0];
            uint32_t w1 = wdata[2*i + 1];
            int32_t lval = (w1<<16) | w0; // 'L' is signed
            val[i] = lval;
        }
    } else if (conv == 'U') {
        for (uint32_t i=0; i<nord; i++) {
            val[i] = (uint16_t)wdata[i];
        }
    } else {// conv == 'W'
        for (uint32_t i=0; i<nord; i++) {
            val[i] = (int16_t)wdata[i];
        }
    }

    return ret;
}

// read data from val and fill to buf
// return: 0= success
int32_t devF3RP61double2buf(double *val, void *buf, const int8_t conv, int32_t nord)
{
    int ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    } else if (conv == 'D') {
        for (uint32_t i=0; i<nord; i++) {
            const double tmp = val[i];
            int64_t lval;
            memcpy(&lval, &tmp, sizeof(double));
            wdata[4*i + 0] = (uint16_t)(lval>> 0);
            wdata[4*i + 1] = (uint16_t)(lval>>16);
            wdata[4*i + 2] = (uint16_t)(lval>>32);
            wdata[4*i + 3] = (uint16_t)(lval>>48);
        }
    } else if (conv == 'F') {
        for (uint32_t i=0; i<nord; i++) {
            float tmp = val[i];
            int32_t lval;
            memcpy(&lval, &tmp, sizeof(float));
            wdata[2*i + 0] = (uint16_t)(lval>> 0);
            wdata[2*i + 1] = (uint16_t)(lval>>16);
        }
    } else if (conv == 'L') {
        for (uint32_t i=0; i<nord; i++) {
            int32_t lval = val[i];
            wdata[2*i + 0] = (uint16_t)(lval>> 0);
            wdata[2*i + 1] = (uint16_t)(lval>>16);
        }
    } else if (conv == 'U') {
        for (uint32_t i=0; i<nord; i++) {
            wdata[i] = (uint16_t)val[i];
        }
    } else {// conv == 'W'
        for (uint32_t i=0; i<nord; i++) {
            wdata[i] = (int16_t)val[i];
        }
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////
//

// read data from buf and fill to val
// return: 0= success, 2= no conversion
int32_t devF3RP61buf2float(void *buf, float *val, const int8_t conv, int32_t nord)
{
    int ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    //} else if (conv == 'D') {
    } else if (conv == 'F') {
        for (uint32_t i=0; i<nord; i++) {
            float tmp;
            uint32_t w0 = wdata[2*i + 0];
            uint32_t w1 = wdata[2*i + 1];
            uint32_t lval = (w1<<16) | w0;
            memcpy(&tmp, &lval, sizeof(float));
            val[i] = tmp;
        }
        ret = 2;
    } else if (conv == 'L') {
        for (uint32_t i=0; i<nord; i++) {
            uint32_t w0 = wdata[2*i + 0];
            uint32_t w1 = wdata[2*i + 1];
            int32_t lval = (w1<<16) | w0; // 'L' is signed
            val[i] = lval;
        }
    } else if (conv == 'U') {
        for (uint32_t i=0; i<nord; i++) {
            val[i] = (uint16_t)wdata[i];
        }
    } else {// conv == 'W'
        for (uint32_t i=0; i<nord; i++) {
            val[i] = (int16_t)wdata[i];
        }
    }

    return ret;
}

// read data from val and fill to buf
// return: 0= success
int32_t devF3RP61float2buf(float *val, void *buf, const int8_t conv, int32_t nord)
{
    int ret = 0;
    uint16_t *wdata = buf;

    if (0) {
    //} else if (conv == 'D') {
    } else if (conv == 'F') {
        for (uint32_t i=0; i<nord; i++) {
            float tmp = val[i];
            int32_t lval;
            memcpy(&lval, &tmp, sizeof(float));
            wdata[2*i + 0] = (uint16_t)(lval>> 0);
            wdata[2*i + 1] = (uint16_t)(lval>>16);
        }
    } else if (conv == 'L') {
        for (uint32_t i=0; i<nord; i++) {
            int32_t lval = val[i];
            wdata[2*i + 0] = (uint16_t)(lval>> 0);
            wdata[2*i + 1] = (uint16_t)(lval>>16);
        }
    } else if (conv == 'U') {
        for (uint32_t i=0; i<nord; i++) {
            wdata[i] = (uint16_t)val[i];
        }
    } else {// conv == 'W'
        for (uint32_t i=0; i<nord; i++) {
            wdata[i] = (int16_t)val[i];
        }
    }

    return ret;
}

// end
