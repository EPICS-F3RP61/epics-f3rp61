/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAoF3RP61.c - Device Support Routines for F3RP61 Analog Output
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
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
#include <math.h>

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
#include <aoRecord.h>

#include <drvF3RP61.h>

// Create the dset for devAoF3RP61
static long init_record();
static long write_ao();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_ao;
    DEVSUPFUN  special_linconv;
} devAoF3RP61 = {
    6,
    NULL,
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_ao,
    NULL
};

epicsExportAddress(dset, devAoF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; // must come first
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    char option;
} F3RP61_AO_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aoRecord *precord)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    // Link type must be INST_IO
    if (precord->out.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devAoF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->out;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse option
    char *popt = strchr(buf, '&');
    if (popt) {
        *popt++ = '\0';
        if (sscanf(popt, "%c", &option) < 1) {
            errlogPrintf("devAoF3RP61: can't get option for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'U') { // Unsigned integer, perhaps we'd better disable this
        } else if (option == 'L') { // Long word
        } else if (option == 'F') { // Single precision floating point
        } else if (option == 'D') { // Double precision floating point
        } else {                    // Option not recognized
            errlogPrintf("devAoF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse for possible interrupt source
    char *pint = strchr(buf, ':'); // check if SCAN is interrupt based (example: @U0,S3,Y1:U0,S4,X1)
    if (pint) {
        *pint++ = '\0';
        if (sscanf(pint, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devAoF3RP61: can't get interrupt source address for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
        if (f3rp61_register_io_interrupt((dbCommon *) precord, unitno, slotno, start) < 0) {
            errlogPrintf("devAoF3RP61: can't register I/O interrupt for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse slot, device and register number
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devAoF3RP61: can't get I/O address for %s\n", precord->name);
                precord->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'R') {
                errlogPrintf("devAoF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
                precord->pact = 1;
            }
        } else {
            device = 'r'; // Shared memory (or 'Old interface' for shared registers/relays)
        }
    }

    // Allocate private data storage area
    F3RP61_AO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_AO_DPVT), "calloc failed");
    dpvt->device = device;
    dpvt->option = option;

    // Consider I/O data length
    int count = 1;
    if (option == 'F' || option == 'L') {
        count = 2; // count for 'A' shold be one, which will be handled later
    } else if (option == 'D') {
        count = 4; // count for 'A' shold be two, which will be handled later
    }

    // Check device validity and compose data structure for I/O request
    if (0) {                                     // dummy

    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'r') {                  // Shared registers - Using 'Old' interface
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno; // for 'r' devices
        pacom->start = start;
        pacom->count = count;

    } else if (device == 'Y') {                  // Output relays on I/O modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        pdrly->count  = count;

    } else if (device == 'A') {                  // I/O registers on special modules
        // 'D' and 'F' option might not make sence for device 'A'
        //if (option != 'W') { // || option != 'U' || option != 'L'
        //    errlogPrintf("devAoF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
        //    precord->pact = 1;
        //    return -1;
        //}
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        if (option == 'L' || option == 'F' || option == 'D') {
            pdrly->count  = count/2; // we use M3IO_WRITE_REG_L
        } else {
            pdrly->count  = count;
        }

    } else {
        errlogPrintf("devAoF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// write_ao() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_ao(aoRecord *precord)
{
    F3RP61_AO_DPVT *dpvt = precord->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    const char device = dpvt->device;
    const char option = dpvt->option;

    // Compose data to write
    uint16_t wdata[4] = {0};
    uint16_t mask[4]  = {0xffff, 0xffff, 0xffff, 0xffff};
    ulong    ldata[2] = {0};
    if (option == 'D') {
        double val = precord->val;
        // todo : consider ASLO and AOFF field

        int64_t lval;
        memcpy(&lval, &val, sizeof(double));

        // for (device == 'A')
        ldata[0] = (uint32_t)(lval>> 0);
        ldata[1] = (uint32_t)(lval>>32);

        // for (device != 'A')
        wdata[0] = (uint16_t)(lval>> 0);
        wdata[1] = (uint16_t)(lval>>16);
        wdata[2] = (uint16_t)(lval>>32);
        wdata[3] = (uint16_t)(lval>>48);
    } else if (option == 'F') {
        float val = precord->val;
        // todo : consider ASLO and AOFF field

        int32_t lval;
        memcpy(&lval, &val, sizeof(float));

        // for (device == 'A')
        ldata[0] = (uint32_t)lval;

        // for (device != 'A')
        wdata[0] = (uint16_t)(lval>> 0);
        wdata[1] = (uint16_t)(lval>>16);
    } else if (option == 'L') {
        // for (device == 'A')
        ldata[0] = (uint32_t)precord->rval;

        // for (device != 'A')
        wdata[0] = (uint16_t)(precord->rval>> 0);
        wdata[1] = (uint16_t)(precord->rval>>16);
    } else {
        wdata[0] = (uint16_t)precord->rval;
    }

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        if (writeM3ComRegister(pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAoF3RP61: writeM3ComRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        if (writeM3LinkRegister(pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAoF3RP61: writeM3LinkRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'r') { // Shared memory
        pacom->pdata = wdata;
        if (ioctl(f3rp61_fd, M3IO_WRITE_COM, pacom) < 0) {
            errlogPrintf("devAoF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'Y') { // Output relays on I/O modules
        pdrly->u.outrly[0].data = wdata[0];
        pdrly->u.outrly[0].mask = mask[0];
        if (option == 'L' || option == 'F') { // count == 2
            pdrly->u.outrly[1].data = wdata[1];
            pdrly->u.outrly[1].mask = mask[1];

        }
        if (option == 'D') { // count == 4
            pdrly->u.outrly[1].data = wdata[1];
            pdrly->u.outrly[1].mask = mask[1];
            pdrly->u.outrly[2].data = wdata[2];
            pdrly->u.outrly[2].mask = mask[2];
            pdrly->u.outrly[3].data = wdata[3];
            pdrly->u.outrly[3].mask = mask[3];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY, pdrly) < 0) {
            errlogPrintf("devAoF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
    } else {//(device == 'A')   // I/O registers on special modules
        if (option == 'L' || option == 'F' || option == 'D') { // count == 2 || count == 4
            pdrly->u.pldata = ldata;
            if (ioctl(f3rp61_fd, M3IO_WRITE_REG_L, pdrly) < 0) {
                errlogPrintf("devAoF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
                return -1;
            }
        } else {
            pdrly->u.pwdata = wdata;
            if (ioctl(f3rp61_fd, M3IO_WRITE_REG, pdrly) < 0) {
                errlogPrintf("devAoF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
                return -1;
            }
        }
    }

    //
    precord->udf = FALSE;
    if (option == 'D' || option == 'F') {
        precord->udf = isnan(precord->val);
    }

    return 0;
}
