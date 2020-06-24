/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAiF3RP61.c - Device Support Routines for F3RP61 Analog Input
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
#include <aiRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devAiF3RP61 */
static long init_record();
static long read_ai();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_ai;
    DEVSUPFUN  special_linconv;
} devAiF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    read_ai,
    NULL
};

epicsExportAddress(dset, devAiF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must comes first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    char option;
    int uword;
} F3RP61_AI_DPVT;

/* */
static long init_record(aiRecord *pai)
{
    int unitno, slotno, cpuno, start;
    char device;
    char option = 'W';
    int uword = 0;

    if (pai->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, pai,
                          "devAiF3RP61 (init_record) Illegal INP field");
        pai->pact = 1;
        return (S_db_badField);
    }

    struct link *plink = &pai->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devAiF3RP61: can't get option for %s\n", pai->name);
            pai->pact = 1;
            return (-1);
        }

        if (option == 'U') {
            uword = 1; /* uword flag is used for the possible future double option case */
        }
    }

    pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devAiF3RP61: can't get interrupt source address for %s\n", pai->name);
            pai->pact = 1;
            return (-1);
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pai, unitno, slotno, start) < 0) {
            errlogPrintf("devAiF3RP61: can't register I/O interrupt for %s\n", pai->name);
            pai->pact = 1;
            return (-1);
        }
    }

    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devAiF3RP61: can't get I/O address for %s\n", pai->name);
                pai->pact = 1;
                return (-1);
            }
            else if (device != 'W' && device != 'R') {
                errlogPrintf("devAiF3RP61: unsupported device \'%c\' for %s\n", device,
                             pai->name);
                pai->pact = 1;
            }
        }
        else {
            device = 'r';
        }
    }

    if (!(device == 'X' || device == 'Y' || device == 'A' ||device == 'r' ||
          device == 'W' || device == 'R')) {
        errlogPrintf("devAiF3RP61: illegal I/O address for %s\n", pai->name);
        pai->pact = 1;
        return (-1);
    }

    if (!(option == 'W' || option == 'L' || option == 'U' || option == 'F' || option == 'D')) {
        errlogPrintf("devAiF3RP61: illegal option for %s\n", pai->name);
        pai->pact = 1;
        return (-1);
    }

    F3RP61_AI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_AI_DPVT), "calloc failed");
    dpvt->device = device;
    dpvt->option = option;
    dpvt->uword = uword;

    if (device == 'r') {
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = (unsigned short) cpuno;
        pacom->start = (unsigned short) start;
        pacom->count = (unsigned short) 1;
    }
    else if (device == 'W' || device == 'R') {
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = (unsigned short) start;
        switch (option) {
        case 'D':
            pacom->count = 4;
            break;
        case 'F':
        case 'L':
            pacom->count = 2;
            break;
        default:
            pacom->count = 1;
        }
    }
    else {
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = (unsigned short) unitno;
        pdrly->slotno = (unsigned short) slotno;
        pdrly->start  = (unsigned short) start;
        pdrly->count  = (unsigned short) 1;
    }

    pai->dpvt = dpvt;

    return (0);
}

static long read_ai(aiRecord *pai)
{
    F3RP61_AI_DPVT *dpvt = pai->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    char option = dpvt->option;
    int uword = dpvt->uword;
    int command = M3IO_READ_REG;
    unsigned short wdata[4];
    unsigned long ldata;
    void *p = pdrly;

    switch (device) {
    case 'X':
        command = M3IO_READ_INRELAY;
        break;
    case 'Y':
        command = M3IO_READ_OUTRELAY;
        break;
    case 'r':
        command = M3IO_READ_COM;
        pacom->pdata = &wdata[0];
        p = pacom;
        break;
    case 'W':
    case 'R':
        break;
    default:
        switch (option) {
        case 'L':
            command = M3IO_READ_REG_L;
            pdrly->u.pldata = &ldata;
            break;
        default:
            pdrly->u.pwdata = &wdata[0];
        }
    }

    if (device != 'W' && device != 'R') {
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devAiF3RP61: ioctl failed [%d] for %s\n", errno, pai->name);
            return (-1);
        }
    }
    else if (device == 'W') {
        if (readM3LinkRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAiF3RP61: readM3LinkRegister failed [%d] for %s\n", errno, pai->name);
            return (-1);
        }
    }
    else {
        if (readM3ComRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAiF3RP61: readM3ComRegister failed [%d] for %s\n", errno, pai->name);
            return (-1);
        }
    }

    pai->udf = FALSE;

    switch (device) {
    case 'X':
        if (uword) {
            pai->rval = (long) pdrly->u.inrly[0].data;
        } else {
            pai->rval = (long) ((signed short) pdrly->u.inrly[0].data);
        }
        break;
    case 'Y':
        if (uword) {
            pai->rval = (long) pdrly->u.outrly[0].data;
        } else {
            pai->rval = (long) ((signed short) pdrly->u.outrly[0].data);
        }
        break;
    case 'r':
    case 'W':
    case 'R':
        if (uword) {
            pai->rval = (long) wdata[0];
        } else {
            switch (option) {
                float fval;
                unsigned char *p;
            case 'D':
                p = (unsigned char *) &pai->val;
                *p++ = (wdata[3] >> 8) & 0xff; *p++ = wdata[3] & 0xff;
                *p++ = (wdata[2] >> 8) & 0xff; *p++ = wdata[2] & 0xff;
                *p++ = (wdata[1] >> 8) & 0xff; *p++ = wdata[1] & 0xff;
                *p++ = (wdata[0] >> 8) & 0xff; *p++ = wdata[0] & 0xff;
                return (2);
            case 'F':
                p = (unsigned char *) &fval;
                *p++ = (wdata[1] >> 8) & 0xff; *p++ = wdata[1] & 0xff;
                *p++ = (wdata[0] >> 8) & 0xff; *p++ = wdata[0] & 0xff;
                pai->val = (double) fval;
                return (2);
            case 'L':
                pai->rval = (long) (((wdata[1] << 16) & 0xffff0000) | (wdata[0] & 0x0000ffff));
                break;
            default:
                pai->rval = (long) ((signed short) wdata[0]);
            }
        }
        break;
    default:
        switch (option) {
        case 'L':
            pai->rval = (long) ((signed long) ldata);
            break;
        default:
            if (uword) {
                pai->rval = (long) wdata[0];
            } else {
                pai->rval = (long) ((signed short) wdata[0]);
            }
        }
    }

    /* convert */
    return (0);
}
