/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devSiF3RP61.c - Device Support Routines for F3RP61 String Input
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExport.h>
#include <errlog.h>
#include <recGbl.h>
#include <recSup.h>
#include <stringinRecord.h>

#include <drvF3RP61.h>

// Create the dset for devSiF3RP61
static long init_record();
static long read_si();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_si;
    DEVSUPFUN  special_linconv;
} devSiF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    read_si,
    NULL
};

epicsExportAddress(dset, devSiF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; // must come first
    M3IO_ACCESS_REG drly;
} F3RP61_SI_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configure
// values.
static long init_record(stringinRecord *precord)
{
    int unitno = 0, slotno = 0, start = 0;
    char device = 0;

    // Link type must be INST_IO
    if (precord->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devSiF3RP61 (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->inp;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for appending the NULL character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse for possible interrupt source
    char *pint = strchr(buf, ':'); // check if SCAN is interrupt based (example: @U0,S3,Y1:U0,S4,X1)
    if (pint) {
        *pint++ = '\0';
        if (sscanf(pint, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devSiF3RP61: can't get interrupt source address for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) precord, unitno, slotno, start) < 0) {
            errlogPrintf("devSiF3RP61: can't register I/O interrupt for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse slot, device and register number
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        errlogPrintf("devSiF3RP61: can't get I/O address for %s\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Allocate private data storage area
    F3RP61_SI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SI_DPVT), "calloc failed");

    // Check device validity and compose data structure for I/O request
    if (0) {                                     // dummy

    } else  if (device == 'A') {                 // I/O registers on special modules
        M3IO_ACCESS_REG *pdrly = &dpvt->drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        //pdrly->u.pbdata = callocMustSucceed(40, sizeof(unsigned char), "calloc failed");
        pdrly->u.pwdata = callocMustSucceed(40, sizeof(unsigned char), "calloc failed");
        //pdrly->count = 40;
        pdrly->count  = 20;
    } else {
        errlogPrintf("devSiF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// read_si() is called when there was a request to process a
// record. When called, it reads the value from the driver and stores
// to the VAL field.
static long read_si(stringinRecord *precord)
{
    F3RP61_SI_DPVT *dpvt = precord->dpvt;
    M3IO_ACCESS_REG *pdrly = &dpvt->drly;

    // Issue API function
    if (ioctl(f3rp61_fd, M3IO_READ_REG, pdrly) < 0) {
        errlogPrintf("devSiF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
        return -1;
    }

    //
    precord->udf = FALSE;

    // fill VAL field
    strncpy(precord->val, (char *) pdrly->u.pbdata, 40);

    return 0;
}
