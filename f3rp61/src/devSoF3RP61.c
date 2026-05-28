/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devSoF3RP61.c - Device Support Routines for F3RP61 String Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <stringoutRecord.h>

//
#include <drvF3RP61.h>

// Create the dset for devSoF3RP61
static long init_record();
static long write_so();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_so;
    DEVSUPFUN  special_linconv;
} devSoF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_so,
    NULL
};

epicsExportAddress(dset, devSoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(stringoutRecord *precord)
{
    //
    struct link *plink = &precord->out;

    // Link type must be INST_IO
    if (precord->out.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devSoF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");

    //
    const int ret = f3rp61ParseLink(plink, dpvt, (dbCommon *)precord, "devSoF3RP61");
    if (ret < 0) {
        //errlogPrintf("devSoF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion Option
    const int8_t option = dpvt->option;
    if (option == 'W') {        // Dummy option for Word access
    } else {                    // Option not recognized
        errlogPrintf("devSoF3RP61: %s : unsupported option \'%c\'\n", precord->name, option);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'A') {                  // I/O registers on special modules
        dpvt->count = 20;
    } else {
        errlogPrintf("devSoF3RP61: %s : unsupported device \'%c\'\n", precord->name, device);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// write_so() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_so(stringoutRecord *precord)
{
    F3RP61_DPVT  *dpvt = precord->dpvt;
    //const int8_t  device = dpvt->device;
    //const int8_t  option = dpvt->option;
    //const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count;

    // Compose data to write
    char bdata[40];
    strncpy(bdata, precord->val, 40);

    // Issue API function
    M3IO_ACCESS_REG drly = {
        .unitno = getunit(dpvt->addr),
        .slotno = getslot(dpvt->addr),
        .start  = getaddr(dpvt->addr),
        .count  = count,
    };
    drly.u.pbdata = (unsigned char *)bdata;

    if (ioctl(f3rp61_fd, M3IO_WRITE_REG, drly) < 0) {
        errlogPrintf("devSoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
        return -1;
    }

    //
    precord->udf = FALSE;

    return 0;
}
