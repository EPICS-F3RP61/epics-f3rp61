/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAaiF3RP61.c - Device Support Routines for F3RP61 Array Analog Input
*
*      Author: Shuei YAMADA
*      Date: 2026-06-02
*/

//
#include <aaiRecord.h>

//
#include <drvF3RP61.h>

//
static const F3RP61_RW rw = kRead;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devAaiF3RP61
static long init_record();
static long read_aai();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_aai;
    DEVSUPFUN  special_linconv;
} devAaiF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_aai,
    NULL
};

epicsExportAddress(dset, devAaiF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aaiRecord *prec)
{
    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAaiF3RP61 (init_record) Illegal INP field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    const uint32_t nelm = prec->nelm;
    const dbfType  ftvl = prec->ftvl;
    const char *ftvlstr = (pamapdbfType[ftvl].strvalue) + 4;

    //
    const int ret = f3rp61ParseLink(plink, dpvt, rw, type, (dbCommon *)prec, dbValueSize(prec->ftvl), nelm);
    if (ret < 0) {
        //errlogPrintf("devAaiF3RP61: %s : syntax error in INP field\n", prec->name);
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

// read_aai() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_aai(aaiRecord *prec)
{
    //
    const uint32_t nelm = prec->nelm;
    const dbfType  ftvl = prec->ftvl;

    //
    F3RP61_DPVT   *dpvt   = prec->dpvt;
    const int8_t   device = dpvt->device;
    const int8_t   conv   = dpvt->conv;
    const int32_t  cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    int32_t        count  = dpvt->count * nelm;

    // debug
    //errlogPrintf("devAaiF3RP61: %s : count=%d (%d*%d) SCAN%s\n", prec->name, count, dpvt->count, nelm, (prec->scan)==SCAN_IO_EVENT?" by I/O intr":"");

    // Buffers for data read
    uint16_t *wdata = dpvt->buf;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (readM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAaiF3RP61: %s : readM3ComRegister failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (readM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAaiF3RP61: %s : readM3LinkRegister failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (readM3ComRelay(addr, count, wdata) < 0) {
            errlogPrintf("devAaiF3RP61: %s : readM3ComRelay failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        const int32_t addr = dpvt->addr;
        if (readM3LinkRelay(addr, count, wdata) < 0) {
            errlogPrintf("devAaiF3RP61: %s : readM3LinkRelay failed [%d]\n", prec->name, errno);
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
        if (ioctl(f3rp61_fd, M3IO_READ_COM, &acom) < 0) {
            errlogPrintf("devAaiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (readM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devAaiF3RP61: %s : readM3CpuMemory failed [%d]\n", prec->name, errno);
            return -1;
        }
#endif

    } else if (device == 'X') { // Input relays on I/O modules
        if (count>4) { // The maximum number of blocks is 4
            count = 4;
        }
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_INRELAY, &drly) < 0) {
            errlogPrintf("devAaiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
        for (int32_t i=0; i<count; i++) { // =1*nelm, =2*nelm(&F, &L), =4*nelm(&D)
            wdata[i] = drly.u.inrly[i].data;
        }

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
        if (ioctl(f3rp61_fd, M3IO_READ_OUTRELAY, &drly) < 0) {
            errlogPrintf("devAaiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
#if defined(__powerpc__)
        for (int32_t i=0; i<count; i++) { // =1*nelm, =2*nelm(&F, &L), =4*nelm(&D)
            wdata[i] = drly.u.inrly[i].data;
        }
#else
        for (int32_t i=0; i<count; i++) { // =1*nelm, =2*nelm(&F, &L), =4*nelm(&D)
            wdata[i] = drly.u.outrly[i].data;
        }
#endif

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
            //.start  = 1,
            //.count  = 3,
            .start  = dpvt->addr,
            .count  = count,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_MODE, &drly) < 0) {
            errlogPrintf("devAaiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
        for (int32_t i=0; i<count; i++) {
            wdata[i] = drly.u.wdata[i];
        }
#else
        if (count>8) { // The maximum number of blocks is 8
            count = 8;
        }
        const int32_t unit = dpvt->unit;
        const int32_t slot = dpvt->slot;
        const int32_t addr = dpvt->addr;
        if (readM3IoModeRegister(unit, slot, addr, count, wdata) < 0) {
            errlogPrintf("devAaiF3RP61: %s : readM3IoModeRegister failed [%d]\n", prec->name, errno);
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
        if (ioctl(f3rp61_fd, M3IO_READ_REG, &drly) < 0) {
            errlogPrintf("devAaiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
    }

    //debug
    //errlogPrintf("devAaiF3RP61: %s : I/O API finished\n", prec->name);

    //
    prec->udf = FALSE;

    // fill VAL field
    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        double *bptr = prec->bptr;
        if (0) {
        } else if (conv == 'D') {
            for (uint32_t i=0; i<nelm; i++) {
                double val;
                uint64_t w0 = wdata[4*i + 0];
                uint64_t w1 = wdata[4*i + 1];
                uint64_t w2 = wdata[4*i + 2];
                uint64_t w3 = wdata[4*i + 3];
                uint64_t lval = (w3<<48) | (w2<<32) | (w1<<16) | w0;
                memcpy(&val, &lval, sizeof(double));
                bptr[i] = val;
            }
        } else if (conv == 'F') {
            for (uint32_t i=0; i<nelm; i++) {
                float val;
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                uint32_t lval = (w1<<16) | w0;
                memcpy(&val, &lval, sizeof(float));
                bptr[i] = val;
            }
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nelm; i++) {
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                int32_t lval = (w1<<16) | w0; // 'L' is signed
                bptr[i] = lval;
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nelm; i++) {
                bptr[i] = (uint16_t)wdata[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                bptr[i] = (int16_t)wdata[i];
            }
        }
    } else if (ftvl == DBF_FLOAT) {
        float *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        } else if (conv == 'F') {
            for (uint32_t i=0; i<nelm; i++) {
                float val;
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                uint32_t lval = (w1<<16) | w0;
                memcpy(&val, &lval, sizeof(float));
                bptr[i] = val;
            }
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nelm; i++) {
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                int32_t lval = (w1<<16) | w0; // 'L' is signed
                bptr[i] = lval;
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nelm; i++) {
                bptr[i] = (uint16_t)wdata[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                bptr[i] = (int16_t)wdata[i];
            }
        }
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        uint32_t *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        //} else if (conv == 'F') {
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nelm; i++) {
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                uint32_t lval = (w1<<16) | w0;
                bptr[i] = lval;
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nelm; i++) {
                bptr[i] = (uint16_t)wdata[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                bptr[i] = (int16_t)wdata[i];
            }
        }
    } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
        uint16_t *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        //} else if (conv == 'F') {
        //} else if (conv == 'L') {
        } else {// conv == 'U' || conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                bptr[i] = wdata[i];
            }
        }
    }

    //
    prec->nord = count / dpvt->count;

    //
    return 0;
}
