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
static long init_record(mbboDirectRecord *precord)
{
    //
    struct link *plink = &precord->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devMbboDirectF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");

    //
    const int ret = f3rp61ParseLink(plink, dpvt, (dbCommon *)precord, "devLoF3RP61");
    if (ret < 0) {
        //errlogPrintf("devLoF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
        precord->nobt = 16;
        precord->mask = 0xffff;
        precord->shft = 0;
    } else if (conv == 'U') { // Unsigned integer
        precord->nobt = 16;
        precord->mask = 0xffff;
        precord->shft = 0;
    } else if (conv == 'L') { // Long word
        precord->nobt = 32;
        precord->mask = 0xffffffff;
        precord->shft = 0;
    } else {
        errlogPrintf("devMbboDirectF3RP61: %s : unsupported conversion specifier \'%c\'\n", precord->name, conv);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'E' || device == 'L' || // Shared relay and Link relay
               device == 'r') {                  // Shared memory
    } else if (device == 'Y' ||                  // Output relays on I/O modules
               device == 'M') {                  // Mode registers on I/O modules
    } else if (device == 'A') {                  // I/O registers on special modules
    } else {
        errlogPrintf("devMbboDirectF3RP61: %s : unsupported device \'%c\'\n", precord->name, device);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 2; // no conversion
}

// write_mbboDirect() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_mbboDirect(mbboDirectRecord *precord)
{
    F3RP61_DPVT  *dpvt = precord->dpvt;
    const int8_t  device = dpvt->device;
    const int8_t  conv   = dpvt->conv;
    const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count;

    // Compose data to write
    uint16_t wdata[8] = {0}; // 2 would be enough, but readM3IoModeRegister requires 8
    uint16_t mask[2]  = {0xffff, 0xffff};

    wdata[0] = (uint16_t)precord->rval;
    if (conv == 'L') {
        wdata[1] = (uint16_t)(precord->rval>>16);
    }

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (writeM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3ComRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3LinkRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (writeM3ComRelay(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3ComRelay failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRelay(addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3LinkRelay failed [%d]\n", precord->name, errno);
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
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (writeM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3CpuMemory failed [%d]\n", precord->name, errno);
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
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'M') { // Mode registers on I/O modules
#if defined(__powerpc__)
        // On F3RP61 start and count are fixed to 1 and 3 in ioctl() request,
        // and only the 1st element is valid in the data written.
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = 1,
            .count  = 3,
        };
        drly.u.wdata[0] = wdata[0];
        if (ioctl(f3rp61_fd, M3IO_WRITE_MODE, &drly) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t unit = dpvt->unit;
        const int32_t slot = dpvt->slot;
        const int32_t addr = dpvt->addr;
        if (writeM3IoModeRegister(unit, slot, addr, count, wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : writeM3IoModeRegisterL failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else {//(device == 'A')   // I/O registers on special modules
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        drly.u.pwdata = wdata;
        if (ioctl(f3rp61_fd, M3IO_WRITE_REG, &drly) < 0) {
            errlogPrintf("devMbboDirectF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
    }

    //
    precord->udf = FALSE;

    //
    return 0;
}
