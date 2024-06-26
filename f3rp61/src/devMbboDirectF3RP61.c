/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
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
#include <mbboDirectRecord.h>

#include <drvF3RP61.h>

// Create the dset for devMbboDirectF3RP61
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
    IOSCANPVT ioscanpvt; // must come first
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
} F3RP61_LO_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbboDirectRecord *precord)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;

    // Link type must be INST_IO
    if (precord->out.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devMbboDirectF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->out;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse option
    //char *popt = strchr(buf, '&');
    //if (popt) {
    //    char option = 'W'; // Dummy option for Word access
    //    *popt++ = '\0';
    //    if (sscanf(popt, "%c", &option) < 1) {
    //        errlogPrintf("devMbboDirectF3RP61: can't get option for %s\n", precord->name);
    //        precord->pact = 1;
    //        return -1;
    //    }
    //    if (1) {                    // Option not recognized
    //        errlogPrintf("devMbboDirectF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
    //        precord->pact = 1;
    //       return -1;
    //   }
    //}

    // Parse for possible interrupt source
    char *pint = strchr(buf, ':');
    if (pint) {
        *pint++ = '\0';
        if (sscanf(pint, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devMbboDirectF3RP61: can't get interrupt source address for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) precord, unitno, slotno, start) < 0) {
            errlogPrintf("devMbboDirectF3RP61: can't register I/O interrupt for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse slot, device and register number
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devMbboDirectF3RP61: can't get I/O address for %s\n", precord->name);
                precord->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
                errlogPrintf("devMbboDirectF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
                precord->pact = 1;
                return -1;
            }
        } else {
            device = 'r'; // Shared memory (or 'Old interface' for shared registers/relays)
        }
    }

    // Allocate private data storage area
    F3RP61_LO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_LO_DPVT), "calloc failed");
    dpvt->device = device;

    // Check device validity and compose data structure for I/O request
    if (0) {                                     // dummy

    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'E' || device == 'L' || // Shared relay and Link relay
               device == 'r') {                  // Shared memory
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno; // for 'r' devices
        pacom->start = start;
        pacom->count = 1;

    } else if (device == 'Y' ||                  // Output relays on I/O modules
               device == 'M' ||                  // Mode registers on I/O modules
               device == 'A') {                  // I/O registers on special modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        pdrly->count  = 1;

    } else {
        errlogPrintf("devMbboDirectF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 2; // no conversion
}

// write_mbboDirect() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_mbboDirect(mbboDirectRecord *precord)
{
    F3RP61_LO_DPVT *dpvt = precord->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    const char device = dpvt->device;

    // Compose data to write
    uint16_t wdata = (uint16_t) precord->rval;
    uint16_t mask  = 0xffff;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        if (writeM3ComRegister(pacom->start, pacom->count, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3ComRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        if (writeM3LinkRegister(pacom->start, pacom->count, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3LinkRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'E') { // Shared relays
        if (writeM3ComRelay(pacom->start, pacom->count, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3ComRelay failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        if (writeM3LinkRelay(pacom->start, pacom->count, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3LinkRelay failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'r') { // Shared memory
#if defined(__powerpc__)
        pacom->pdata = &wdata;
        if (ioctl(f3rp61_fd, M3IO_WRITE_COM, pacom) < 0) {
            errlogPrintf("devMbboDirectF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#else
        if (writeM3CpuMemory(pacom->cpuno, pacom->start, pacom->count, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3CpuMemory failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#endif

    } else if (device == 'Y') { // Output relays on I/O modules
        pdrly->u.outrly[0].data = wdata;
        pdrly->u.outrly[0].mask = mask;
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY, pdrly) < 0) {
            errlogPrintf("devMbboDirectF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'M') { // Mode registers on I/O modules
#if defined(__powerpc__)
        // On F3RP61 start and count are fixed to 1 and 3 in ioctl() request,
        // and only the 1st element is valid in the data written.
        pdrly->start  = 1;
        pdrly->count  = 3;
        pdrly->u.wdata[0] = wdata;
        if (ioctl(f3rp61_fd, M3IO_WRITE_MODE, pdrly) < 0) {
            errlogPrintf("devMbboDirectF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#else
        if (writeM3IoModeRegister(pdrly->unitno, pdrly->slotno, pdrly->start, pdrly->count, &wdata) < 0) {
            errlogPrintf("devMbboDirectF3RP61: writeM3IoModeRegisterL failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#endif

    } else {//(device == 'A')   // I/O registers on special modules
        pdrly->u.pwdata = &wdata;
        if (ioctl(f3rp61_fd, M3IO_WRITE_REG, pdrly) < 0) {
            errlogPrintf("devMbboDirectF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
    }

    //
    precord->udf = FALSE;

    return 0;
}
