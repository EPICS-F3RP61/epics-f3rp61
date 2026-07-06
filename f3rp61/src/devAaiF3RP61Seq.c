/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAaiF3RP61Seq.c - Device Support Routines for F3RP61 Array Analog Input
*
*      Author: Shuei YAMADA (KEK/J-PARC)
*      Date: 2026-07-03
*/

//
#include <aaiRecord.h>

//
#include <drvF3RP61Seq.h>

//
#include <math.h>

// Create the dset for devAiF3RP61Seq
static long init_record();
static long read_aai();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_aai;
    DEVSUPFUN  special_linconv;
} devAaiF3RP61Seq = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    read_aai,
    NULL
};

epicsExportAddress(dset, devAaiF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aaiRecord *prec)
{
    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAaiF3RP61Seq (init_record) Illegal INP field");
        return S_db_badField;
    }

    //
    const dbfType  ftvl = prec->ftvl;
    const uint32_t nelm = prec->nelm;
    const int ret = f3rp61seqParseLink(plink, kRead, kWord, (dbCommon *)prec, ftvl, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 0;
    }

    //
    return 0;
}

// read_aai() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field, then sets PACT field back to TRUE.
static long read_aai(aaiRecord *prec)
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
        uint16_t *buf = pmcmdResponse->dataBuff.wData;

        // fill VAL field
        int32_t nord = dpvt->ret; // dpvt->nord;
        const dbfType ftvl = prec->ftvl;
        if (0) {
        } else if (ftvl == DBF_DOUBLE) {
            devF3RP61buf2double(buf, prec->bptr, dpvt->conv, nord);
        } else if (ftvl == DBF_FLOAT) {
            devF3RP61buf2float(buf, prec->bptr, dpvt->conv, nord);
        } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
            int ret = devF3RP61buf2long(buf, prec->bptr, dpvt->conv, nord);
            if (ret < 0) {
                // overflow happend in bcd2ushort
                recGblSetSevr(prec, HIGH_ALARM, INVALID_ALARM);
            }
        } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
            int ret = devF3RP61buf2short(buf, prec->bptr, dpvt->conv, nord);
            if (ret < 0) {
                // overflow happend in bcd2ushort
                recGblSetSevr(prec, HIGH_ALARM, INVALID_ALARM);
            }
        }

        //
        prec->nord = nord;

        return 0;

    } else { // First call (PACT is FALSE)
        // Issue read request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devAiF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            return -1;
        }

        dpvt->nord = 0;
        prec->pact = 1;
    }

    return 0;
}
