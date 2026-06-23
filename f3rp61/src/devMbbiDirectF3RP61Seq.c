/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiDirectF3RP61Seq.c - Device Support Routines for F3RP61 Multi-bit
* Binary Direct Input
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <mbbiDirectRecord.h>

//
#include <drvF3RP61Seq.h>

// Create the dset for devMbbiDirectF3RP61Seq
static long init_record();
static long read_mbbiDirect();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_mbbiDirect;
} devMbbiDirectF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_mbbiDirect
};

epicsExportAddress(dset, devMbbiDirectF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbbiDirectRecord *prec)
{
    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devMbbiDirectF3RP61Seq (init_record) Illegal INP field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61seqParseLink(plink, kRead, kWord, (dbCommon *)prec);
    if (ret < 0) {
        //errlogPrintf("devMbbiF3RP61Seq: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    } else if (conv == 'U') { // Unsigned integer
        prec->nobt = 16;
        prec->mask = 0xffff;
        prec->shft = 0;
    } else if (conv == 'L') { // Long word
        prec->nobt = 32;
        prec->mask = 0xffffffff;
        prec->shft = 0;
    } else {
        errlogPrintf("devMbbiDirectF3RP61Seq: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    //
    callbackSetUser(prec, &dpvt->callback);

    return 0;
}

// read_mbbiDirect() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field, then sets PACT field back to TRUE.
static long read_mbbiDirect(mbbiDirectRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devMbbiDirectF3RP61Seq: %s :read_mbbiDirect failed\n", prec->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;
        uint16_t *wdata = pmcmdResponse->dataBuff.wData;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devMbbiDirectF3RP61Seq: %s : errorCode 0x%04x returned\n", prec->name, pmcmdResponse->errorCode);
            return -1;
        }

        //
        prec->udf = FALSE;

        // fill VAL field
        const char conv = dpvt->conv;
        if (conv == 'L') {
            prec->rval = wdata[1]<<16 | wdata[0];

        } else if (conv == 'U') {
            prec->rval = (uint16_t)wdata[0];

        } else {
            prec->rval = (int16_t)wdata[0];
        }

    } else { // First call (PACT is still FALSE)
        // Issue read request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devMbbiDirectF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            return -1;
        }

        prec->pact = 1;
    }

    return 0;
}
