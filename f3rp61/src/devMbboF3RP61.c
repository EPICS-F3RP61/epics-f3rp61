/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbboF3RP61.c - Device Support Routines for F3RP61 multi-bit binary
* Output
*
*      Author: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
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
#include <mbboRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devMbboF3RP61 */
static long init_record();
static long write_mbbo();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_mbbo;
    DEVSUPFUN  special_linconv;
}devMbboF3RP61={
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_mbbo,
    NULL
};

epicsExportAddress(dset, devMbboF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must come first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
} F3RP61_LO_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(mbboRecord *pmbbo)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;

    /* Link type must be INST_IO */
    if (pmbbo->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pmbbo,
                          "devMbboF3RP61 (init_record) Illegal OUT field");
        pmbbo->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pmbbo->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse for possible interrupt source */
    char *pC = strchr(buf, ':');  /* check if it is interrupt based (example: @U0,S3,Y1:U0,S4,X1) */
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devMbboF3RP61: can't get interrupt source address for %s\n", pmbbo->name);
            pmbbo->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) pmbbo, unitno, slotno, start) < 0) {
            errlogPrintf("devMbboF3RP61: can't register I/O interrupt for %s\n", pmbbo->name);
            pmbbo->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devMbboF3RP61: can't get I/O address for %s\n", pmbbo->name);
                pmbbo->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
                errlogPrintf("devMbboF3RP61: unsupported device \'%c\' for %s\n", device, pmbbo->name);
                pmbbo->pact = 1;
                return -1;
            }
        } else {
            device = 'r';
        }
    }

    /* Allocate private data storage area */
    F3RP61_LO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_LO_DPVT), "calloc failed");
    dpvt->device = device;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'r') {                         // Shared registers - Using 'Old' interface
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno;
        pacom->start = start;
        pacom->count = 1;
    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'E' || device == 'L') { // Shared relays and Link relays
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = start;
    } else if (device == 'Y' ||                  // Output relays on I/O modules
               device == 'A' || device == 'M') { // I/O registers on special modules and mode registers on I/O modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        pdrly->count  = 1;
    } else {
        errlogPrintf("devMbboF3RP61: unsupported device \'%c\' for %s\n", device, pmbbo->name);
        pmbbo->pact = 1;
        return -1;
    }

    pmbbo->dpvt = dpvt;

    return 0;
}

/*
  write_mbbo() is called when there was a request to process a
  record. When called, it sends the value from the VAL field to the
  driver.
*/
static long write_mbbo(mbboRecord *pmbbo)
{
    F3RP61_LO_DPVT *dpvt = pmbbo->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    int command = 0;
    unsigned short wdata = 0;
    void *p = pdrly;

    /* Compose ioctl request */
    switch (device) {
    case 'W':
    case 'R':
    case 'L':
    case 'E':
        wdata = (uint16_t)pmbbo->rval;
        break;
    case 'Y':
        command = M3IO_WRITE_OUTRELAY;
        pdrly->u.outrly[0].data = (uint16_t)pmbbo->rval;
        pdrly->u.outrly[0].mask = 0xFFFF;
        break;
    case 'r':
        command = M3IO_WRITE_COM;
        wdata = (unsigned short) pmbbo->rval;
        pacom->pdata = &wdata;
        p = pacom;
        break;
    case 'M':
        /* need to use old style */
        command = M3IO_WRITE_MODE;
        wdata = (unsigned short) pmbbo->rval;
        pdrly->u.wdata[0] = wdata;
        break;
    default:
        command = M3IO_WRITE_REG;
        wdata = (uint16_t)pmbbo->rval;
        pdrly->u.pwdata = &wdata;
    }

    /* Issue API function */
    if (device == 'R') {        // Shared registers
        if (writeM3ComRegister(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboF3RP61: writeM3ComRegister failed [%d] for %s\n", errno, pmbbo->name);
            return -1;
        }
    } else if (device == 'E') { // Shared relays
        if (writeM3ComRelay(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboF3RP61: writeM3ComRelay failed [%d] for %s\n", errno, pmbbo->name);
            return -1;
        }
    } else if (device == 'W') { // Link registers
        if (writeM3LinkRegister(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboF3RP61: writeM3LinkRegister failed [%d] for %s\n", errno, pmbbo->name);
            return -1;
        }
    } else if (device == 'L') { // Link relays
        if (writeM3LinkRelay(pacom->start, 1, &wdata) < 0) {
            errlogPrintf("devMbboF3RP61: writeM3LinkRelay failed [%d] for %s\n", errno, pmbbo->name);
            return -1;
        }
    } else {                    // Registers and relays on I/O modules
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devMbboF3RP61: ioctl failed [%d] for %s\n", errno, pmbbo->name);
            return -1;
        }
    }

    pmbbo->udf = FALSE;

    return 0;
}
