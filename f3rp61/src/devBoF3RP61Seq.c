/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBoF3RP61Seq.c - Device Support Routines for F3RP61 Binary Output
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*/

//
#include <boRecord.h>

//
#include <drvF3RP61Seq.h>

// Create the dset for devBoF3RP61Seq
static long init_record();
static long write_bo();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_bo;
} devBoF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    write_bo
};

epicsExportAddress(dset, devBoF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(boRecord *prec)
{
    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devBoF3RP61Seq (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61seqParseLink(plink, kWrite, kBit, (dbCommon *)prec);
    if (ret < 0) {
        //errlogPrintf("devLoF3RP61Seq: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
    } else {
        errlogPrintf("devBoF3RP61Seq: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    //
    callbackSetUser(prec, &dpvt->callback);

    return 0;
}

// write_bo() is called when there was a request to process a record.
// When called, it sends the value from the VAL filed to the driver,
// then sets PACT field back to TRUE.
static long write_bo(boRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devBoF3RP61Seq: %s : write_bo failed\n", prec->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devBoF3RP61Seq: %s : errorCode 0x%04x returned\n", prec->name, pmcmdResponse->errorCode);
            return -1;
        }

        //
        prec->udf = FALSE;

    } else { // First call (PACT is still FALSE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

        //
        pM3WriteSeqdev->dataBuff.wData[0] = (unsigned short) prec->rval;

        // Issue write request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devBoF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            return -1;
        }

        prec->pact = 1;
    }

    return 0;
}
