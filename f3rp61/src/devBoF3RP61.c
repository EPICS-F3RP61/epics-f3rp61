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
#include <boRecord.h>

#include <drvF3RP61.h>

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

typedef struct {
    IOSCANPVT ioscanpvt; // must come first
    M3IO_ACCESS_RELAY_POINT outrlyp;
    char device;
} F3RP61_BO_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(boRecord *precord)
{
    int unitno = 0, slotno = 0, position = 0;
    char device = 0;

    // Link type must be INST_IO
    if (precord->out.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devBoF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->out;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse option
    //char *popt = strchr(buf, '&');
    //if (popt) {
    //    char option = 'W'; // Dummy option for Word access
    //    *popt++ = '\0';
    //    if (sscanf(popt, "%c", &option) < 1) {
    //        errlogPrintf("devBoF3RP61: can't get option for %s\n", precord->name);
    //        precord->pact = 1;
    //        return -1;
    //    }
    //    if (1) {                    // Option not recognized
    //        errlogPrintf("devBoF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
    //        precord->pact = 1;
    //       return -1;
    //   }
    //}

    // Parse for possible interrupt source
    char *pint = strchr(buf, ':'); // check if SCAN is interrupt based (example: @U0,S3,Y1:U0,S4,X1)
    if (pint) {
        *pint++ = '\0';
        if (sscanf(pint, "U%d,S%d,X%d", &unitno, &slotno, &position) < 3) {
            errlogPrintf("devBoF3RP61: can't get interrupt source address for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) precord, unitno, slotno, position) < 0) {
            errlogPrintf("devBoF3RP61: can't register I/O interrupt for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse slot, device and relay number
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &position) < 4) {
        if (sscanf(buf, "%c%d", &device, &position) < 2) {
            errlogPrintf("devBoF3RP61: can't get I/O address for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        } else if (device != 'E' && device != 'L') {
            errlogPrintf("devBoF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Allocate private data storage area
    F3RP61_BO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_BO_DPVT), "calloc failed");
    dpvt->device = device;

    // Check device validity and compose data structure for I/O request
    if (0) {                                     // dummy

    } else if (device == 'E' || device == 'L') { // Shared relays and Link relays
        M3IO_ACCESS_RELAY_POINT *poutrlyp = &dpvt->outrlyp;
        poutrlyp->position = position;

    } else if (device == 'Y') {                  // Output relays on I/O modules
        M3IO_ACCESS_RELAY_POINT *poutrlyp = &dpvt->outrlyp;
        poutrlyp->unitno = unitno;
        poutrlyp->slotno = slotno;
        poutrlyp->position = position;

    } else {
        errlogPrintf("devBoF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// write_bo() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_bo(boRecord *precord)
{
    F3RP61_BO_DPVT *dpvt = precord->dpvt;
    M3IO_ACCESS_RELAY_POINT *poutrlyp = &dpvt->outrlyp;
    const char device = dpvt->device;

    // Compose data to write
    uint8_t cdata = precord->rval;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'E') { // Shared relays
        if (writeM3ComRelayB(poutrlyp->position, 1, &cdata) < 0) {
            errlogPrintf("devBoF3RP61: writeM3ComRelayB failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        if (writeM3LinkRelayB(poutrlyp->position, 1, &cdata) < 0) {
            errlogPrintf("devBoF3RP61: writeM3LinkRelayB failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else {//(device == 'Y')   // Relays on I/O modules
        poutrlyp->data = cdata;
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY_POINT, poutrlyp) < 0) {
            errlogPrintf("devBoF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
    }

    //
    precord->udf = FALSE;

    return 0;
}
