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
        return S_db_badField;
    }

    //
    const uint32_t nelm = 1;
    const int ret = f3rp61seqParseLink(plink, kWrite, kBit, (dbCommon *)prec, DBF_ENUM, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return 0;
    }

    return 0;
}

// write_bo() is called when there was a request to process a record.
// When called, it sends the value from the VAL filed to the driver,
// then sets PACT field back to TRUE.
static long write_bo(boRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in OUT field and init_record() failed
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

        //
        prec->udf = FALSE;

    } else { // First call (PACT is FALSE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

        //
        pM3WriteSeqdev->dataBuff.wData[0] = (unsigned short) prec->rval;

        // Issue write request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devBoF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

        prec->pact = 1;
    }

    return 0;
}
