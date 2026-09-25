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
#include <drvF3RP61_private.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kWord;

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
static long init_record(stringoutRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (prec->out.type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devSoF3RP61 (init_record) Illegal OUT field");
        return S_db_badField;
    }

    //
    const uint32_t nelm = 20;
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, DBF_STRING, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return 0;
    }

    // Check device validity
    F3RP61_DPVT *dpvt = prec->dpvt;
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'A') {                  // I/O registers on special modules
        dpvt->count = 20;
    } else {
        errlogPrintf("devSoF3RP61: %s : unsupported device \'%c\'\n", prec->name, device);
    }

    //
    return 0;
}

// write_so() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_so(stringoutRecord *prec)
{
    F3RP61_DPVT   *dpvt  = prec->dpvt;
    if (!dpvt) { // something was wrong in OUT field and init_record() failed
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    //
    const int32_t count = dpvt->count;

    // Compose data to write
    void *bdata = dpvt->buf;
    strncpy(bdata, prec->val, count*sizeof(int16_t));

    // Issue API function
    M3IO_ACCESS_REG drly = {
        .unitno   = dpvt->unit,
        .slotno   = dpvt->slot,
        .start    = dpvt->addr,
        .count    = count,
        .u.pbdata = bdata,
    };

    if (ioctl(f3rp61_fd, M3IO_WRITE_REG, drly) < 0) {
        errlogPrintf("devSoF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    return 0;
}
