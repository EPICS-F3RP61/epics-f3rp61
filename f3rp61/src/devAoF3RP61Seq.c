/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAoF3RP61Seq.c - Device Support Routines for F3RP61 Analog Output
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*/

//
#include <aoRecord.h>

//
#include <drvF3RP61Seq.h>

//
#include <math.h>

// Create the dset for devAoF3RP61Seq
static long init_record();
static long write_ao();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_ao;
    DEVSUPFUN  special_linconv;
} devAoF3RP61Seq = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    write_ao,
    NULL
};

epicsExportAddress(dset, devAoF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aoRecord *prec)
{
    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAoF3RP61Seq (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61seqParseLink(plink, kWrite, kWord, (dbCommon *)prec);
    if (ret < 0) {
        //errlogPrintf("devAoF3RP61Seq: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }


    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
    } else if (conv == 'U') { // Unsigned integer, perhaps we'd better disable this
    } else if (conv == 'L') { // Long word
    } else if (conv == 'F') { // Single precision floating point
    } else if (conv == 'D') { // Double precision floating point
    } else {
        errlogPrintf("devAoF3RP61Seq: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    //
    callbackSetUser(prec, &dpvt->callback);

    return 0;
}

// write_ao() is called when there was a request to process a record.
// When called, it sends the value from the VAL filed to the driver,
// then sets PACT field back to TRUE.
static long write_ao(aoRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;
    int retval = 0; // with conversion

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

        //
        const char conv = dpvt->conv;
        if (conv == 'D') {
            double val = prec->val;
            // todo : consider ASLO and AOFF field

            uint64_t lval;
            memcpy(&lval, &val, sizeof(double));
            wdata[0] = (uint16_t)(lval>> 0);
            wdata[1] = (uint16_t)(lval>>16);
            wdata[2] = (uint16_t)(lval>>32);
            wdata[3] = (uint16_t)(lval>>48);

            prec->udf = isnan(val); // does this make sense?
            // it seems that returning 2 (=no conversion) is meaningless
            //retval = 2; // no conversion

        } else if (conv == 'F') {
            float val = prec->val;
            // todo : consider ASLO and AOFF field

            uint32_t lval;
            memcpy(&lval, &val, sizeof(float));
            wdata[0] = (uint16_t)(lval>> 0);
            wdata[1] = (uint16_t)(lval>>16);

            prec->udf = isnan(val); // does this make sense?
            // it seems that returning 2 (=no conversion) is meaningless
            //retval = 2; // no conversion

        } else if (conv == 'L') {
            wdata[0] = (uint16_t)(prec->rval>> 0);
            wdata[1] = (uint16_t)(prec->rval>>16);

        } else if (conv == 'U') {
            wdata[0] = (uint16_t)prec->rval;

        } else {
            wdata[0] = (int16_t)prec->rval;

        }

        // Issue write request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devAoF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

        prec->pact = 1;
    }

    return retval;
}
