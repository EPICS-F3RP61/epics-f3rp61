/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBiF3RP61Seq.c - Device Support Routines for F3RP61 Binary Input
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*/

//
#include <biRecord.h>

//
#include <drvF3RP61Seq.h>

// Create the dset for devBiF3RP61Seq
static long init_record();
static long read_bi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_bi;
} devBiF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_bi
};

epicsExportAddress(dset, devBiF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(biRecord *prec)
{
    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devBiF3RP61Seq (init_record) Illegal INP field");
        return S_db_badField;
    }

    //
    const uint32_t nelm = 1;
    const int ret = f3rp61seqParseLink(plink, kRead, kBit, (dbCommon *)prec, DBF_ENUM, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 0;
    }

    return 0;
}

// read_bi() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field, then sets PACT field back to TRUE.
static long read_bi(biRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in INP field and init_record() failed
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            return -1;
        }

        //
        prec->udf = FALSE;

        //
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;
        //uint16_t *wdata = pmcmdResponse->dataBuff.wData;

        // fill VAL field
        prec->rval = (unsigned long) pmcmdResponse->dataBuff.wData[0];

    } else { // First call (PACT is FALSE)
        // Issue read request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devBiF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            return -1;
        }

        prec->pact = 1;
    }

    return 0;
}
