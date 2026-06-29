/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devLoF3RP61Seq.c - Device Support Routines for F3RP61 Long Output
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*
*      Modified: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/

//
#include <longoutRecord.h>

//
#include <drvF3RP61Seq.h>

// Create the dset for devLoF3RP61Seq
static long init_record();
static long write_longout();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_longout;
} devLoF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    write_longout
};

epicsExportAddress(dset, devLoF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(longoutRecord *prec)
{
    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devLoF3RP61Seq (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");
    prec->dpvt = dpvt;
    const uint32_t nelm = 1;

    //
    const int ret = f3rp61seqParseLink(plink, kWrite, kWord, (dbCommon *)prec, DBF_LONG, nelm);
    if (ret < 0) {
        //errlogPrintf("devLoF3RP61Seq: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    //
    callbackSetUser(prec, &dpvt->callback);

    return 0;
}

// write_longout() is called when there was a request to process a record.
// When called, it sends the value from the VAL filed to the driver,
// then sets PACT field back to TRUE.
static long write_longout(longoutRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;
    const uint32_t nelm = 1;

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

        //
        prec->udf = FALSE;

    } else { // First call (PACT is still FALSE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        uint16_t *wdata = pM3WriteSeqdev->dataBuff.wData;

        // Compose data to write
        int ret = devF3RP61int2buf(&prec->val, wdata, dpvt->conv, nelm);
        if (!ret) {
            // overflow happend in int2bcd
            recGblSetSevr(prec, HW_LIMIT_ALARM, INVALID_ALARM);
        }

        // Issue write request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devLoF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

        prec->pact = 1;
    }

    return 0;
}
