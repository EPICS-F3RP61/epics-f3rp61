/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAaoF3RP61.c - Device Support Routines for F3RP61 Array Analog Output
*
*      Author: Shuei YAMADA
*      Date: 2026-06-02
*/

//
#include <aaoRecord.h>

//
#include <drvF3RP61.h>

//
#include <math.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devAaoF3RP61
static long init_record();
static long write_aao();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_aaao;
    DEVSUPFUN  special_linconv;
} devAaoF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_aao,
    NULL
};

epicsExportAddress(dset, devAaoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aaoRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAaoF3RP61 (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    prec->dpvt = dpvt;
    const dbfType  ftvl = prec->ftvl;
    const uint32_t nelm = prec->nelm;

    //
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, ftvl, nelm);
    if (ret < 0) {
        //errlogPrintf("devAaoF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    //
    return 0;
}

// write_aao() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_aao(aaoRecord *prec)
{
    F3RP61_DPVT   *dpvt = prec->dpvt;
    const uint32_t nelm = prec->nelm;
    const dbfType  ftvl = prec->ftvl;
    //const int8_t   conv = dpvt->conv;

    // Compose data to write
    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        devF3RP61double2buf(prec->bptr, dpvt->buf, dpvt->conv, nelm);
    } else if (ftvl == DBF_FLOAT) {
        devF3RP61float2buf(prec->bptr, dpvt->buf, dpvt->conv, nelm);
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        int ret = devF3RP61int2buf(prec->bptr, dpvt->buf, dpvt->conv, nelm); // nord
        if (!ret) {
            // overflow happend in int2bcd
            recGblSetSevr(prec, HW_LIMIT_ALARM, INVALID_ALARM);
        }
    } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
        const uint16_t *bptr  = prec->bptr;
        uint16_t       *wdata = dpvt->buf;
        if (0) {
        //} else if (conv == 'D') {
        //} else if (conv == 'F') {
        //} else if (conv == 'L') {
        } else {// conv == 'U' || conv == 'W'
            for (uint32_t i=0; i<nelm; i++) {
                wdata[i] = (uint16_t)bptr[i];
            }
        }
    }

    // Issue API function
    const int32_t nord = f3rp61Write((dbCommon*)prec, nelm);
    if (nord < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    //
    prec->nord = nord;

    //
    return 0;
}
