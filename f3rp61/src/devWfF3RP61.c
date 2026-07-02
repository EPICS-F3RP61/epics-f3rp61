/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devWfF3RP61.c - Device Support Routines for F3RP61 Waveform
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <waveformRecord.h>

//
#include <drvF3RP61.h>

//
static const F3RP61_RW rw = kRead;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devWfF3RP61
static long init_record();
static long read_wf();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_wf;
    DEVSUPFUN  special_linconv;
} devWfF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_wf,
    NULL
};

epicsExportAddress(dset, devWfF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(waveformRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devWfF3RP61 (init_record) Illegal INP field");
        return S_db_badField;
    }

    //
    const dbfType  ftvl = prec->ftvl;
    const uint32_t nelm = prec->nelm;
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, ftvl, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 0;
    }

    //
    return 0;
}

// read_wf() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_wf(waveformRecord *prec)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in OUT field and init_record() failed
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    //debug
    //fprintf(stderr, "%s : %s : dpvt->count=%d dpvt->nord=%d prec->nord=%d\n", __func__, prec->name, dpvt->count, dpvt->nord, prec->nord);

    //
    prec->udf = FALSE;

    //
    int32_t nord = dpvt->nord;
    const dbfType ftvl = prec->ftvl;

    // Issue API function
    nord = f3rp61Read((dbCommon*)prec, nord); // nord must be identical to dpvt->nord, if no error
    if (nord < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    // fill VAL field
    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        devF3RP61buf2double(dpvt->buf, prec->bptr, dpvt->conv, nord);
    } else if (ftvl == DBF_FLOAT) {
        devF3RP61buf2float(dpvt->buf, prec->bptr, dpvt->conv, nord);
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        int ret = devF3RP61buf2long(dpvt->buf, prec->bptr, dpvt->conv, nord);
        if (ret < 0) {
            // overflow happend in bcd2ushort
            recGblSetSevr(prec, HIGH_ALARM, INVALID_ALARM);
        }
    } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
        int ret = devF3RP61buf2short(dpvt->buf, prec->bptr, dpvt->conv, nord);
        if (ret < 0) {
            // overflow happend in bcd2ushort
            recGblSetSevr(prec, HIGH_ALARM, INVALID_ALARM);
        }
    }

    //
    prec->nord = nord;

    //
    return 0;
}
