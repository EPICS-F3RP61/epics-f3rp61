/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBoF3RP61.c - Device Support Routines for F3RP61 Binary Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <boRecord.h>

//
#include <drvF3RP61_private.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kBit;

// Create the dset for devBoF3RP61
static long init_record();
static long write_bo();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_bo;
} devBoF3RP61 = {
    5,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_bo
};

epicsExportAddress(dset, devBoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(boRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devBoF3RP61 (init_record) Illegal OUT field");
        prec->pact = 1;
        return S_db_badField;
    }

    //
    const uint32_t nelm = 1;
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, DBF_ENUM, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return 0;
    }

    //
    return 0;
}

// write_bo() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_bo(boRecord *prec)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in OUT field and init_record() failed
        recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    // Compose data to write
    uint8_t cdata = prec->rval;

    // Issue API function
    const int8_t device = dpvt->device;
    if (0) {                    // dummy

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (writeM3ComRelayB(addr, 1, &cdata) < 0) {
            errlogPrintf("devBoF3RP61: %s : writeM3ComRelayB failed [%d]n", prec->name, errno);
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRelayB(addr, 1, &cdata) < 0) {
            errlogPrintf("devBoF3RP61: %s : writeM3LinkRelayB failed [%d]\n", prec->name, errno);
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }

    } else if (device == 'Y') { // Relays on I/O modules
        M3IO_ACCESS_RELAY_POINT outrlyp = {
            .unitno   = dpvt->unit,
            .slotno   = dpvt->slot,
            .position = dpvt->addr,
            .data     = cdata,
        };
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY_POINT, &outrlyp) < 0) {
            errlogPrintf("devBoF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            recGblSetSevr(prec, WRITE_ALARM, INVALID_ALARM);
            return -1;
        }
    } else {
        //
    }

    //
    return 0;
}
