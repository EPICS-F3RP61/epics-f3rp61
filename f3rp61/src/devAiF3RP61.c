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
    IOSCANPVT ioscanpvt; /* must come first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    char option;
} F3RP61_AI_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(aiRecord *pai)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;
    char option = 'W'; /* Dummy option for Word access */

    /* Link type must be INST_IO */
    if (pai->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, pai,
                          "devAiF3RP61 (init_record) Illegal INP field");
        pai->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pai->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse option */
    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devAiF3RP61: can't get option for %s\n", pai->name);
            pai->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'L') { // Long-word
        } else if (option == 'U') { // Unsigned integer
        } else if (option == 'F') { // Single precision floating point
        } else if (option == 'D') { // Double precision
        } else {                    // Option not recognized
            errlogPrintf("devAiF3RP61: unsupported option \'%c\' for %s\n", option, pai->name);
            pai->pact = 1;
            return -1;
        }
    }

    /* Parse for possible interrupt source */
    pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devAiF3RP61: can't get interrupt source address for %s\n", pai->name);
            pai->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pai, unitno, slotno, start) < 0) {
            errlogPrintf("devAiF3RP61: can't register I/O interrupt for %s\n", pai->name);
            pai->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devAiF3RP61: can't get I/O address for %s\n", pai->name);
                pai->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'R') {
                errlogPrintf("devAiF3RP61: unsupported device \'%c\' for %s\n", device, pai->name);
                pai->pact = 1;
            }
        } else {
            device = 'r';
        }
    }

    /* Allocate private data storage area */
    F3RP61_AI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_AI_DPVT), "calloc failed");
    dpvt->device = device;
    dpvt->option = option;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'r') {                         // Shared registers - Using 'Old' interface
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno;
        pacom->start = start;
        pacom->count = 1;
    } else if (device == 'R' || device == 'W') { // Shared registers and Link registers
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = start;
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
    } else if (device == 'X' || device == 'Y' || // Input and output relays on I/O modules
               device == 'A') {                  // Internal registers on I/O modules
        if (option != 'W') {
            errlogPrintf("devAiF3RP61: unsupported option \'%c\' for %s\n", option, pai->name);
            pai->pact = 1;
            return -1;
        }
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        pdrly->count  = 1;
    } else {
        errlogPrintf("devAiF3RP61: unsupported device \'%c\' for %s\n", device, pai->name);
        pai->pact = 1;
        return -1;
    }

    pai->dpvt = dpvt;

    return 0;
}

/*
  read_ai() is called when there was a request to process a
  record. When called, it reads the value from the driver and stores
  to the VAL field.
*/
static long read_ai(aiRecord *pai)
{
    F3RP61_AI_DPVT *dpvt = pai->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    char option = dpvt->option;
    int command = M3IO_READ_REG;
    unsigned short wdata[4] = {0};
    unsigned long ldata = 0;
    void *p = pdrly;

    /* Compose ioctl request */
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

    /* Issue API function */
    if (device == 'R') { // Shared registers
        if (readM3ComRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAiF3RP61: readM3ComRegister failed [%d] for %s\n", errno, pai->name);
            return -1;
        }
    } else if (device == 'W') { // Link registers
        if (readM3LinkRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAiF3RP61: readM3LinkRegister failed [%d] for %s\n", errno, pai->name);
            return -1;
        }
    } else { // Registers and relays on I/O modules
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devAiF3RP61: ioctl failed [%d] for %s\n", errno, pai->name);
            return -1;
        }
    }

    /* fill VAL field */
    pai->udf = FALSE;
    switch (device) {
    case 'X':
        if (option == 'U') {
            pai->rval = (long) pdrly->u.inrly[0].data;
        } else {
            pai->rval = (long) ((signed short) pdrly->u.inrly[0].data);
        }
        break;
    case 'Y':
        if (option == 'U') {
            pai->rval = (long) pdrly->u.outrly[0].data;
        } else {
            pai->rval = (long) ((signed short) pdrly->u.outrly[0].data);
        }
        break;
    case 'r':
    case 'W':
    case 'R':
        if (option == 'U') {
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
                return 2; // no conversion
            case 'F':
                p = (unsigned char *) &fval;
                *p++ = (wdata[1] >> 8) & 0xff; *p++ = wdata[1] & 0xff;
                *p++ = (wdata[0] >> 8) & 0xff; *p++ = wdata[0] & 0xff;
                pai->val = (double) fval;
                return 2; // no conversion
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
        case 'U':
            pai->rval = (long) wdata[0];
            break;
        default:
            pai->rval = (long) ((signed short) wdata[0]);
            break;
        }
    }

    return 0;
}
