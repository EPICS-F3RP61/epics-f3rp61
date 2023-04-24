/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
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
#include <aiRecord.h>

#include <drvF3RP61.h>

// Create the dset for devAiF3RP61
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
    IOSCANPVT ioscanpvt; // must come first
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    char option;
} F3RP61_AI_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aiRecord *precord)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    // Link type must be INST_IO
    if (precord->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devAiF3RP61 (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->inp;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse option
    char *popt = strchr(buf, '&');
    if (popt) {
        *popt++ = '\0';
        if (sscanf(popt, "%c", &option) < 1) {
            errlogPrintf("devAiF3RP61: can't get option for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'D') { // Double precision floating point
        } else if (option == 'F') { // Single precision floating point
        } else if (option == 'L') { // Long word
        } else if (option == 'U') { // Unsigned integer
        } else {                    // Option not recognized
            errlogPrintf("devAiF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse for possible interrupt source
    char *pint = strchr(buf, ':'); // check if SCAN is interrupt based (example: @U0,S3,Y1:U0,S4,X1)
    if (pint) {
        *pint++ = '\0';
        if (sscanf(pint, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devAiF3RP61: can't get interrupt source address for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) precord, unitno, slotno, start) < 0) {
            errlogPrintf("devAiF3RP61: can't register I/O interrupt for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse slot, device and register number
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devAiF3RP61: can't get I/O address for %s\n", precord->name);
                precord->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'R') {
                errlogPrintf("devAiF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
                precord->pact = 1;
            }
        } else {
            device = 'r'; // Shared memory (or 'Old interface' for shared registers/relays)
        }
    }

    // Allocate private data storage area
    F3RP61_AI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_AI_DPVT), "calloc failed");
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
               device == 'r') {                  // Shared memory
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno; // for 'r' devices
        pacom->start = start;
        pacom->count = count;

    } else if (device == 'X' || device == 'Y') { // Input and output relays on I/O modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        pdrly->count  = count;

    } else if (device == 'A') {                  // I/O registers on special modules
        // 'D' and 'F' option might not make sence for device 'A'
        //if (option != 'W') { // || option != 'U' || option != 'L'
        //    errlogPrintf("devAiF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
        //    precord->pact = 1;
        //    return -1;
        //}
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        if (option == 'L' || option == 'F' || option == 'D') {
            pdrly->count = count/2; // we use M3IO_READ_REG_L
        } else {
            pdrly->count = count;
        }

    } else {
        errlogPrintf("devAiF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// read_ai() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_ai(aiRecord *precord)
{
    F3RP61_AI_DPVT *dpvt = precord->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    const char device = dpvt->device;
    const char option = dpvt->option;

    // Buffers for data read
    uint16_t wdata[4] = {0};
    ulong    ldata[2] = {0};

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        if (readM3ComRegister(pacom->start, pacom->count, wdata) < 0) {
            errlogPrintf("devAiF3RP61: readM3ComRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        if (readM3LinkRegister(pacom->start, pacom->count, wdata) < 0) {
            errlogPrintf("devAiF3RP61: readM3LinkRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'r') { // Shared memory
#if defined(__powerpc__)
        pacom->pdata = wdata;
        if (ioctl(f3rp61_fd, M3IO_READ_COM, pacom) < 0) {
            errlogPrintf("devAiF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#else
        if (readM3CpuMemory(pacom->cpuno, pacom->start, pacom->count, wdata) < 0) {
            errlogPrintf("devAiF3RP61: readM3CpuMemory failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#endif

    } else if (device == 'X') { // Input relays on I/O modules
        if (ioctl(f3rp61_fd, M3IO_READ_INRELAY, pdrly) < 0) {
            errlogPrintf("devAiF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
        wdata[0] = pdrly->u.inrly[0].data;
        if (option == 'L' || option == 'F') { // count == 2
            wdata[1] = pdrly->u.inrly[1].data;
        }
        if (option == 'D') { // count == 4
            wdata[1] = pdrly->u.inrly[1].data;
            wdata[2] = pdrly->u.inrly[2].data;
            wdata[3] = pdrly->u.inrly[3].data;
        }

    } else if (device == 'Y') { // Output relays on I/O modules
        if (ioctl(f3rp61_fd, M3IO_READ_OUTRELAY, pdrly) < 0) {
            errlogPrintf("devAiF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#if defined(__powerpc__)
        wdata[0] = pdrly->u.inrly[0].data;
        if (option == 'L' || option == 'F') { // count == 2
            wdata[1] = pdrly->u.inrly[1].data;
        }
        if (option == 'D') { // count == 4
            wdata[1] = pdrly->u.inrly[1].data;
            wdata[2] = pdrly->u.inrly[2].data;
            wdata[3] = pdrly->u.inrly[3].data;
        }
#else
        wdata[0] = pdrly->u.outrly[0].data;
        if (option == 'L' || option == 'F') { // count == 2
            wdata[1] = pdrly->u.outrly[1].data;
        }
        if (option == 'D') { // count == 4
            wdata[1] = pdrly->u.outrly[1].data;
            wdata[2] = pdrly->u.outrly[2].data;
            wdata[3] = pdrly->u.outrly[3].data;
        }
#endif

    } else {//(device == 'A')   // I/O registers on special modules
        if (option == 'L' || option == 'F' || option == 'D') {
            pdrly->u.pldata = ldata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG_L, pdrly) < 0) {
                errlogPrintf("devAiF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
                return -1;
            }
        } else {
            pdrly->u.pwdata = wdata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG, pdrly) < 0) {
                errlogPrintf("devAiF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
                return -1;
            }
        }
    }

    //
    precord->udf = FALSE;

    // fill VAL field
    if (option == 'D') {
        double val;
        if (device == 'A') {
            uint64_t lval = (((int64_t)ldata[1])<<32) | ldata[0];
            memcpy(&val, &lval, sizeof(double));
        } else {
            uint64_t lval = (((uint64_t)wdata[3])<<48) | (((uint64_t)wdata[2])<<32) | (wdata[1]<<16) | wdata[0];
            memcpy(&val, &lval, sizeof(double));
        }
        // todo : consider ASLO and AOFF field
        // todo : consider SMOO field
        precord->val = val;
        precord->udf = isnan(precord->val);
        return 2; // no conversion
    } else if (option == 'F') {
        float val;
        if (device == 'A') {
            uint32_t lval = ldata[0];
            memcpy(&val, &lval, sizeof(float));
        } else {
            uint32_t lval = (wdata[1]<<16) | wdata[0];
            memcpy(&val, &lval, sizeof(float));
        }
        // todo : consider ASLO and AOFF field
        // todo : consider SMOO field
        precord->val = val;
        precord->udf = isnan(precord->val);
        return 2; // no conversion
    } else if (option == 'L') {
        if (device == 'A') {
            precord->rval = ldata[0];
        } else {
            precord->rval = wdata[1]<<16 | wdata[0];
        }
    } else if (option == 'U') {
        precord->rval = (uint16_t)wdata[0];
    } else {
        precord->rval = (int16_t)wdata[0];
    }

    return 0;
}
