/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
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
#include <waveformRecord.h>

#include <drvF3RP61.h>

// Create the dset for devWfF3RP61
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

typedef struct {
    IOSCANPVT ioscanpvt; // must come first
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    //char option;
    void *pdata;
} F3RP61_WF_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(waveformRecord *precord)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;
    //char option = 'W'; // option is no implemented for waveform Record
    const int ftvl = precord->ftvl;

    // Link type must be INST_IO
    if (precord->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devWfF3RP61 (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->inp;
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
    //        errlogPrintf("devWfF3RP61: can't get option for %s\n", precord->name);
    //        precord->pact = 1;
    //        return -1;
    //    }
    //    if (1) {                    // Option not recognized
    //        errlogPrintf("devWfF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
    //        precord->pact = 1;
    //       return -1;
    //   }
    //}
    //if (!(option == 'W' || option == 'L' || option == 'U' || option == 'F' || option == 'D')) {
    //    errlogPrintf("devWfF3RP61: unsupported option \'%c\' for %s\n", option, precord->name);
    //    precord->pact = 1;
    //    return -1;
    //}

    // Parse for possible interrupt source
    char *pint = strchr(buf, ':'); // check if SCAN is interrupt based (example: @U0,S3,Y1:U0,S4,X1)
    if (pint) {
        *pint++ = '\0';
        if (sscanf(pint, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devWfF3RP61: can't get interrupt source address for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (f3rp61_register_io_interrupt((dbCommon *) precord, unitno, slotno, start) < 0) {
            errlogPrintf("devWfF3RP61: can't register I/O interrupt for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse slot, device and register number
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devWfF3RP61: can't get I/O address for %s\n", precord->name);
                precord->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'R') {
                errlogPrintf("devWfF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
                precord->pact = 1;
            }
        } else {
            device = 'r'; // Shared memory (or 'Old interface' for shared registers/relays)
        }
    }

    // Allocate private data storage area
    F3RP61_WF_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_WF_DPVT), "calloc failed");
    dpvt->device = device;

    void *pdata = callocMustSucceed(precord->nelm, dbValueSize(ftvl), "calloc failed");
    dpvt->pdata = pdata;

    // Consider I/O data length
    // Note : It is **WRONG** that count depending on FTVL.
    //        What we need are (1) count depending on &L/&F/&D option and (2) check for supported FTVL.
    int count = 0;
    switch (ftvl) {
    case DBF_DOUBLE:
        count = (unsigned short) (precord->nelm * 4);
        break;
    case DBF_FLOAT:
    case DBF_ULONG:
    case DBF_LONG:
        count = (unsigned short) (precord->nelm * 2);
        break;
    case DBF_USHORT:
    case DBF_SHORT:
        count = (unsigned short) (precord->nelm * 1);
        break;
    default: // STRING, CHAR, UCHAR, ENUM
        errlogPrintf("devWfF3RP61: unsupported FTVL field %d for %s\n", ftvl, precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check device validity and compose data structure for I/O request
    if (0) {                                     // dummy

    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'r') {                  // Shared memory
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno; // for 'r' devices
        pacom->start = start;
        pacom->count = count;

    } else if (device == 'A') {                  // I/O registers on special modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        if (ftvl != DBF_USHORT && ftvl != DBF_SHORT) {
            pdrly->count  = count/2; // we use M3IO_READ_REG_L for DOUBLE, FLOAT, ULONG, and LONG
        } else {
            pdrly->count  = count;
        }

    } else {
        errlogPrintf("devWfF3RP61: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// read_wf() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_wf(waveformRecord *precord)
{
    F3RP61_WF_DPVT *dpvt = precord->dpvt;
    const int ftvl = precord->ftvl;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    const char device = dpvt->device;
    //const char option = dpvt->option;

    // Buffers for data read
    uint16_t *wdata = dpvt->pdata;
    ulong    *ldata = dpvt->pdata;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        if (readM3ComRegister(pacom->start, pacom->count, wdata) < 0) {
            errlogPrintf("devWfF3RP61: readM3ComRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        if (readM3LinkRegister(pacom->start, pacom->count, wdata) < 0) {
            errlogPrintf("devWfF3RP61: readM3LinkRegister failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

    } else if (device == 'r') { // Shared memory
#if defined(__powerpc__)
        pacom->pdata = wdata;
        if (ioctl(f3rp61_fd, M3IO_READ_COM, pacom) < 0) {
            errlogPrintf("devWfF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#else
        if (readM3CpuMemory(pacom->cpuno, pacom->start, pacom->count, wdata) < 0) {
            errlogPrintf("devWfF3RP61: readM3CpuMemory failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#endif

    } else {//(device == 'A')   // I/O registers on special modules
        if (ftvl != DBF_USHORT && ftvl != DBF_SHORT) {
            pdrly->u.pldata = ldata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG_L, pdrly) < 0) {
                errlogPrintf("devWfF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
                return -1;
            }
        } else {
            pdrly->u.pwdata = wdata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG, pdrly) < 0) {
                errlogPrintf("devWfF3RP61: ioctl failed [%d] for %s\n", errno, precord->name);
                return -1;
            }
        }
    }

    //
    precord->udf = FALSE;

    // fill VAL field
    // need clean up
    switch (device) {
        unsigned char  *p1;
        unsigned long  *p2;
        unsigned short *p3;
    case 'r':
    case 'W':
    case 'R':
        switch (ftvl) {
        case DBF_DOUBLE:
            p1 = precord->bptr;
            for (int i = 0; i < precord->nelm; i++) {
                *p1++ = (wdata[3 + (4 * i)] >> 8) & 0xff; *p1++ = wdata[3 + (4 * i)] & 0xff;
                *p1++ = (wdata[2 + (4 * i)] >> 8) & 0xff; *p1++ = wdata[2 + (4 * i)] & 0xff;
                *p1++ = (wdata[1 + (4 * i)] >> 8) & 0xff; *p1++ = wdata[1 + (4 * i)] & 0xff;
                *p1++ = (wdata[0 + (4 * i)] >> 8) & 0xff; *p1++ = wdata[0 + (4 * i)] & 0xff;
            }
            break;
        case DBF_FLOAT:
            p1 = precord->bptr;
            for (int i = 0; i < precord->nelm; i++) {
                *p1++ = (wdata[1 + (2 * i)] >> 8) & 0xff; *p1++ = wdata[1 + (2 * i)] & 0xff;
                *p1++ = (wdata[0 + (2 * i)] >> 8) & 0xff; *p1++ = wdata[0 + (2 * i)] & 0xff;
            }
            break;
        case DBF_ULONG:
        case DBF_LONG:
            p2 = precord->bptr;
            for (int i = 0; i < precord->nelm; i++) {
                p2[i] = ((wdata[1 + (2 *i)] << 16) & 0xffff0000)  |  (wdata[0 + (2 * i)] & 0x0000ffff);
            }
            break;
        case DBF_USHORT:
        case DBF_SHORT:
            p3 = precord->bptr;
            for (int i = 0; i < precord->nelm; i++) {
                p3[i] = wdata[0 + (1 * i)];
            }
            break;
        default:
            errlogPrintf("%s:unsupported field type of value\n", precord->name);
            precord->pact = 1;
            return -1;
        }
        break;
    default:
        switch (ftvl) {
        case DBF_ULONG:
            p2 = precord->bptr;
            for (int i = 0; i < precord->nelm; i++) {
                p2[i] = ldata[i];
            }
            break;
        case DBF_USHORT:
        case DBF_SHORT:
            p3 = precord->bptr;
            for (int i = 0; i < precord->nelm; i++) {
                p3[i] = wdata[i];
            }
            break;
        default:
            errlogPrintf("%s:unsupported field type of value\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    precord->nord = precord->nelm;

    return 0;
}
