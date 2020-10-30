/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
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
#include <biRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devBiF3RP61 */
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
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    read_bi
};

epicsExportAddress(dset, devBiF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must come first */
    union {
        M3IO_ACCESS_RELAY_POINT inrlyp;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    unsigned short start;
    unsigned short shift;
} F3RP61_BI_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(biRecord *pbi)
{
    int unitno = 0, slotno = 0, position = 0;
    char device = 0;

    /* Link type must be INST_IO */
    if (pbi->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, pbi,
                          "devBiF3RP61 (init_record) Illegal INP field");
        pbi->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pbi->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse for possible interrupt source */
    char *pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &position) < 3) {
            errlogPrintf("devBiF3RP61: can't get interrupt source address for %s\n", pbi->name);
            pbi->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pbi, unitno, slotno, position) < 0) {
            errlogPrintf("devBiF3RP61: can't register I/O interrupt for %s\n", pbi->name);
            pbi->pact = 1;
            return -1;
        }
    }

    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &position) < 4) {
        if (sscanf(buf, "%c%d", &device, &position) < 2) {
            errlogPrintf("devBiF3RP61: can't get I/O address for %s\n", pbi->name);
            pbi->pact = 1;
            return -1;
        } else if (device != 'L' && device != 'E') {
            errlogPrintf("devBiF3RP61: unsupported device \'%c\' for %s\n", device, pbi->name);
            pbi->pact = 1;
        }
    }

    /* Allocate private data storage area */
    F3RP61_BI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_BI_DPVT), "calloc failed");
    dpvt->device = device;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'E' || device == 'L') {        // Shared relays and Link relays
        M3IO_ACCESS_RELAY_POINT *pinrlyp = &dpvt->u.inrlyp;
        pinrlyp->position = position;
    } else if (device == 'X') {                  // Input relays on I/O modules
        M3IO_ACCESS_RELAY_POINT *pinrlyp = &dpvt->u.inrlyp;
        pinrlyp->unitno = unitno;
        pinrlyp->slotno = slotno;
        pinrlyp->position = position;
    } else if (device == 'Y') {                  // Output relays on I/O modules
        dpvt->start = ((position - 1) / 16) * 16 + 1;
        dpvt->shift = ((position - 1) % 16);
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start = dpvt->start;
        pdrly->count = 1;
    } else {
        errlogPrintf("devBiF3RP61: unsupported device \'%c\' for %s\n", device, pbi->name);
        pbi->pact = 1;
        return -1;
    }

    pbi->dpvt = dpvt;

    return 0;
}

/*
  read_bi() is called when there was a request to process a
  record. When called, it reads the value from the driver and stores
  to the VAL field.
*/
static long read_bi(biRecord *pbi)
{
    F3RP61_BI_DPVT *dpvt = pbi->dpvt;
    M3IO_ACCESS_RELAY_POINT *pinrlyp = &dpvt->u.inrlyp;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    int command = M3IO_READ_INRELAY_POINT;
    void *p = pinrlyp;
    unsigned char data = 0;

    /* Compose ioctl request */
    switch (device) {
    case 'Y':
        command = M3IO_READ_OUTRELAY;
        p = pdrly;
        break;
    //case 'L':
    //case 'E':
    default:
        break;
    }

    /* Issue API function */
    if (device == 'E') {        // Shared relays
        if (readM3ComRelayB(pinrlyp->position, 1, &data) < 0) {
            errlogPrintf("devBiF3RP61: readM3ComRelayB failed [%d] for %s\n", errno, pbi->name);
            return -1;
        }
    } else if (device == 'L') { // Link realys
        if (readM3LinkRelayB(pinrlyp->position, 1, &data) < 0) {
            errlogPrintf("devBiF3RP61: readM3LinkRelayB failed [%d] for %s\n", errno, pbi->name);
            return -1;
        }
    } else {                    // Relays on I/O modules
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devBiF3RP61: ioctl failed [%d] for %s\n", errno, pbi->name);
            return -1;
        }
    }

    /* fill VAL field */
    pbi->udf = FALSE;
    switch (device) {
    case 'Y':
        pbi->rval = (pdrly->u.outrly[0].data >> dpvt->shift) & 0x1;
        break;
    case 'L':
    case 'E':
        pbi->rval = data;
        break;
    default:
        pbi->rval = pinrlyp->data;
    }

    return 0;
}
