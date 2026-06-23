/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbboDirectF3RP61Seq.c - Device Support Routines for F3RP61 Multi-bit
* Binary Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <mbboDirectRecord.h>

//
#include <drvF3RP61Seq.h>

// Create the dset for devMbboDirectF3RP61Seq
static long init_record();
static long write_mbboDirect();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_mbboDirect;
} devMbboDirectF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    write_mbboDirect
};

epicsExportAddress(dset, devMbboDirectF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbboDirectRecord *prec)
{
    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devMbboDirectF3RP61Seq (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61seqParseLink(plink, kWrite, kWord, (dbCommon *)prec);
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
        errlogPrintf("devMbboDirectF3RP61Seq: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }


    //
    callbackSetUser(prec, &dpvt->callback);

    return 2; // no conversion
}

// write_mbboDirect() is called when there was a request to process a record.
// When called, it sends the value from the VAL filed to the driver,
// then sets PACT field back to TRUE.
static long write_mbboDirect(mbboDirectRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devMbboDirectF3RP61Seq: %s : write_mbboDirect failed\n", prec->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devMbboDirectF3RP61Seq: %s : errorCode 0x%04x returned\n", prec->name, pmcmdResponse->errorCode);
            return -1;
        }

        //
        prec->udf = FALSE;

    } else { // First call (PACT is still FALSE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        uint16_t *wdata = pM3WriteSeqdev->dataBuff.wData;

        //
        const char conv = dpvt->conv;
        if (conv == 'L') {
            wdata[0] = (uint16_t)(prec->rval>> 0);
            wdata[1] = (uint16_t)(prec->rval>>16);

        } else if (conv == 'U') {
            wdata[0] = (uint16_t)prec->rval;

        } else {
            wdata[0] = (int16_t)prec->rval;

        }

        // Issue write request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devMbboDirectF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            return -1;
        }

        prec->pact = 1;
    }

    return 0;
}
