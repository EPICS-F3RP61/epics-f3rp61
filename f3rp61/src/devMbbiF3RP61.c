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

//
static const F3RP61_RW rw = kRead;
static const F3RP61_ACCESS_TYPE type = kWord;

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
static long init_record(mbbiRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devMbbiF3RP61 (init_record) Illegal INP field");
        prec->pact = 1;
        return S_db_badField;
    }

    //
    const uint32_t nelm = 1;
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, DBF_LONG, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 0;
    }

    // Set MASK, NOBT, and MASK
    F3RP61_DPVT *dpvt = prec->dpvt;
    const int8_t conv = dpvt->conv;
    if (conv == 'L' || conv == 'X') { // 'X' conversion may not make sense for mbbiDirect
        prec->nobt = 32;
        prec->mask = 0xffffffff;
        prec->shft = 0;
    } else {
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    }

    //
    return 0;
}

// read_mbbi() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_mbbi(mbbiRecord *prec)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in OUT field and init_record() failed
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    //
    int32_t nord = dpvt->nord;

    // Issue API function
    nord = f3rp61Read((dbCommon*)prec, nord); // nord must be identical to dpvt->nord, if no error
    if (nord < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    // fill VAL field
    int ret = devF3RP61buf2uint(dpvt->buf, &prec->rval, dpvt->conv, nord);
    if (ret < 0) {
        // overflow happend in bcd2int
        recGblSetSevr(prec, HIGH_ALARM, INVALID_ALARM);
    }

    //
    return 0;
}
