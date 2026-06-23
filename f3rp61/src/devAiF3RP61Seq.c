/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAiF3RP61Seq.c - Device Support Routines for F3RP61 Analog Input
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*/

//
#include <aiRecord.h>

//
#include <drvF3RP61Seq.h>

//
#include <math.h>

// Create the dset for devAiF3RP61Seq
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
} devAiF3RP61Seq = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    read_ai,
    NULL
};

epicsExportAddress(dset, devAiF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aiRecord *prec)
{
    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAiF3RP61Seq (init_record) Illegal INP field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61seqParseLink(plink, kRead, kWord, (dbCommon *)prec);
    if (ret < 0) {
        //errlogPrintf("devAiF3RP61Seq: %s : syntax error in INP field\n", prec->name);
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
        errlogPrintf("devAiF3RP61Seq: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
            return -1;
    }

    //
    callbackSetUser(prec, &dpvt->callback);

    return 0;
}

// read_ai() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field, then sets PACT field back to TRUE.
static long read_ai(aiRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devAiF3RP61Seq: %s : read_ai failed\n", prec->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;
        uint16_t *wdata = pmcmdResponse->dataBuff.wData;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devAiF3RP61Seq: %s : errorCode 0x%04x returned\n", prec->name, pmcmdResponse->errorCode);
            return -1;
        }

        //
        prec->udf = FALSE;

        // fill VAL field
        const char conv = dpvt->conv;
        if (conv == 'D') {
            const uint64_t l0 = wdata[0];
            const uint64_t l1 = wdata[1];
            const uint64_t l2 = wdata[2];
            const uint64_t l3 = wdata[3];
            const uint64_t lval = (l3<<48) | (l2<<32) | (l1<<16) | l0;
            double val;
            memcpy(&val, &lval, sizeof(double));

            // todo : consider ASLO and AOFF field
            // todo : consider SMOO field
            prec->val = val;
            prec->udf = isnan(val);
            return 2; // no conversion

        } else if (conv == 'F') {
            const uint32_t l0 = wdata[0];
            const uint32_t l1 = wdata[1];
            const uint32_t lval = (l1<<16) | l0;
            float val;
            memcpy(&val, &lval, sizeof(float));

            // todo : consider ASLO and AOFF field
            // todo : consider SMOO field
            prec->val = val;
            prec->udf = isnan(val);
            return 2; // no conversion

        } else if (conv == 'L') {
            const uint32_t l0 = wdata[0];
            const uint32_t l1 = wdata[1];
            prec->rval = l1<<16 | l0;

        } else if (conv == 'U') {
            prec->rval = (uint16_t)wdata[0];

        } else {
            prec->rval = (int16_t)wdata[0];

        }

    } else { // First call (PACT is still FALSE)
        // Issue read request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devAiF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            return -1;
        }

        prec->pact = 1;
    }

    return 0; // with conversion
}
