/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBiF3RP61.c - Device Support Routines for F3RP61 Binary Input
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

//
#include <biRecord.h>

//
#include <drvF3RP61_private.h>

//
static const F3RP61_RW rw = kRead;
static const F3RP61_ACCESS_TYPE type = kBit;

// Create the dset for devBiF3RP61
static long init_record();
static long read_bi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_bi;
} devBiF3RP61 = {
    5,
    NULL,
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_bi
};

epicsExportAddress(dset, devBiF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(biRecord *prec)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, prec->name);

    //
    struct link *plink = &prec->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, prec,
                          "devBiF3RP61 (init_record) Illegal INP field");
        prec->pact = 1;
        return S_db_badField;
    }

    //
    const uint32_t nelm = 1;
    const int ret = f3rp61ParseLink(plink, rw, type, (dbCommon *)prec, DBF_ENUM, nelm);
    if (ret < 0) {
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return 0;
    }

    //
    return 0;
}

// read_bi() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_bi(biRecord *prec)
{
    F3RP61_DPVT *dpvt = prec->dpvt;
    if (!dpvt) { // something was wrong in INP field and init_record() failed
        recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
        return -1;
    }

    //
    prec->udf = FALSE;

    // Buffer for data read
    uint8_t cdata = 0;

    // Issue API function
    const int8_t device = dpvt->device;
    if (0) {                    // dummy

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (readM3ComRelayB(addr, 1, &cdata) < 0) {
            errlogPrintf("devBiF3RP61: %s : readM3ComRelayB failed [%d]\n", prec->name, errno);
            recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            return -1;
        }
        prec->rval = cdata;

    } else if (device == 'L') { // Link realys
        const int32_t addr = dpvt->addr;
        if (readM3LinkRelayB(addr, 1, &cdata) < 0) {
            errlogPrintf("devBiF3RP61: %s : readM3LinkRelayB failed [%d]\n", prec->name, errno);
            recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            return -1;
        }
        prec->rval = cdata;

    } else if (device == 'X') { // Input relays on I/O modules
        M3IO_ACCESS_RELAY_POINT inrlyp = {
            .unitno   = dpvt->unit,
            .slotno   = dpvt->slot,
            .position = dpvt->addr,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_INRELAY_POINT, &inrlyp) < 0) {
            errlogPrintf("devBiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            return -1;
        }
        prec->rval = inrlyp.data;
    } else if (device == 'Y') { // Output relay on I/O modules
        const int32_t addr  = dpvt->addr;
        const int32_t start = ((addr - 1) / 16) * 16 + 1;
        const int32_t shift = ((addr - 1) % 16);
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = start,
            .count  = 1,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_OUTRELAY, &drly) < 0) {
            errlogPrintf("devBiF3RP61: %s : ioctl failed [%d]\n", prec->name, errno);
            recGblSetSevr(prec, READ_ALARM, INVALID_ALARM);
            return -1;
        }
        cdata = drly.u.outrly[0].data;
        cdata >>= shift;
        cdata &= 0x01;
        prec->rval = cdata;
    } else {
        //
    }

    //
    return 0;
}
