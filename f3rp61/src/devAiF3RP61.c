/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAiF3RP61.c - Device Support Routines for F3RP61 Analog Input
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <aiRecord.h>

//
#include <drvF3RP61.h>

//
#include <math.h>

//
static const F3RP61_RW rw = kRead;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devAiF3RP61
static long init_record();
static long read_ai();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_ai;
    DEVSUPFUN  special_linconv;
} devAiF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_ai,
    NULL
};

epicsExportAddress(dset, devAiF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aiRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAiF3RP61 (init_record) Illegal INP field");
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
        //errlogPrintf("devAiF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
    } else if (conv == 'U') { // Unsigned integer
    } else if (conv == 'L') { // Long word
    } else if (conv == 'F') { // Single precision floating point
    } else if (conv == 'D') { // Double precision floating point
    } else {
        errlogPrintf("devAiF3RP61: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    //
    return 0;
}

// read_ai() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_ai(aiRecord *prec)
{
    // Issue API function
    const int32_t nord = f3rp61Read((dbCommon*)prec, 1);
    if (nord < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    // fill VAL field
    F3RP61_DPVT  *dpvt  = prec->dpvt;
    uint16_t     *wdata = dpvt->buf;
    const int8_t  conv  = dpvt->conv;

    if (conv == 'D') {
        double val;
        uint64_t w0 = wdata[0];
        uint64_t w1 = wdata[1];
        uint64_t w2 = wdata[2];
        uint64_t w3 = wdata[3];
        uint64_t lval = (w3<<48) | (w2<<32) | (w1<<16) | w0;

        memcpy(&val, &lval, sizeof(double));
        // todo : consider ASLO and AOFF field
        // todo : consider SMOO field
        prec->val = val;
        prec->udf = isnan(prec->val);
        return 2; // no conversion
    } else if (conv == 'F') {
        float val;
        uint32_t lval = (wdata[1]<<16) | wdata[0];
        memcpy(&val, &lval, sizeof(float));
        // todo : consider ASLO and AOFF field
        // todo : consider SMOO field
        prec->val = val;
        prec->udf = isnan(prec->val);
        return 2; // no conversion
    } else if (conv == 'L') {
        prec->rval = wdata[1]<<16 | wdata[0];
    } else if (conv == 'U') {
        prec->rval = (uint16_t)wdata[0];
    } else {
        prec->rval = (int16_t)wdata[0];
    }

    //
    return 0;
}
