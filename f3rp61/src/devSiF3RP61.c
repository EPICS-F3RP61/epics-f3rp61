/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devSiF3RP61.c - Device Support Routines for F3RP61 String Input
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <stringinRecord.h>

//
#include <drvF3RP61.h>

//
static const F3RP61_RW rw = kRead;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devSiF3RP61
static long init_record();
static long read_si();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_si;
    DEVSUPFUN  special_linconv;
} devSiF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_si,
    NULL
};

epicsExportAddress(dset, devSiF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(stringinRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devSiF3RP61 (init_record) Illegal INP field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    const uint32_t nelm = 20;
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, nelm);
    if (ret < 0) {
        //errlogPrintf("devSiF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
    } else {
        errlogPrintf("devSiF3RP61: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'A') {                  // I/O registers on special modules
        dpvt->count  = 20;
    } else {
        errlogPrintf("devSiF3RP61: %s : unsupported device \'%c\'\n", prec->name, device);
        prec->pact = 1;
        return -1;
    }

    //
    return 0;
}

// read_si() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_si(stringinRecord *prec)
{
    F3RP61_DPVT  *dpvt = prec->dpvt;
    const int32_t count  = dpvt->count;

    // Buffer for data read
    void *bdata = dpvt->buf;

    // Issue API function
    M3IO_ACCESS_REG drly = {
        .unitno   = dpvt->unit,
        .slotno   = dpvt->slot,
        .start    = dpvt->addr,
        .count    = count,
        .u.pbdata = bdata,
    };
    if (ioctl(f3rp61_fd, M3IO_READ_REG, drly) < 0) {
        errlogPrintf("devSiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    // fill VAL field
    strncpy(prec->val, bdata, count*sizeof(int16_t));

    //
    return 0;
}
