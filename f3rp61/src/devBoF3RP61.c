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
#include <errno.h>
#include <fcntl.h>
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
#include <boRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devBoF3RP61 */
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

typedef struct {
    IOSCANPVT ioscanpvt; /* must come first */
    M3IO_ACCESS_RELAY_POINT outrlyp;
    char device;
} F3RP61_BO_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(boRecord *pbo)
{
    int unitno = 0, slotno = 0, position = 0;
    char device = 0;

    /* Link type must be INST_IO */
    if (pbo->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pbo,
                          "devBoF3RP61 (init_record) Illegal OUT field");
        pbo->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pbo->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse for possible interrupt source */
    char *pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &position) < 3) {
            errlogPrintf("devBoF3RP61: can't get interrupt source address for %s\n", pbo->name);
            pbo->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pbo, unitno, slotno, position) < 0) {
            errlogPrintf("devBoF3RP61: can't register I/O interrupt for %s\n", pbo->name);
            pbo->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &position) < 4) {
        if (sscanf(buf, "%c%d", &device, &position) < 2) {
            errlogPrintf("devBoF3RP61: can't get I/O address for %s\n", pbo->name);
            pbo->pact = 1;
            return -1;
        } else if (device != 'E' && device != 'L') {
            errlogPrintf("devBoF3RP61: unsupported device \'%c\' for %s\n", device, pbo->name);
            pbo->pact = 1;
            return -1;
        }
    }

    /* Allocate private data storage area */
    F3RP61_BO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_BO_DPVT), "calloc failed");
    dpvt->device = device;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'E' || device == 'L') {        // Shared relays and Link relays
        M3IO_ACCESS_RELAY_POINT *poutrlyp = &dpvt->outrlyp;
        poutrlyp->position = position;
    } else if (device == 'Y') {                  // Output relays on I/O modules
        M3IO_ACCESS_RELAY_POINT *poutrlyp = &dpvt->outrlyp;
        poutrlyp->unitno = unitno;
        poutrlyp->slotno = slotno;
        poutrlyp->position = position;
    } else {
        errlogPrintf("devBoF3RP61: unsupported device \'%c\' for %s\n", device, pbo->name);
        pbo->pact = 1;
        return -1;
    }

    pbo->dpvt = dpvt;

    return 0;
}

/*
  write_bo() is called when there was a request to process a
  record. When called, it sends the value from the VAL field to the
  driver.
*/
static long write_bo(boRecord *pbo)
{
    F3RP61_BO_DPVT *dpvt = pbo->dpvt;
    M3IO_ACCESS_RELAY_POINT *poutrlyp = &dpvt->outrlyp;
    unsigned char data = pbo->rval;
    const char device = dpvt->device;

    /* Issue API function */
    if (device == 'E') {        // Shared relays
        if (writeM3ComRelayB(poutrlyp->position, 1, &data) < 0) {
            errlogPrintf("devBoF3RP61: writeM3ComRelayB failed [%d] for %s\n", errno, pbo->name);
            return -1;
        }
    } else if (device == 'L') { // Link relays
        if (writeM3LinkRelayB(poutrlyp->position, 1, &data) < 0) {
            errlogPrintf("devBoF3RP61: writeM3LinkRelayB failed [%d] for %s\n", errno, pbo->name);
            return -1;
        }
    } else {                    // Relays on I/O modules
        poutrlyp->data = data;
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY_POINT, poutrlyp) < 0) {
            errlogPrintf("devBoF3RP61: ioctl failed [%d] for %s\n", errno, pbo->name);
            return -1;
        }
    }

    pbo->udf = FALSE;

    return 0;
}
