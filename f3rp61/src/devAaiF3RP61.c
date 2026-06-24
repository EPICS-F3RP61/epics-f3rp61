/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAaiF3RP61.c - Device Support Routines for F3RP61 Array Analog Input
*
*      Author: Shuei YAMADA
*      Date: 2026-06-02
*/

//
#include <aaiRecord.h>

//
#include <drvF3RP61.h>

//
static const F3RP61_RW rw = kRead;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devAaiF3RP61
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
} devAaiF3RP61 = {
    6,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_aai,
    NULL
};

epicsExportAddress(dset, devAaiF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aaiRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devAaiF3RP61 (init_record) Illegal INP field");
        prec->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    const uint32_t nelm = prec->nelm;
    prec->dpvt = dpvt;

    //
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, nelm);
    if (ret < 0) {
        //errlogPrintf("devAaiF3RP61: %s : syntax error in INP field\n", prec->name);
        prec->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const dbfType  ftvl    = prec->ftvl;
    const char    *ftvlstr = (pamapdbfType[ftvl].strvalue) + 4;
    const int8_t   conv    = dpvt->conv;
    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        } else if (conv == 'L') { // Long word
        } else if (conv == 'F') { // Single precision floating point
        } else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else if (ftvl == DBF_FLOAT) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        } else if (conv == 'L') { // Long word
        } else if (conv == 'F') { // Single precision floating point
        //} else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        } else if (conv == 'L') { // Long word
        //} else if (conv == 'F') { // Single precision floating point
        //} else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else if (ftvl == DBF_SHORT || ftvl == DBF_USHORT) {
        if (conv == 'W') {        // Dummy for Word access
        } else if (conv == 'U') { // Unsigned integer
        //} else if (conv == 'L') { // Long word
        //} else if (conv == 'F') { // Single precision floating point
        //} else if (conv == 'D') { // Double precision floating point
        } else {
            errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\' with FTVL field %s\n", prec->name, conv, ftvlstr);
            prec->pact = 1;
            return -1;
        }
    } else {
        errlogPrintf("devAaiF3RP61: %s : unsupported conversion specifier \'%c\'\n", prec->name, conv);
        prec->pact = 1;
        return -1;
    }

    //
    return 0;
}

// read_aai() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_aai(aaiRecord *prec)
{
    //
    const uint32_t nelm = prec->nelm;

    // Issue API function
    const int32_t nord = f3rp61Read((dbCommon*)prec, nelm);
    if (nord < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    // fill VAL field
    const dbfType  ftvl   = prec->ftvl;
    F3RP61_DPVT   *dpvt   = prec->dpvt;
    const int8_t   conv   = dpvt->conv;
    uint16_t      *wdata  = dpvt->buf;

    if (0) {
    } else if (ftvl == DBF_DOUBLE) {
        double *bptr = prec->bptr;
        if (0) {
        } else if (conv == 'D') {
            for (uint32_t i=0; i<nord; i++) {
                double val;
                uint64_t w0 = wdata[4*i + 0];
                uint64_t w1 = wdata[4*i + 1];
                uint64_t w2 = wdata[4*i + 2];
                uint64_t w3 = wdata[4*i + 3];
                uint64_t lval = (w3<<48) | (w2<<32) | (w1<<16) | w0;
                memcpy(&val, &lval, sizeof(double));
                bptr[i] = val;
            }
        } else if (conv == 'F') {
            for (uint32_t i=0; i<nord; i++) {
                float val;
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                uint32_t lval = (w1<<16) | w0;
                memcpy(&val, &lval, sizeof(float));
                bptr[i] = val;
            }
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nord; i++) {
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                int32_t lval = (w1<<16) | w0; // 'L' is signed
                bptr[i] = lval;
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nord; i++) {
                bptr[i] = (uint16_t)wdata[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nord; i++) {
                bptr[i] = (int16_t)wdata[i];
            }
        }
    } else if (ftvl == DBF_FLOAT) {
        float *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        } else if (conv == 'F') {
            for (uint32_t i=0; i<nord; i++) {
                float val;
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                uint32_t lval = (w1<<16) | w0;
                memcpy(&val, &lval, sizeof(float));
                bptr[i] = val;
            }
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nord; i++) {
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                int32_t lval = (w1<<16) | w0; // 'L' is signed
                bptr[i] = lval;
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nord; i++) {
                bptr[i] = (uint16_t)wdata[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nord; i++) {
                bptr[i] = (int16_t)wdata[i];
            }
        }
    } else if (ftvl == DBF_LONG || ftvl == DBF_ULONG) {
        uint32_t *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        //} else if (conv == 'F') {
        } else if (conv == 'L') {
            for (uint32_t i=0; i<nord; i++) {
                uint32_t w0 = wdata[2*i + 0];
                uint32_t w1 = wdata[2*i + 1];
                uint32_t lval = (w1<<16) | w0;
                bptr[i] = lval;
            }
        } else if (conv == 'U') {
            for (uint32_t i=0; i<nord; i++) {
                bptr[i] = (uint16_t)wdata[i];
            }
        } else {// conv == 'W'
            for (uint32_t i=0; i<nord; i++) {
                bptr[i] = (int16_t)wdata[i];
            }
        }
    } else {//(ftvl == DBF_SHORT || ftvl == DBF_USHORT)
        uint16_t *bptr = prec->bptr;
        if (0) {
        //} else if (conv == 'D') {
        //} else if (conv == 'F') {
        //} else if (conv == 'L') {
        } else {// conv == 'U' || conv == 'W'
            for (uint32_t i=0; i<nord; i++) {
                bptr[i] = wdata[i];
            }
        }
    }

    //
    prec->nord = nord;

    //
    return 0;
}
