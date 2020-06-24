/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbboDirectF3RP61.c - Device Support Routines for F3RP61 multi-bit binary
* Output
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
#include <mbboDirectRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devMbboDirectF3RP61 */
static long init_record();
static long write_mbboDirect();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_mbboDirect;
    DEVSUPFUN  special_linconv;
} devMbboDirectF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_mbboDirect,
    NULL
};

epicsExportAddress(dset, devMbboDirectF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must comes first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
} F3RP61_LO_DPVT;

/* */
static long init_record(mbboDirectRecord *pmbboDirect)
{
    int unitno, slotno, cpuno, start;
    char device;

    /* bi.out must be an INST_IO */
    if (pmbboDirect->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pmbboDirect,
                          "devMbboDirectF3RP61 (init_record) Illegal OUT field");
        pmbboDirect->pact = 1;
        return (S_db_badField);
    }

    struct link *plink = &pmbboDirect->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    char *pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devMbboDirectF3RP61: can't get interrupt source address for %s\n", pmbboDirect->name);
            pmbboDirect->pact = 1;
            return (-1);
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pmbboDirect, unitno, slotno, start) < 0) {
            errlogPrintf("devMbboDirectF3RP61: can't register I/O interrupt for %s\n", pmbboDirect->name);
            pmbboDirect->pact = 1;
            return (-1);
        }
    }

    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devMbboDirectF3RP61: can't get I/O address for %s\n", pmbboDirect->name);
                pmbboDirect->pact = 1;
                return (-1);
            }
            else if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
                errlogPrintf("devMbboDirectF3RP61: unsupported device \'%c\' for %s\n", device,
                             pmbboDirect->name);
                pmbboDirect->pact = 1;
                return (-1);
            }
        }
        else {
            device = 'r';
        }
    }

    if (!(device == 'Y' || device == 'A' || device == 'r' || device == 'W' ||
          device == 'L' || device == 'M' || device == 'R' || device == 'E')) {
        errlogPrintf("devMbboDirectF3RP61: illegal I/O address for %s\n", pmbboDirect->name);
        pmbboDirect->pact = 1;
        return (-1);
    }

    F3RP61_LO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_LO_DPVT), "calloc failed");
    dpvt->device = device;

    if (device == 'r') {
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = (unsigned short) cpuno;
        pacom->start = (unsigned short) start;
        pacom->count = (unsigned short) 1;
    }
    else if (device == 'W' || device == 'L' || device == 'R' || device == 'E') {
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = (unsigned short) start;
    }
    else {
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = (unsigned short) unitno;
        pdrly->slotno = (unsigned short) slotno;
        pdrly->start  = (unsigned short) start;
        pdrly->count  = (unsigned short) 1;
    }

    pmbboDirect->dpvt = dpvt;

    return (0);
}

static long write_mbboDirect(mbboDirectRecord *pmbboDirect)
{
    F3RP61_LO_DPVT *dpvt = pmbboDirect->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    void *p = pdrly;
    unsigned short wdata;
    int command;

    switch (device) {
    case 'Y':
        command = M3IO_WRITE_OUTRELAY;
        pdrly->u.outrly[0].data = (unsigned short) pmbboDirect->rval;
        pdrly->u.outrly[0].mask = (unsigned short) 0xffff;
        break;
    case 'r':
        command = M3IO_WRITE_COM;
        wdata = (unsigned short) pmbboDirect->rval;
        pacom->pdata = &wdata;
        p = pacom;
        break;
    case 'W':
    case 'R':
    case 'L':
    case 'E':
        wdata = (unsigned short) pmbboDirect->rval;
        break;
    case 'M':
        /* need to use old style */
        command = M3IO_WRITE_MODE;
        wdata = (unsigned short) pmbboDirect->rval;
        pdrly->u.wdata[0] = wdata;
        break;
    default:
        command = M3IO_WRITE_REG;
        wdata = (unsigned short) pmbboDirect->rval;
        pdrly->u.pwdata = &wdata;
    }

    if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devMbboDirectF3RP61: ioctl failed [%d] for %s\n", errno, pmbboDirect->name);
            return (-1);
        }
    }
    else if (device == 'W') {
        if (writeM3LinkRegister((int) pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3LinkRegister failed [%d] for %s\n",
                         errno, pmbboDirect->name);
            return (-1);
        }
    }
    else if (device == 'R') {
        if (writeM3ComRegister((int) pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3ComRegister failed [%d] for %s\n",
                         errno, pmbboDirect->name);
            return (-1);
        }
    }
    else if (device == 'L') {
        if (writeM3LinkRelay((int) pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3LinkRelay failed [%d] for %s\n",
                         errno, pmbboDirect->name);
            return (-1);
        }
    }
    else {
        if (writeM3ComRelay((int) pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3ComRelay failed [%d] for %s\n",
                         errno, pmbboDirect->name);
            return (-1);
        }
    }

    pmbboDirect->udf = FALSE;

    return (0);
}
