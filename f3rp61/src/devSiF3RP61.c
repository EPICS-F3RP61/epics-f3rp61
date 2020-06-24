/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
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

/* Create the dset for devSiF3RP61 */
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
    IOSCANPVT ioscanpvt; /* must comes first */
    M3IO_ACCESS_REG drly;
} F3RP61_SI_DPVT;

/* */
static long init_record(stringinRecord *psi)
{
    int unitno, slotno, start;
    char device;

    /* si.inp must be an INST_IO */
    if (psi->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, psi,
                          "devSiF3RP61 (init_record) Illegal INP field");
        psi->pact = 1;
        return (S_db_badField);
    }

    struct link *plink = &psi->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    char *pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devSiF3RP61: can't get interrupt source address for %s\n", psi->name);
            psi->pact = 1;
            return (-1);
        }

        if (f3rp61_register_io_interrupt((dbCommon *) psi, unitno, slotno, start) < 0) {
            errlogPrintf("devSiF3RP61: can't register I/O interrupt for %s\n", psi->name);
            psi->pact = 1;
            return (-1);
        }
    }

    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        errlogPrintf("devSiF3RP61: can't get I/O address for %s\n", psi->name);
        psi->pact = 1;
        return (-1);
    }

    if (!(device == 'A')) {
        errlogPrintf("devSiF3RP61: illegal I/O address for %s\n", psi->name);
        psi->pact = 1;
        return (-1);
    }

    F3RP61_SI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SI_DPVT), "calloc failed");

    M3IO_ACCESS_REG *pdrly = &dpvt->drly;
    pdrly->unitno = (unsigned short) unitno;
    pdrly->slotno = (unsigned short) slotno;
    pdrly->start  = (unsigned short) start;
    /*
      pdrly->u.pbdata = callocMustSucceed(40, sizeof(unsigned char), "calloc failed");
    */
    pdrly->u.pwdata = callocMustSucceed(40, sizeof(unsigned char), "calloc failed");
    /*
      pdrly->count = (unsigned short) 40;
    */
    pdrly->count = 20;

    psi->dpvt = dpvt;

    return (0);
}

static long read_si(stringinRecord *psi)
{
    F3RP61_SI_DPVT *dpvt = psi->dpvt;
    M3IO_ACCESS_REG *pdrly = &dpvt->drly;
    /*
      printf("devSiF3RP61: read_si() entered for %s\n", psi->name);
    */
    /*
      if (ioctl(f3rp61_fd, M3IO_READ_REG_B, pdrly) < 0) {
    */
    if (ioctl(f3rp61_fd, M3IO_READ_REG, pdrly) < 0) {
        errlogPrintf("devSiF3RP61: ioctl failed [%d] for %s\n", errno, psi->name);
        return (-1);
    }
    strncpy((char *) &psi->val, (char *) pdrly->u.pbdata, 40);

    psi->udf = FALSE;

    return (0);
}
