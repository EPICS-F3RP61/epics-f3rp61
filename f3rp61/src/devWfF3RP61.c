/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devWfF3RP61.c - Device Support Routines for F3RP61 Analog Input
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
#include <waveformRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devWfF3RP61 */
static long init_record();
static long read_wf();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_wf;
    DEVSUPFUN  special_linconv;
} devWfF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    read_wf,
    NULL
};

epicsExportAddress(dset, devWfF3RP61);

extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

typedef struct {
    IOSCANPVT ioscanpvt; /* must come first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    void *pdata;
} F3RP61_WF_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(waveformRecord *pwf)
{
    const int ftvl = pwf->ftvl;
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;
    //char option = 'W'; // option is no implemented for waveform Record

    /* Link type must be INST_IO */
    if (pwf->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, pwf,
                          "devWfF3RP61 (init_record) Illegal INP field");
        pwf->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pwf->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse for possible interrupt source */
    char *pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devWfF3RP61: can't get interrupt source address for %s\n", pwf->name);
            pwf->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pwf, unitno, slotno, start) < 0) {
            errlogPrintf("devWfF3RP61: can't register I/O interrupt for %s\n", pwf->name);
            pwf->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devWfF3RP61: can't get I/O address for %s\n", pwf->name);
                pwf->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'R') {
                errlogPrintf("devWfF3RP61: unsupported device \'%c\' for %s\n", device, pwf->name);
                pwf->pact = 1;
            }
        } else {
            device = 'r';
        }
    }

    //if (!(option == 'W' || option == 'L' || option == 'U' || option == 'F' || option == 'D')) {
    //    errlogPrintf("devWfF3RP61: unsupported option \'%c\' for %s\n", option, pwf->name);
    //    pwf->pact = 1;
    //    return -1;
    //}

    /* Allocate private data storage area */
    F3RP61_WF_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_WF_DPVT), "calloc failed");
    dpvt->device = device;

    void *pdata = callocMustSucceed(pwf->nelm, dbValueSize(ftvl), "calloc failed");
    dpvt->pdata = pdata;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'r') { /* Shared registers - Using 'Old' interface */
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = (unsigned short) cpuno;
        pacom->start = (unsigned short) start;
        pacom->count = (unsigned short) (pwf->nelm * 1);
    } else if (device == 'W' || device == 'R') { /* Shared registers and Link registers */
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = (unsigned short) start;
        switch (ftvl) {
        case DBF_DOUBLE:
            pacom->count = (unsigned short) (pwf->nelm * 4);
            break;
        case DBF_FLOAT:
        case DBF_ULONG:
        case DBF_LONG:
            pacom->count = (unsigned short) (pwf->nelm * 2);
            break;
        default:
            pacom->count = (unsigned short) (pwf->nelm * 1);
        }
    } else if (device =='A') { /* I/O Registers on special modules */
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = (unsigned short) unitno;
        pdrly->slotno = (unsigned short) slotno;
        pdrly->start  = (unsigned short) start;
        pdrly->count  = (unsigned short) (pwf->nelm * 1);
    } else {
        errlogPrintf("devWfF3RP61: unsupported device \'%c\' for %s\n", device, pwf->name);
        pwf->pact = 1;
        return -1;
    }

    pwf->dpvt = dpvt;

    return 0;
}

/*
  read_wf() is called when there was a request to process a
  record. When called, it reads the value from the driver and stores
  to the VAL field.
*/
static long read_wf(waveformRecord *pwf)
{
    F3RP61_WF_DPVT *dpvt = pwf->dpvt;
    const int ftvl = pwf->ftvl;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    int command = M3IO_READ_REG;
    unsigned short *pwdata = dpvt->pdata;
    unsigned long *pldata = dpvt->pdata;
    void *p = pdrly;

    /* Compose ioctl request */
    switch (device) {
    case 'r':
        command = M3IO_READ_COM;
        pacom->pdata = pwdata;
        p = pacom;
        break;
    case 'W':
    case 'R':
        break;
    default:
        switch (ftvl) {
        case DBF_ULONG:
            command = M3IO_READ_REG_L;
            pdrly->u.pldata = pldata;
            break;
        case DBF_USHORT:
        case DBF_SHORT:
            pdrly->u.pwdata = pwdata;
            break;
        default:
            errlogPrintf("devWfF3RP61: unsupported field type of value for %s\n", pwf->name);
            pwf->pact = 1;
            return -1;
        }
    }

    /* Issue API function */
    if (device == 'R') { /* Shared registers */
        if (readM3ComRegister((int) pacom->start, pacom->count, pwdata) < 0) {
            errlogPrintf("devWfF3RP61: readM3ComRegister failed [%d] for %s\n", errno, pwf->name);
            return -1;
        }
    } else if (device == 'W') { /* Link registers */
        if (readM3LinkRegister((int) pacom->start, pacom->count, pwdata) < 0) {
            errlogPrintf("devWfF3RP61: readM3LinkRegister failed [%d] for %s\n", errno, pwf->name);
            return -1;
        }
    } else {
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devWfF3RP61: ioctl failed [%d] for %s\n", errno, pwf->name);
            return -1;
        }
    }

    /* fill VAL field */
    pwf->udf = FALSE;
    switch (device) {
        unsigned char  *p1;
        unsigned long  *p2;
        unsigned short *p3;
    case 'r':
    case 'W':
    case 'R':
        switch (ftvl) {
        case DBF_DOUBLE:
            p1 = pwf->bptr;
            for (int i = 0; i < pwf->nelm; i++) {
                *p1++ = (pwdata[3 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[3 + (4 * i)] & 0xff;
                *p1++ = (pwdata[2 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[2 + (4 * i)] & 0xff;
                *p1++ = (pwdata[1 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[1 + (4 * i)] & 0xff;
                *p1++ = (pwdata[0 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[0 + (4 * i)] & 0xff;
            }
            break;
        case DBF_FLOAT:
            p1 = pwf->bptr;
            for (int i = 0; i < pwf->nelm; i++) {
                *p1++ = (pwdata[1 + (2 * i)] >> 8) & 0xff; *p1++ = pwdata[1 + (2 * i)] & 0xff;
                *p1++ = (pwdata[0 + (2 * i)] >> 8) & 0xff; *p1++ = pwdata[0 + (2 * i)] & 0xff;
            }
            break;
        case DBF_ULONG:
        case DBF_LONG:
            p2 = pwf->bptr;
            for (int i = 0; i < pwf->nelm; i++) {
                p2[i] = ((pwdata[1 + (2 *i)] << 16) & 0xffff0000)  |  (pwdata[0 + (2 * i)] & 0x0000ffff);
            }
            break;
        case DBF_USHORT:
        case DBF_SHORT:
            p3 = pwf->bptr;
            for (int i = 0; i < pwf->nelm; i++) {
                p3[i] = pwdata[0 + (1 * i)];
            }
            break;
        default:
            errlogPrintf("%s:unsupported field type of value\n", pwf->name);
            pwf->pact = 1;
            return -1;
        }
        break;
    default:
        switch (ftvl) {
        case DBF_ULONG:
            p2 = pwf->bptr;
            for (int i = 0; i < pwf->nelm; i++) {
                p2[i] = pldata[i];
            }
            break;
        case DBF_USHORT:
        case DBF_SHORT:
            p3 = pwf->bptr;
            for (int i = 0; i < pwf->nelm; i++) {
                p3[i] = pwdata[i];
            }
            break;
        default:
            errlogPrintf("%s:unsupported field type of value\n", pwf->name);
            pwf->pact = 1;
            return -1;
        }
    }

    pwf->nord = pwf->nelm;

    return 0;
}
