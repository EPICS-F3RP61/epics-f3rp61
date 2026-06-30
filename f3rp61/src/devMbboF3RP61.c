/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbboF3RP61.c - Device Support Routines for F3RP61 multi-bit binary
* Output
*
*      Author: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/

//
#include <mbboRecord.h>

//
#include <drvF3RP61.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devMbboF3RP61
static long init_record();
static long write_mbbo();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_mbbo;
    DEVSUPFUN  special_linconv;
} devMbboF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_mbbo,
    NULL
};

epicsExportAddress(dset, devMbboF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbboRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devMbboF3RP61 (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    prec->dpvt = dpvt;
    const uint32_t nelm = 1;

    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, DBF_LONG, nelm);
    if (ret < 0) {
        //errlogPrintf("devMbboF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Set MASK, NOBT, and MASK
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

// write_mbbo() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_mbbo(mbboRecord *prec)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    int32_t      nord = dpvt->nord;

    // Compose data to write
    int ret = devF3RP61uint2buf(&prec->rval, dpvt->buf, dpvt->conv, nord);
    if (!ret) {
        // overflow happend in int2bcd
        recGblSetSevr(prec, HW_LIMIT_ALARM, INVALID_ALARM);
    }

    // Issue API function
    nord = f3rp61Write((dbCommon*)prec, nord); // nord must be identical to dpvt->nord, if no error
    if (nord < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    //
    return 0;
}
