/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAaoF3RP61Seq.c - Device Support Routines for F3RP61 Array Analog Output
*
*      Author: Shuei YAMADA (KEK/J-PARC)
*      Date: 2026-07-03
*/

//
#include <aaoRecord.h>

//
#include <drvF3RP61Seq.h>

//
#include <math.h>

// Create the dset for devAoF3RP61Seq
static long init_record();
static long write_aao();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_aao;
    DEVSUPFUN  special_linconv;
} devAaoF3RP61Seq = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    write_aao,
    NULL
};

epicsExportAddress(dset, devAaoF3RP61Seq);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aaoRecord *prec)
{
    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAaoF3RP61Seq (init_record) Illegal OUT field");
        return S_db_badField;
    }

    //
    const dbfType  ftvl = prec->ftvl;
    const uint32_t nelm = prec->nelm;
    const int ret = f3rp61seqParseLink(plink, kWrite, kWord, (dbCommon *)prec, ftvl, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return 0;
    }

    // Set NORD otherwise it becomes zero for records not processed yet.
    // The array widget of CSS/Boy will disable elements that exceed NORD, thus prevents value input.
    prec->nord = ret;

    //
    return 0;
}

// write_aao() is called when there was a request to process a record.
// When called, it sends the value from the VAL filed to the driver,
// then sets PACT field back to TRUE.
static long write_aao(aaoRecord *prec)
{
    F3RP61SEQ_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in OUT field and init_record() failed
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    int ret = 0; // with conversion

    if (prec->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

        //
        prec->udf = FALSE;

        //
        prec->nord = dpvt->ret; // dpvt->nord;

    } else { // First call (PACT is FALSE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        uint16_t *buf = pM3WriteSeqdev->dataBuff.wData;

        // Compose data to write
        int32_t nord = dpvt->nord;
        const dbfType ftvl = prec->ftvl;
        if (0) {
        } else if (ftvl == DBF_DOUBLE) {
            devF3RP61double2buf(prec->bptr, buf, dpvt->conv, nord);
        } else if (ftvl == DBF_FLOAT) {
            devF3RP61float2buf(prec->bptr, buf, dpvt->conv, nord);
        } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
            int ret = devF3RP61long2buf(prec->bptr, buf, dpvt->conv, nord);
            if (ret < 0) {
                // overflow happend in ushort2bcd
                recGblSetSevr(prec, HW_LIMIT_ALARM, INVALID_ALARM);
            }
        } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
            int ret = devF3RP61short2buf(prec->bptr, buf, dpvt->conv, nord);
            if (ret < 0) {
                // overflow happend in ushort2bcd
                recGblSetSevr(prec, HW_LIMIT_ALARM, INVALID_ALARM);
            }
        }

        // Issue write request
        if (f3rp61seqQueueRequest(dpvt) < 0) {
            errlogPrintf("devAoF3RP61Seq: %s : f3rp61seqQueueRequest failed\n", prec->name);
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

        prec->pact = 1;
    }

    return ret;
}
