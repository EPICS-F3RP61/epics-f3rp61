/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiF3RP61Seq.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
*
*      Author: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/

//
#include <mbbiRecord.h>

//
#include <drvF3RP61Seq.h>

// Create the dset for devMbbiF3RP61Seq
static long init_record();
static long read_mbbi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_mbbi;
} devMbbiF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_mbbi
};

epicsExportAddress(dset, devMbbiF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbbiRecord *precord)
{
    //
    struct link *plink = &precord->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devMbbiF3RP61Seq (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");

    //
    const int ret = f3rp61seqParseLink(plink, dpvt, kRead, kWord, (dbCommon *)precord, "devMbbiF3RP61Seq");
    if (ret < 0) {
        //errlogPrintf("devMbbiF3RP61Seq: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion Option
    const int8_t option = dpvt->option;
    if (option == 'W') {        // Dummy option for Word access
        precord->nobt = 16;
        precord->mask = 0xffff;
        precord->shft = 0;
    } else if (option == 'U') { // Unsigned integer
        precord->nobt = 16;
        precord->mask = 0xffff;
        precord->shft = 0;
    } else if (option == 'L') { // Long word
        precord->nobt = 32;
        precord->mask = 0xffffffff;
        precord->shft = 0;
    } else {                    // Option not recognized
        errlogPrintf("devMbbiF3RP61Seq: %s : unsupported option \'%c\'\n", precord->name, option);
        precord->pact = 1;
        return -1;
    }

    //
    callbackSetUser(precord, &dpvt->callback);
    precord->dpvt = dpvt;

    return 0;
}

// read_mbbi() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field, then sets PACT field back to TRUE.
static long read_mbbi(mbbiRecord *precord)
{
    F3RP61SEQ_DPVT *dpvt = precord->dpvt;

    if (precord->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devMbbiF3RP61Seq: %s : read_mbbi failed\n", precord->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;
        uint16_t *wdata = pmcmdResponse->dataBuff.wData;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devMbbiF3RP61Seq: %s : errorCode 0x%04x returned\n", precord->name, pmcmdResponse->errorCode);
            return -1;
        }

        //
        precord->udf = FALSE;

        // fill VAL field
        const char option = dpvt->option;
        if (option == 'L') {
            precord->rval = wdata[1]<<16 | wdata[0];

        } else if (option == 'U') {
            precord->rval = (uint16_t)wdata[0];

        } else {
            precord->rval = (int16_t)wdata[0];
        }

    } else { // First call - PACT is set to FALSE
        // Issue read request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devMbbiF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", precord->name);
            return -1;
        }

        precord->pact = 1;
    }

    return 0;
}
