/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devSoF3RP61.c - Device Support Routines for F3RP61 String Output
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
#include <stringoutRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devSoF3RP61 */
static long init_record();
static long write_so();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_so;
    DEVSUPFUN  special_linconv;
} devSoF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_so,
    NULL
};

epicsExportAddress(dset, devSoF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must come first */
    M3IO_ACCESS_REG drly;
    char device;
} F3RP61_SO_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(stringoutRecord *pso)
{
    int unitno = 0, slotno = 0, start = 0;
    char device = 0;

    /* Link type must be INST_IO */
    if (pso->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pso,
                          "devSoF3RP61 (init_record) Illegal OUT field");
        pso->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pso->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse for possible interrupt source */
    char *pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devSoF3RP61: can't get interrupt source address for %s\n", pso->name);
            pso->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pso, unitno, slotno, start) < 0) {
            errlogPrintf("devSoF3RP61: can't register I/O interrupt for %s\n", pso->name);
            pso->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        errlogPrintf("devSoF3RP61: can't get I/O address for %s\n", pso->name);
        pso->pact = 1;
        return -1;
    }

    /* Allocate private data storage area */
    F3RP61_SO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SO_DPVT), "calloc failed");
    dpvt->device = device;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'A') {                         // Internal registers on I/O modules
        M3IO_ACCESS_REG *pdrly = &dpvt->drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        /*
          pdrly->u.pbdata = callocMustSucceed(40, sizeof(unsigned char), "calloc failed");
        */
        pdrly->u.pwdata = callocMustSucceed(40, sizeof(char),  "calloc failed");

        /*
          pdrly->count = (unsigned short) 40;
        */
        pdrly->count = 20;
    } else {
        errlogPrintf("devSoF3RP61: unsupported device \'%c\' for %s\n", device, pso->name);
        pso->pact = 1;
        return -1;
    }

    pso->dpvt = dpvt;

    return 0;
}

/*
  write_so() is called when there was a request to process a
  record. When called, it sends the value from the VAL field to the
  driver.
*/
static long write_so(stringoutRecord *pso)
{
    F3RP61_SO_DPVT *dpvt = pso->dpvt;
    M3IO_ACCESS_REG *pdrly = &dpvt->drly;

    strncpy((char *) pdrly->u.pbdata, (char *)&pso->val, 40);

    /* Issue API function */
    if (ioctl(f3rp61_fd, M3IO_WRITE_REG, pdrly) < 0) {
        errlogPrintf("devSoF3RP61: ioctl failed [%d] for %s\n", errno, pso->name);
        return -1;
    }

    pso->udf = FALSE;

    return 0;
}
