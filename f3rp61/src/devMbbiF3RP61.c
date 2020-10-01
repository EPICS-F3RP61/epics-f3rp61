/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiF3RP61.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
*
*      Author: Gregor Kostevc (Cosylab)
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
#include <mbbiRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devMbbiF3RP61 */
static long init_record();
static long read_mbbi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_mbbi;
    DEVSUPFUN  special_linconv;
} devMbbiF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    read_mbbi,
    NULL
};

epicsExportAddress(dset, devMbbiF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must come first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
} F3RP61_MBBI_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(mbbiRecord *pmbbi)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;

    /* Link type must be INST_IO */
    if (pmbbi->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, pmbbi,
                          "devMbbiF3RP61 (init_record) Illegal INP field");
        pmbbi->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pmbbi->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse for possible interrupt source */
    char *pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devMbbiF3RP61: can't get interrupt source address for %s\n", pmbbi->name);
            pmbbi->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pmbbi, unitno, slotno, start) < 0) {
            errlogPrintf("devMbbiF3RP61: can't register I/O interrupt for %s\n", pmbbi->name);
            pmbbi->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devMbbiF3RP61: can't get I/O address for %s\n", pmbbi->name);
                pmbbi->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
                errlogPrintf("devMbbiF3RP61: unsupported device \'%c\' for %s\n", device, pmbbi->name);
                pmbbi->pact = 1;
                return -1;
            }
        } else {
            device = 'r';
        }
    }

    /* Allocate private data storage area */
    F3RP61_MBBI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_MBBI_DPVT), "calloc failed");
    dpvt->device = device;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'r') { // Shared registers and Link registers
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno;
        pacom->start = start;
        pacom->count = 1;
    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'E' || device == 'L') { // Shared relays and Link relays
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = start;
    } else if (device == 'X' || device == 'Y' || // Input and output relays on I/O modules
               device == 'A' || device == 'M') { // Internal registers and mode registers on I/O modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        pdrly->count  = 1;
    } else {
        errlogPrintf("devMbbiF3RP61: unsupported device \'%c\' for %s\n", device, pmbbi->name);
        pmbbi->pact = 1;
        return -1;
    }

    pmbbi->dpvt = dpvt;

    return 0;
}

/*
  read_mbbi() is called when there was a request to process a
  record. When called, it reads the value from the driver and stores
  to the VAL field.
*/
static long read_mbbi(mbbiRecord *pmbbi)
{
    F3RP61_MBBI_DPVT *dpvt = pmbbi->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    int command = -1;
    unsigned short wdata;
    void *p = pdrly;

    /* Compose ioctl request */
    switch (device) {
    case 'R':
    case 'W':
    case 'E':
    case 'L':
        break;
    case 'X':
        command = M3IO_READ_INRELAY;
        break;
    case 'Y':
        command = M3IO_READ_OUTRELAY;
        break;
    case 'r':
        command = M3IO_READ_COM;
        pacom->pdata = &wdata;
        p =  pacom;
        break;
    case 'M':
        /* need to use old style */
        command = M3IO_READ_MODE;
        break;
    default:
        command = M3IO_READ_REG;
        pdrly->u.pwdata = &wdata;
    }

    /* Issue API function */
    if (device == 'R') {        // Shared registers
        if (readM3ComRegister(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: readM3ComRegister failed [%d] for %s\n", errno, pmbbi->name);
            return -1;
        }
    } else if (device == 'W') { // Link registers
        if (readM3LinkRegister(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: readM3LinkRegister failed [%d] for %s\n", errno, pmbbi->name);
            return -1;
        }
    } else if (device == 'E') { // Shared relay
        if (readM3ComRelay(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: readM3ComRelay failed [%d] for %s\n", errno, pmbbi->name);
            return -1;
        }
    } else if (device == 'L') { // Link relay
        if (readM3LinkRelay(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbbiF3RP61: readM3LinkRelay failed [%d] for %s\n", errno, pmbbi->name);
            return -1;
        }
    } else {                    // Registers and relays on I/O modules
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devMbbiF3RP61: ioctl failed [%d] for %s\n", errno, pmbbi->name);
            return -1;
        }
    }

    /* fill VAL field */
    pmbbi->udf = FALSE;
    switch (device) {
    case 'X':
        pmbbi->rval = pdrly->u.inrly[0].data;
        break;
    case 'Y':
        pmbbi->rval = pdrly->u.outrly[0].data;
        break;
    case 'M':
        /* need to use old style */
        pmbbi->rval = (long) pdrly->u.wdata[0];
        break;
    case 'r':
    case 'W':
    case 'R':
    case 'L':
    case 'E':
    default:
        pmbbi->rval = wdata;
    }

    return 0;
}
