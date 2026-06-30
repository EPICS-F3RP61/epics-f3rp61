/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAoF3RP61.c - Device Support Routines for F3RP61 Analog Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <aoRecord.h>

//
#include <drvF3RP61.h>

//
#include <math.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devAoF3RP61
static long init_record();
static long write_ao();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_ao;
    DEVSUPFUN  special_linconv;
} devAoF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_ao,
    NULL
};

epicsExportAddress(dset, devAoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aoRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAoF3RP61 (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    prec->dpvt = dpvt;
    const uint32_t nelm = 1;

    //
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, DBF_DOUBLE, nelm);
    if (ret < 0) {
        //errlogPrintf("devAoF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    //
    return 0;
}

// write_ao() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_ao(aoRecord *prec)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    int32_t      nord = dpvt->nord;

    // Compose data to write
    devF3RP61double2buf(&prec->val, dpvt->buf, dpvt->conv, nord);
    // todo : consider ASLO and AOFF field

    // Issue API function
    nord = f3rp61Write((dbCommon*)prec, nord); // nord must be identical to dpvt->nord, if no error
    if (nord < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = isnan(prec->val);

    //
    return 0;
}
