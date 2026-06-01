/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devLiF3RP61Seq.c - Device Support Routines for F3RP61 Long Input
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*
*      Modified: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/

//
#include <longinRecord.h>

//
#include <drvF3RP61Seq.h>
#include <devF3RP61bcd.h>

// Create the dset for devLiF3RP61Seq
static long init_record();
static long read_longin();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_longin;
} devLiF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_longin
};

epicsExportAddress(dset, devLiF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(longinRecord *precord)
{
    //
    struct link *plink = &precord->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devLiF3RP61Seq (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");

    //
    const int ret = f3rp61seqParseLink(plink, dpvt, kRead, kWord, (dbCommon *)precord, "devLiF3RP61Seq");
    if (ret < 0) {
        //errlogPrintf("devLiF3RP61Seq: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
    } else if (conv == 'B') { // Binary Coded Decimal format
    } else if (conv == 'U') { // Unsigned integer
    } else if (conv == 'L') { // Long word
    } else {
        errlogPrintf("devLiF3RP61Seq: %s : unsupported conversion specifier \'%c\'\n", precord->name, conv);
        precord->pact = 1;
        return -1;
    }

    //
    callbackSetUser(precord, &dpvt->callback);
    precord->dpvt = dpvt;

    return 0;
}

// read_longin() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field, then sets PACT field back to TRUE.
static long read_longin(longinRecord *precord)
{
    F3RP61SEQ_DPVT *dpvt = precord->dpvt;

    if (precord->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devLiF3RP61Seq: %s : read_longin failed\n", precord->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;
        uint16_t *wdata = pmcmdResponse->dataBuff.wData;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devLiF3RP61Seq: %s : errorCode 0x%04x returned\n", precord->name, pmcmdResponse->errorCode);
            return -1;
        }

        //
        precord->udf = FALSE;

        // fill VAL field
        const char conv = dpvt->conv;
        if (conv == 'B') {
            precord->val = devF3RP61bcd2int(wdata[0], precord);

        } else if (conv == 'L') {
            precord->val = wdata[1]<<16 | wdata[0];

        } else if (conv == 'U') {
            precord->val = (uint16_t)wdata[0];

        } else {
            precord->val = (int16_t)wdata[0];
        }

    } else { // First call (PACT is still FALSE)
        // Issue read request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devLiF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", precord->name);
            return -1;
        }

        precord->pact = 1;
    }

    return 0;
}
