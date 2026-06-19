/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBoF3RP61.c - Device Support Routines for F3RP61 Binary Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <boRecord.h>

//
#include <drvF3RP61.h>

// Create the dset for devBoF3RP61
static long init_record();
static long write_bo();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_bo;
} devBoF3RP61 = {
    5,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_bo
};

epicsExportAddress(dset, devBoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(boRecord *precord)
{
    //
    struct link *plink = &precord->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devBoF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");

    //
    const int ret = f3rp61ParseLink(plink, dpvt, (dbCommon *)precord, "devBoF3RP61");
    if (ret < 0) {
        //errlogPrintf("devBoF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
    } else {
        errlogPrintf("devBiF3RP61: %s : unsupported conversion specifier \'%c\'\n", precord->name, conv);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'E' || device == 'L') { // Shared relays and Link relays
    } else if (device == 'Y') {                  // Output relays on I/O modules
    } else {
        errlogPrintf("devBoF3RP61: %s : unsupported device \'%c\'\n", precord->name, device);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// write_bo() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_bo(boRecord *precord)
{
    // debug
    //if (precord->scan == SCAN_IO_EVENT) {
    //    errlogPrintf("devBoF3RP61: %s : SCAN by I/O intr\n", precord->name);
    //}

    F3RP61_DPVT  *dpvt = precord->dpvt;
    const int8_t  device = dpvt->device;
    //const int8_t  conv   = dpvt->conv;
    //const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    //const int32_t count  = dpvt->count;

    // Compose data to write
    uint8_t cdata = precord->rval;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (writeM3ComRelayB(addr, 1, &cdata) < 0) {
            errlogPrintf("devBoF3RP61: %s : writeM3ComRelayB failed [%d]n", precord->name, errno);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRelayB(addr, 1, &cdata) < 0) {
            errlogPrintf("devBoF3RP61: %s : writeM3LinkRelayB failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'Y') { // Relays on I/O modules
        M3IO_ACCESS_RELAY_POINT outrlyp = {
            .unitno   = dpvt->unit,
            .slotno   = dpvt->slot,
            .position = dpvt->addr,
            .data     = cdata,
        };
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY_POINT, &outrlyp) < 0) {
            errlogPrintf("devBoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
    } else {
        //
    }

    //
    precord->udf = FALSE;

    //
    return 0;
}
