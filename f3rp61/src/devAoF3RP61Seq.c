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
static long init_record(aoRecord *precord)
{
    //
    struct link *plink = &precord->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devAoF3RP61Seq (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SEQ_DPVT), "calloc failed");

    //
    const int ret = f3rp61seqParseLink(plink, dpvt, kWrite, kWord, (dbCommon *)precord, "devAoF3RP61Seq");
    if (ret < 0) {
        //errlogPrintf("devAoF3RP61Seq: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }


    // Check conversion Option
    const int8_t option = dpvt->option;
    if (option == 'W') {        // Dummy option for Word access
    } else if (option == 'U') { // Unsigned integer, perhaps we'd better disable this
    } else if (option == 'L') { // Long word
    } else if (option == 'F') { // Single precision floating point
    } else if (option == 'D') { // Double precision floating point
    } else {                    // Option not recognized
        errlogPrintf("devAoF3RP61Seq: %s : unsupported option \'%c\'\n", precord->name, option);
        precord->pact = 1;
        return -1;
    }

    //
    callbackSetUser(precord, &dpvt->callback);
    precord->dpvt = dpvt;

    return 0;
}

// write_ao() is called when there was a request to process a record.
// When called, it sends the value from the VAL filed to the driver,
// then sets PACT field back to TRUE.
static long write_ao(aoRecord *precord)
{
    F3RP61SEQ_DPVT *dpvt = precord->dpvt;
    int retval = 0; // with conversion

    if (precord->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devAoF3RP61Seq: %s : write_ao failed\n", precord->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devAoF3RP61Seq: %s : errorCode 0x%04x returned\n", precord->name, pmcmdResponse->errorCode);
            return -1;
        }

        //
        precord->udf = FALSE;

    } else { // First call (PACT is still FALSE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        uint16_t *wdata = pM3WriteSeqdev->dataBuff.wData;

        //
        const char option = dpvt->option;
        if (option == 'D') {
            double val = precord->val;
            // todo : consider ASLO and AOFF field

            uint64_t lval;
            memcpy(&lval, &val, sizeof(double));
            wdata[0] = (uint16_t)(lval>> 0);
            wdata[1] = (uint16_t)(lval>>16);
            wdata[2] = (uint16_t)(lval>>32);
            wdata[3] = (uint16_t)(lval>>48);

            precord->udf = isnan(val); // does this make sense?
            // it seems that returning 2 (=no conversion) is meaningless
            //retval = 2; // no conversion

        } else if (option == 'F') {
            float val = precord->val;
            // todo : consider ASLO and AOFF field

            uint32_t lval;
            memcpy(&lval, &val, sizeof(float));
            wdata[0] = (uint16_t)(lval>> 0);
            wdata[1] = (uint16_t)(lval>>16);

            precord->udf = isnan(val); // does this make sense?
            // it seems that returning 2 (=no conversion) is meaningless
            //retval = 2; // no conversion

        } else if (option == 'L') {
            wdata[0] = (uint16_t)(precord->rval>> 0);
            wdata[1] = (uint16_t)(precord->rval>>16);

        } else if (option == 'U') {
            wdata[0] = (uint16_t)precord->rval;

        } else {
            wdata[0] = (int16_t)precord->rval;

        }

        // Issue write request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devAoF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", precord->name);
            return -1;
        }

        precord->pact = 1;
    }

    return retval;
}
