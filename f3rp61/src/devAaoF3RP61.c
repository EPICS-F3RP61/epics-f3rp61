/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAaoF3RP61.c - Device Support Routines for F3RP61 Array Analog Output
*
*      Author: Shuei YAMADA
*      Date: 2026-06-02
*/

//
#include <aaoRecord.h>

//
#include <drvF3RP61.h>

//
#include <math.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devAaoF3RP61
static long init_record();
static long write_aao();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_aaao;
    DEVSUPFUN  special_linconv;
} devAaoF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_aao,
    NULL
};

epicsExportAddress(dset, devAaoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aaoRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAaoF3RP61 (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    const uint32_t nelm = prec->nelm;
    const uint16_t ftvl = prec->ftvl;
    const char *ftvlstr = (pamapdbfType[ftvl].strvalue) + 4;

    //
    const int ret = f3rp61ParseLink(plink, dpvt, rw, type, (dbCommon *)prec, dbValueSize(prec->ftvl), nelm, "devAaoF3RP61");
    if (ret < 0) {
        //errlogPrintf("devAaoF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        } else if (conv == 'L') { // Long word
        } else if (conv == 'F') { // Single precision floating point
        } else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else if (ftvl == DBF_FLOAT) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        } else if (conv == 'L') { // Long word
        } else if (conv == 'F') { // Single precision floating point
        //} else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        } else if (conv == 'L') { // Long word
        //} else if (conv == 'F') { // Single precision floating point
        //} else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else if (ftvl == DBF_SHORT || ftvl == DBF_USHORT) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        //} else if (conv == 'L') { // Long word
        //} else if (conv == 'F') { // Single precision floating point
        //} else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else {
        errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    //
    prec->dpvt = dpvt;

    return 0;
}

// write_aao() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_aao(aaoRecord *prec)
{
    // debug
    //if (prec->scan == SCAN_IO_EVENT) {
    //    errlogPrintf("devAaoF3RP61: %s : SCAN by I/O intr\n", prec->name);
    //}

    //
    const uint32_t nelm = prec->nelm;
    const uint16_t ftvl = prec->ftvl;

    //
    F3RP61_DPVT   *dpvt = prec->dpvt;
    const int8_t   device = dpvt->device;
    const int8_t   conv   = dpvt->conv;
    const int32_t  cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    int32_t        count  = dpvt->count * nelm;

    // debug
    //errlogPrintf("devAaiF3RP61: %s : count=%d (%d*%d) SCAN%s\n", prec->name, count, dpvt->count, nelm, (prec->scan)==SCAN_IO_EVENT?" by I/O intr":"");

    // Compose data to write
    uint16_t *wdata  = dpvt->buf;
    uint16_t mask[4] = {0xffff, 0xffff, 0xffff, 0xffff};

    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        double *bptr = prec->bptr;
        if (0) {
        } else if (conv == 'D') {
            for (uint32_t i=0; i<nelm; i++) {
                const double val = bptr[i];
                int64_t lval;
                memcpy(&lval, &val, sizeof(double));
                wdata[4*i + 0] = (uint16_t)(lval>> 0);
                wdata[4*i + 1] = (uint16_t)(lval>>16);
                wdata[4*i + 2] = (uint16_t)(lval>>32);
                wdata[4*i + 3] = (uint16_t)(lval>>48);
            }
        } else if (conv == 'F') {
            for (uint32_t i=0; i<nelm; i++) {
                float val = bptr[i];
                int32_t lval;
                memcpy(&lval, &val, sizeof(float));
                wdata[2*i + 0] = (uint16_t)(lval>> 0);
                wdata[2*i + 1] = (uint16_t)(lval>>16);
            }
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nelm; i++) {
                int32_t lval = bptr[i];
                wdata[2*i + 0] = (uint16_t)(lval>> 0);
                wdata[2*i + 1] = (uint16_t)(lval>>16);
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (uint16_t)bptr[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (int16_t)bptr[i];
            }
        }
    } else if (ftvl == DBF_FLOAT) {
        const float *bptr = prec->bptr;
        if (0) {
        } else if (conv == 'F') {
            for (uint32_t i=0; i<nelm; i++) {
                float val = bptr[i];
                int32_t lval;
                memcpy(&lval, &val, sizeof(float));
                wdata[2*i + 0] = (uint16_t)(lval>> 0);
                wdata[2*i + 1] = (uint16_t)(lval>>16);
            }
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nelm; i++) {
                int32_t lval = bptr[i];
                wdata[2*i + 0] = (uint16_t)(lval>> 0);
                wdata[2*i + 1] = (uint16_t)(lval>>16);
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (uint16_t)bptr[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (int16_t)bptr[i];
            }
        }
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        const int32_t *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        //} else if (conv == 'F') {
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nelm; i++) {
                int32_t lval = bptr[i];
                wdata[2*i + 0] = (uint16_t)(lval>> 0);
                wdata[2*i + 1] = (uint16_t)(lval>>16);
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (uint16_t)bptr[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (int16_t)bptr[i];
            }
        }
    } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
        const uint16_t *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        //} else if (conv == 'F') {
        //} else if (conv == 'L') {
        } else {// conv == 'U' || conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (uint16_t)bptr[i];
            }
        }
    }

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (writeM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAaoF3RP61: %s : writeM3ComRegister failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAaoF3RP61: %s : writeM3LinkRegister failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (writeM3ComRelay(addr, count, wdata) < 0) {
            errlogPrintf("devAaoF3RP61: %s : writeM3ComRelay failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRelay(addr, count, wdata) < 0) {
            errlogPrintf("devAaoF3RP61: %s : writeM3LinkRelay failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'r') { // Shared memory
#if defined(__powerpc__)
        M3IO_ACCESS_COM acom = {
            .cpuno = cpuno,
            .start = dpvt->addr,
            .count = count,
            .pdata = wdata,
        };
        if (ioctl(f3rp61_fd, M3IO_WRITE_COM, &acom) < 0) {
            errlogPrintf("devAaoF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (writeM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devAaoF3RP61: %s : writeM3CpuMemory failed [%d]\n", prec->name, errno);
            return -1;
        }
#endif

    } else if (device == 'Y') { // Output relays on I/O modules
        if (count>4) { // The maximum number of blocks is 4
            count = 4;
        }
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        for (int32_t i=0; i<count; i++) { // =1, =2(&F, &L), =4(&D)
            drly.u.outrly[i].data = wdata[i];
            drly.u.outrly[i].mask = mask[i];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY, &drly) < 0) {
            errlogPrintf("devAaoF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'M') { // Mode registers on I/O modules
#if defined(__powerpc__)
        // The F3RP61 Linux BSP Reference manual states that start
        // address is fixed at 1 and the count at 3. However it
        // appears that start address of 2, 3, or 4 are also
        // accepted. Similary, the count can be 1, 2, or 4. Be aware
        // that writing to the address 4 does not make sense, and may
        // cause problems.
        if (count>4) { // The maximum number of blocks is 3, but 4 seems OK
            count = 4; // 3
        }
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
//            .start  = 1,
//            .count  = 3,
            .start  = dpvt->addr,
            .count  = count,
        };
        for (int32_t i=0; i<count; i++) {
            drly.u.wdata[i] = wdata[i];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_MODE, &drly) < 0) {
            errlogPrintf("devAaoF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
#else
        if (count>8) { // The maximum number of blocks is 8
            count = 8;
        }
        const int32_t unit = dpvt->unit;
        const int32_t slot = dpvt->slot;
        const int32_t addr = dpvt->addr;
        if (writeM3IoModeRegister(unit, slot, addr, count, wdata) < 0) {
            errlogPrintf("devAaoF3RP61: %s : writeM3IoModeRegister failed [%d]\n", prec->name, errno);
            return -1;
        }
#endif

    } else {//(device == 'A')   // I/O registers on special modules
        M3IO_ACCESS_REG drly = {
            .unitno   = dpvt->unit,
            .slotno   = dpvt->slot,
            .start    = dpvt->addr,
            .count    = count,
            .u.pwdata = wdata,
        };
        if (ioctl(f3rp61_fd, M3IO_WRITE_REG, &drly) < 0) {
            errlogPrintf("devAaoF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
    }

    //
    prec->udf = FALSE;

    //
    prec->nord = count / dpvt->count;

    //
    return 0;
}
