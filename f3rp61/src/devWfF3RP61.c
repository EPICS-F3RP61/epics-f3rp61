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

//
#include <waveformRecord.h>

//
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
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_wf,
    NULL
};

epicsExportAddress(dset, devWfF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(waveformRecord *precord)
{
    //
    struct link *plink = &precord->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devWfF3RP61 (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");
    const int ftvl = precord->ftvl;
    void *pdata = callocMustSucceed(precord->nelm, dbValueSize(ftvl), "calloc failed");
    dpvt->pdata = pdata;

    //
    const int ret = f3rp61ParseLink(plink, dpvt, (dbCommon *)precord, "devWfF3RP61");
    if (ret < 0) {
        //errlogPrintf("devWfF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion Option
    const int8_t option = dpvt->option;
    if (option == 'W') {        // Dummy option for Word access
    //} else if (option == 'U') { // Unsigned integer
    //} else if (option == 'L') { // Long word
    //} else if (option == 'F') { // Single precision floating point
    //} else if (option == 'D') { // Double precision floating point
    } else {                    // Option not recognized
        errlogPrintf("devWfF3RP61: %s : unsupported option \'%c\'\n", precord->name, option);
        precord->pact = 1;
        return -1;
    }

    // Consider I/O data length
    // Note : It is **WRONG** that count depending on FTVL.
    //        What we need are (1) count depending on &L/&F/&D option and (2) check for supported FTVL.
    dpvt->count = 0;
    switch (ftvl) {
    case DBF_DOUBLE:
        dpvt->count = (unsigned short) (precord->nelm * 4);
        break;
    case DBF_FLOAT:
    case DBF_ULONG:
    case DBF_LONG:
        dpvt->count = (unsigned short) (precord->nelm * 2);
        break;
    case DBF_USHORT:
    case DBF_SHORT:
        dpvt->count = (unsigned short) (precord->nelm * 1);
        break;
    default: // STRING, CHAR, UCHAR, ENUM
        errlogPrintf("devWfF3RP61: %s : unsupported FTVL field %d\n", precord->name, ftvl);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'r') {                  // Shared memory
    } else if (device == 'A') {                  // I/O registers on special modules
        if (ftvl != DBF_USHORT && ftvl != DBF_SHORT) {
            dpvt->count  /= 2; // we use M3IO_READ_REG_L for DOUBLE, FLOAT, ULONG, and LONG
        }

    } else {
        errlogPrintf("devWfF3RP61: %s : unsupported device \'%c\'\n", precord->name, device);
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
    F3RP61_DPVT  *dpvt = precord->dpvt;
    const int8_t  device = dpvt->device;
    //const int8_t  option = dpvt->option;
    const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count;

    //
    const int ftvl = precord->ftvl;

    // Buffers for data read
    uint16_t *wdata = dpvt->pdata;
    ulong    *ldata = dpvt->pdata;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (readM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devWfF3RP61: %s : readM3ComRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (readM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devWfF3RP61: %s : readM3LinkRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'r') { // Shared memory
#if defined(__powerpc__)
        M3IO_ACCESS_COM acom = {
            .cpuno = cpuno,
            .start = dpvt->addr,
            .count = count,
            .pdata = wdata,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_COM, &acom) < 0) {
            errlogPrintf("devWfF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (readM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devWfF3RP61: %s : readM3CpuMemory failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else {//(device == 'A')   // I/O registers on special modules
        M3IO_ACCESS_REG drly = {
            .unitno = getunit(dpvt->addr),
            .slotno = getslot(dpvt->addr),
            .start  = getaddr(dpvt->addr),
            .count  = count,
        };
        if (ftvl != DBF_USHORT && ftvl != DBF_SHORT) {
            drly.u.pldata = ldata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG_L, &drly) < 0) {
                errlogPrintf("devWfF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
                return -1;
            }
        } else {
            drly.u.pwdata = wdata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG, &drly) < 0) {
                errlogPrintf("devWfF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
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
