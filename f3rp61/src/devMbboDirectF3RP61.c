/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbboDirectF3RP61.c - Device Support Routines for F3RP61 multi-bit
* Binary Direct Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <mbboDirectRecord.h>

//
#include <drvF3RP61.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devMbboDirectF3RP61
static long init_record();
static long write_mbboDirect();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_mbboDirect;
    DEVSUPFUN  special_linconv;
} devMbboDirectF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_mbboDirect,
    NULL
};

epicsExportAddress(dset, devMbboDirectF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbboDirectRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devMbboDirectF3RP61 (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    const uint32_t nelm = 1;
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, nelm);
    if (ret < 0) {
        //errlogPrintf("devLoF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    } else if (conv == 'U') { // Unsigned integer
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    } else if (conv == 'L') { // Long word
        prec->nobt = 32;
        prec->mask = 0xffffffff;
        prec->shft = 0;
    } else {
        errlogPrintf("devMbboDirectF3RP61: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    //
    return 2; // no conversion
}

// write_mbboDirect() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_mbboDirect(mbboDirectRecord *prec)
{
    F3RP61_DPVT  *dpvt = prec->dpvt;
    const int8_t  device = dpvt->device;
    const int8_t  conv   = dpvt->conv;
    const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count;

    // Compose data to write
    uint16_t *wdata   = dpvt->buf;
    uint16_t  mask[2] = {0xffff, 0xffff};

    wdata[0] = (uint16_t)prec->rval;
    if (conv == 'L') {
        wdata[1] = (uint16_t)(prec->rval>>16);
    }

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (writeM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3ComRegister failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3LinkRegister failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (writeM3ComRelay(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3ComRelay failed [%d]\n", prec->name, errno);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRelay(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3LinkRelay failed [%d]\n", prec->name, errno);
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
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (writeM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3CpuMemory failed [%d]\n", prec->name, errno);
            return -1;
        }
#endif

    } else if (device == 'Y') { // Output relays on I/O modules
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        drly.u.outrly[0].data = wdata[0];
        drly.u.outrly[0].mask = mask[0];
        if (conv == 'L') {
            drly.u.outrly[1].data = wdata[1];
            drly.u.outrly[1].mask = mask[1];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY, &drly) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
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
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            //.start  = 1,
            //.count  = 3,
            .start  = dpvt->addr,
            .count  = count,
        };
        drly.u.wdata[0] = wdata[0];
        if (conv == 'L') {
            drly.u.wdata[1] = wdata[1];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_MODE, &drly) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
#else
        const int32_t unit = dpvt->unit;
        const int32_t slot = dpvt->slot;
        const int32_t addr = dpvt->addr;
        if (writeM3IoModeRegister(unit, slot, addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3IoModeRegisterL failed [%d]\n", prec->name, errno);
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
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            return -1;
        }
    }

    //
    prec->udf = FALSE;

    //
    return 0;
}
