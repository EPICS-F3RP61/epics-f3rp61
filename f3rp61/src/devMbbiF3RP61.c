/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiF3RP61.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
*
*      Author: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/

//
#include <mbbiRecord.h>

//
#include <drvF3RP61.h>

// Create the dset for devMbbiF3RP61
static long init_record();
static long read_mbbi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_mbbi;
    DEVSUPFUN  special_linconv;
} devMbbiF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_mbbi,
    NULL
};

epicsExportAddress(dset, devMbbiF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbbiRecord *precord)
{
    //
    struct link *plink = &precord->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devMbbiF3RP61 (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");

    //
    const int ret = f3rp61ParseLink(plink, dpvt, (dbCommon *)precord, "devMbbiF3RP61");
    if (ret < 0) {
        //errlogPrintf("devMbbiF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy conv for Word access
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
        errlogPrintf("devMbbiF3RP61: %s : unsupported conversion specifier \'%c\'\n", precord->name, conv);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'E' || device == 'L' || // Shared relays and Link relays
               device == 'r') {                  // Shared memory
    } else if (device == 'X' || device == 'Y' || // Input and output relays on I/O modules
               device == 'M') {                  // Mode registers on I/O modules
    } else if (device == 'A') {                  // I/O registers on special modules
    } else {
        errlogPrintf("devMbbiF3RP61: %s : unsupported device \'%c\'\n", precord->name, device);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// read_mbbi() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_mbbi(mbbiRecord *precord)
{
    F3RP61_DPVT  *dpvt = precord->dpvt;
    const int8_t  device = dpvt->device;
    const int8_t  conv   = dpvt->conv;
    const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count; // should be 1

    // Buffer for data read
    uint16_t wdata[8] = {0}; // 2 would be enough, but readM3IoModeRegister requires 8

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (readM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : readM3ComRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (readM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : readM3LinkRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'E') { // Shared relay
        const int32_t addr = dpvt->addr;
        if (readM3ComRelay(addr, count, wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : readM3ComRelay failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'L') { // Link relay
        const int32_t addr = dpvt->addr;
        if (readM3LinkRelay(addr, count, wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : readM3LinkRelay failed [%d]\n", precord->name, errno);
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
            errlogPrintf("devMbbiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (readM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : readM3CpuMemory failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else if (device == 'X') { // Input relays on I/O modules
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_INRELAY, &drly) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
        wdata[0] = drly.u.inrly[0].data;
        if (conv == 'L') {
            wdata[1] = drly.u.inrly[1].data;
        }

    } else if (device == 'Y') { // Output relays on I/O modules
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_OUTRELAY, &drly) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#if defined(__powerpc__)
        wdata[0] = drly.u.inrly[0].data;
        if (conv == 'L') {
            wdata[1] = drly.u.inrly[1].data;
        }
#else
        wdata[0] = drly.u.outrly[0].data;
        if (conv == 'L') {
            wdata[1] = drly.u.outrly[1].data;
        }
#endif

    } else if (device == 'M') { // Mode registers on I/O modules
#if defined(__powerpc__)
        // On F3RP61 start and count are fixed to 1 and 3 in ioctl() request,
        // and only the 1st element is valid in the data read out.
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = 1,
            .count  = 3,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_MODE, &drly) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
        wdata[0] = drly.u.wdata[0];
#else
        const int32_t unit = dpvt->unit;
        const int32_t slot = dpvt->slot;
        const int32_t addr = dpvt->addr;
        if (readM3IoModeRegister(unit, slot, addr, count, wdata) < 0) {
            errlogPrintf("devMbbiiF3RP61: %s : readM3IoModeRegister failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else {//(device == 'A') // I/O registers on special modules
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        drly.u.pwdata = wdata;
        if (ioctl(f3rp61_fd, M3IO_READ_REG, &drly) < 0) {
            errlogPrintf("devMbbiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
    }

    //
    precord->udf = FALSE;

    // fill VAL field
    if (conv == 'L') {
        precord->rval = (wdata[1]<<16) | wdata[0];
    } else if (conv == 'U') {
        precord->rval = (uint16_t)wdata[0];
    } else {
        precord->rval = (int16_t)wdata[0];
    }

    //
    return 0;
}
