/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devLoF3RP61.c - Device Support Routines for F3RP61 Long Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*
*      Modified: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
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
#include <longoutRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devLoF3RP61 */
static long init_record();
static long write_longout();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_longout;
    DEVSUPFUN  special_linconv;
} devLoF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_longout,
    NULL
};

epicsExportAddress(dset, devLoF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must comes first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    char option;
    int  lword; /* Long-word flag */
    int  bcd;   /* Binary Coded Decimal format flag */
} F3RP61_LO_DPVT;

/* */
static long init_record(longoutRecord *plongout)
{
    int unitno, slotno, cpuno, start;
    char device;
    char option = 'W'; /* 'W' as default option when nothing is passed */
    int lword = 0;
    int bcd = 0;

    /* bi.out must be an INST_IO */
    if (plongout->out.type != INST_IO) {
        recGblRecordError(S_db_badField, plongout,
                          "devLoF3RP61 (init_record) Illegal OUT field");
        plongout->pact = 1;
        return (S_db_badField);
    }

    struct link *plink = &plongout->out;
    int   size = strlen(plink->value.instio.string) + 1;  /* + 1 for appending the NULL character */
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse 'option' */
    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devLoF3RP61: can't get option for %s\n", plongout->name);
            plongout->pact = 1;
            return (-1);
        }

        if (option == 'L') {      /* Long-word flag */
            lword = 1;
        }
        else if (option == 'B') { /* Binary Coded Decimal format flag */
            bcd = 1;
        }
        else {                    /* Option not recognized */
            errlogPrintf("devLoF3RP61: illegal option for %s\n", plongout->name);
            plongout->pact = 1;
            return (-1);
        }
    }

    /* Parse for possible interrupt source */
    pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devLoF3RP61: can't get interrupt source address for %s\n", plongout->name);
            plongout->pact = 1;
            return (-1);
        }

        if (f3rp61_register_io_interrupt((dbCommon *) plongout, unitno, slotno, start) < 0) {
            errlogPrintf("devLoF3RP61: can't register I/O interrupt for %s\n", plongout->name);
            plongout->pact = 1;
            return (-1);
        }
    }

    /* Parse 'device' */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devLoF3RP61: can't get I/O address for %s\n", plongout->name);
                plongout->pact = 1;
                return (-1);
            }
            else if (device != 'W' && device != 'R') {
                errlogPrintf("devLoF3RP61: unsupported device \'%c\' for %s\n", device,
                             plongout->name);
                plongout->pact = 1;
            }
        }
        else {
            device = 'r';
        }
    }

    /* Check 'device' validity */
    if (!(device == 'Y' || device == 'A' || device == 'r' || device == 'W' ||
          device == 'R')) {
        errlogPrintf("devLoF3RP61: illegal I/O address for %s\n", plongout->name);
        plongout->pact = 1;
        return (-1);
    }

    /* Allocate private data storage area */
    F3RP61_LO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_LO_DPVT), "calloc failed");
    dpvt->device = device;
    dpvt->option = option;
    dpvt->lword  = lword;
    dpvt->bcd    = bcd;

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
        case 'L':
            pacom->count = 2;
            break;
        default:  /* Option either 'B' or 'W' (default when not passed) */
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

    plongout->dpvt = dpvt;

    return (0);
}

static long write_longout(longoutRecord *plongout)
{
    F3RP61_LO_DPVT *dpvt = plongout->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    /* char option = dpvt->option; */
    int lword   = dpvt->lword; /* Long-word flag */
    int bcd     = dpvt->bcd;   /* Binary Coded Decimal format flag */
    int command = M3IO_WRITE_REG;
    unsigned short wdata[2];
    unsigned long  ldata;
    unsigned short dataBCD = 0; /* For storing the value decoded from binary-coded-decimal format */
    void *p = pdrly;

    if (bcd) {
        /* Encode decimal to BCD */
        unsigned short i = 0;
        long data_temp = (long) plongout->val;
        /* Check data range */
        if (data_temp > 9999) {
            data_temp = 9999;
            recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
        }
        else if (data_temp < 0) {
            data_temp = 0;
            recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
        }

        while(data_temp > 0) {
            dataBCD = dataBCD | (((unsigned long) (data_temp % 10)) << (i*4));
            data_temp /= 10;
            i++;
        }
    }

    /* Set 'device' specific commands and get data */
    switch (device) {
    case 'Y':
        command = M3IO_WRITE_OUTRELAY;
        pdrly->u.outrly[0].data = (unsigned short) plongout->val;
        pdrly->u.outrly[0].mask = (unsigned short) 0xffff;
        break;
    case 'r':
        command = M3IO_WRITE_COM;
        if (bcd) {
            wdata[0] = dataBCD;
        }
        else {
            wdata[0] = (unsigned short) plongout->val;
        }
        pacom->pdata = &wdata[0];
        p =  pacom;
        break;
    case 'W':
    case 'R':
        if (lword) {
            wdata[0] = (unsigned short) (plongout->val & 0x0000ffff);
            wdata[1] = (unsigned short) ((plongout->val >> 16) & 0x0000ffff);
        }
        else if (bcd) {
            wdata[0] = dataBCD;
        }
        else {
            wdata[0] = (unsigned short) plongout->val;
        }
        break;
    default:  /* for device 'A' */
        if (lword) {
            command = M3IO_WRITE_REG_L;
            ldata = (unsigned long) plongout->val;
            pdrly->u.pldata = &ldata;
        }
        else if (bcd) {
            wdata[0] = dataBCD;
            pdrly->u.pwdata = &wdata[0];
        }
        else {
            wdata[0] = (unsigned short) plongout->val;
            pdrly->u.pwdata = &wdata[0];
        }
    }

    /* USE API FUNCTIONS TO WRITE TO DEVICE */
    /* For device 'r' and Registers and I/Os of specific modules (X,Y,A,..) */
    if (device != 'W' && device != 'R') {
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devLoF3RP61: ioctl failed [%d] for %s\n", errno, plongout->name);
            return (-1);
        }
    }

    /* For Link Register W */
    else if (device == 'W') {
        if (writeM3LinkRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devLoF3RP61: writeM3LinkRegister failed [%d] for %s\n", errno, plongout->name);
            return (-1);
        }
    }

    /* For Shared Register R */
    else {
        if (writeM3ComRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devLoF3RP61: writeM3ComRegister failed [%d] for %s\n", errno, plongout->name);
            return (-1);
        }
    }

    plongout->udf = FALSE;

    return (0);
}
