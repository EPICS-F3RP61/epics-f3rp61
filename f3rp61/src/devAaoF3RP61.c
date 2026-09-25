/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAaoF3RP61.c - Device Support Routines for F3RP61 Array Analog Output
*
*      Author: Shuei YAMADA (KEK/J-PARC)
*      Date: 2026-06-02
*/

//
#include <aaoRecord.h>

//
#include <drvF3RP61_private.h>

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
        return S_db_badField;
    }

    //
    const dbfType  ftvl = prec->ftvl;
    const uint32_t nelm = prec->nelm;
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, ftvl, nelm);
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
// When called, it sends the value from the VAL field to the driver.
static long write_aao(aaoRecord *prec)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in OUT field and init_record() failed
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //debug
    //fprintf(stderr, "%s : %s : dpvt->count=%d dpvt->nord=%d prec->nord=%d\n", __func__, prec->name, dpvt->count, dpvt->nord, prec->nord);

    //
    prec->udf = FALSE;

    //
    int32_t nord = dpvt->nord;
    const dbfType ftvl = prec->ftvl;

    // Client may put number of elements smaller than NORD, but NORD should not be made smaller.
    // The array widget of CSS/Boy will disable elements that exceed NORD, thus prevents value input.
    //if (nord > prec->nord) {
    //    nord = prec->nord;
    //}

    // Compose data to write
    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        devF3RP61double2buf(prec->bptr, dpvt->buf, dpvt->conv, nord);
    } else if (ftvl == DBF_FLOAT) {
        devF3RP61float2buf(prec->bptr, dpvt->buf, dpvt->conv, nord);
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        int ret = devF3RP61long2buf(prec->bptr, dpvt->buf, dpvt->conv, nord);
        if (ret < 0) {
            // overflow happend in ushort2bcd
            recGblSetSevr(prec, HW_LIMIT_ALARM, INVALID_ALARM);
        }
    } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
        int ret = devF3RP61short2buf(prec->bptr, dpvt->buf, dpvt->conv, nord);
        if (ret < 0) {
            // overflow happend in ushort2bcd
            recGblSetSevr(prec, HW_LIMIT_ALARM, INVALID_ALARM);
        }
    }

    // Issue API function
    nord = f3rp61Write((dbCommon*)prec, nord); // nord must be identical to dpvt->nord, if no error
    if (nord < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->nord = nord;

    //
    return 0;
}
