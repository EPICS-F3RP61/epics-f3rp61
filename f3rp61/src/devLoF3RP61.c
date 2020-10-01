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
    IOSCANPVT ioscanpvt; /* must come first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    char option;
} F3RP61_LO_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(longoutRecord *plongout)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    /* Link type must be INST_IO */
    if (plongout->out.type != INST_IO) {
        recGblRecordError(S_db_badField, plongout,
                          "devLoF3RP61 (init_record) Illegal OUT field");
        plongout->pact = 1;
        return S_db_badField;
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
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'L') { // Long word
        } else if (option == 'U') { // Unsigned integer, perhaps we'd better disable this
        } else if (option == 'B') { // Binary Coded Decimal format
        } else {                    // Option not recognized
            errlogPrintf("devLoF3RP61: unsupported option \'%c\' for %s\n", option, plongout->name);
            plongout->pact = 1;
            return -1;
        }
    }

    /* Parse for possible interrupt source */
    pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devLoF3RP61: can't get interrupt source address for %s\n", plongout->name);
            plongout->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) plongout, unitno, slotno, start) < 0) {
            errlogPrintf("devLoF3RP61: can't register I/O interrupt for %s\n", plongout->name);
            plongout->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devLoF3RP61: can't get I/O address for %s\n", plongout->name);
                plongout->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'R') {
                errlogPrintf("devLoF3RP61: unsupported device \'%c\' for %s\n", device, plongout->name);
                plongout->pact = 1;
                return -1;
            }
        } else {
            device = 'r';
        }
    }

    /* Allocate private data storage area */
    F3RP61_LO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_LO_DPVT), "calloc failed");
    dpvt->device = device;
    dpvt->option = option;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'r') {                         // Shared registers - Using 'Old' interface
        //if (option == 'L') {
        //    errlogPrintf("devAiF3RP61: unsupported option \'%c\' for %s\n", option, pai->name);
        //    pai->pact = 1;
        //    return -1;
        //}
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno;
        pacom->start = start;
        pacom->count = 1; // we don't have '&L' option support yet.
    } else if (device == 'R' || device == 'W') { // Shared registers and Link registers
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = start;
        if (option == 'L') {
            pacom->count = 2;
        } else {
            pacom->count = 1;
        }
    } else if (device == 'A') {                   // I/O registers on I/O modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        if (option == 'L') {
            pdrly->count = 1; // we use M3IO_READ_REG_L therefore count shall be 1
        } else {
            pdrly->count = 1;
        }
    } else if (device == 'Y') {                  // Output relays on I/O modules
        if (option == 'B') {
            errlogPrintf("devLoF3RP61: unsupported option \'%c\' for %s\n", option, plongout->name);
            plongout->pact = 1;
            return -1;
        }
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        if (option == 'L') {
            pdrly->count = 2;
        } else {
            pdrly->count = 1;
        }
    } else {
        errlogPrintf("devLoF3RP61: unsupported device \'%c\' for %s\n", device, plongout->name);
        plongout->pact = 1;
        return -1;
    }

    plongout->dpvt = dpvt;

    return 0;
}

/*
  write_longout() is called when there was a request to process a
  record. When called, it sends the value from the VAL field to the
  driver.
*/
static long write_longout(longoutRecord *plongout)
{
    F3RP61_LO_DPVT *dpvt = plongout->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    char option = dpvt->option;
    int command = M3IO_WRITE_REG;
    uint16_t wdata[2] = {0};
    ulong    ldata = 0;
    unsigned short dataBCD = 0; /* For storing the value decoded from binary-coded-decimal format */
    void *p = pdrly;

    if (option == 'B') {
        /* Encode decimal to BCD */
        unsigned short i = 0;
        long data_temp = (long) plongout->val;
        /* Check data range */
        if (data_temp > 9999) {
            data_temp = 9999;
            recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
        } else if (data_temp < 0) {
            data_temp = 0;
            recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
        }

        while(data_temp > 0) {
            dataBCD = dataBCD | (((unsigned long) (data_temp % 10)) << (i*4));
            data_temp /= 10;
            i++;
        }
    }

    /* Compose ioctl request */
    switch (device) {
    case 'W':
    case 'R':
        if (option == 'B') {
            wdata[0] = dataBCD;
        } else if (option == 'L') {
            wdata[0] = (uint16_t)(plongout->val>> 0);
            wdata[1] = (uint16_t)(plongout->val>>16);
        } else {
            wdata[0] = (uint16_t)plongout->val;
        }
        break;
    case 'Y':
        command = M3IO_WRITE_OUTRELAY;
        if (option == 'L') {
            pdrly->u.outrly[0].data = (uint16_t)(plongout->val>> 0);
            pdrly->u.outrly[0].mask = 0xFFFF;
            pdrly->u.outrly[1].data = (uint16_t)(plongout->val>>16);
            pdrly->u.outrly[1].mask = 0xFFFF;
        } else {
            pdrly->u.outrly[0].data = (uint16_t)plongout->val;
            pdrly->u.outrly[0].mask = 0xFFFF;
        }
        break;
    case 'r':
        command = M3IO_WRITE_COM;
        if (option == 'B') {
            wdata[0] = dataBCD;
        } else {
            wdata[0] = (unsigned short) plongout->val;
        }
        pacom->pdata = &wdata[0];
        p =  pacom;
        break;
    default:  /* for device 'A' */
        if (option == 'B') {
            wdata[0] = dataBCD;
            pdrly->u.pwdata = &wdata[0];
        } else if (option == 'L') {
            command = M3IO_WRITE_REG_L;
            ldata = (ulong)plongout->val;
            pdrly->u.pldata = &ldata;
        } else {
            wdata[0] = (uint16_t)plongout->val;
            pdrly->u.pwdata = &wdata[0];
        }
    }

    /* Issue API function */
    if (device == 'R') {        // Shared registers
        if (writeM3ComRegister(pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devLoF3RP61: writeM3ComRegister failed [%d] for %s\n", errno, plongout->name);
            return -1;
        }
    } else if (device == 'W') { // Link registers
        if (writeM3LinkRegister(pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devLoF3RP61: writeM3LinkRegister failed [%d] for %s\n", errno, plongout->name);
            return -1;
        }
    } else {                    // Registers and relays on I/O modules
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devLoF3RP61: ioctl failed [%d] for %s\n", errno, plongout->name);
            return -1;
        }
    }

    plongout->udf = FALSE;

    return 0;
}
