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

//
#include <aiRecord.h>

//
#include <drvF3RP61.h>

//
#include <math.h>

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
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    read_ai,
    NULL
};

epicsExportAddress(dset, devAiF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aiRecord *precord)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, precord->name);

    //
    struct link *plink = &precord->inp;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devAiF3RP61 (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");

    //
    const int ret = f3rp61ParseLink(plink, dpvt, (dbCommon *)precord, "devAiF3RP61");
    if (ret < 0) {
        //errlogPrintf("devAiF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion Option
    const int8_t option = dpvt->option;
    if (option == 'W') {        // Dummy option for Word access
    } else if (option == 'U') { // Unsigned integer
    } else if (option == 'L') { // Long word
    } else if (option == 'F') { // Single precision floating point
    } else if (option == 'D') { // Double precision floating point
    } else {                    // Option not recognized
        errlogPrintf("devAiF3RP61: %s : unsupported option \'%c\'\n", precord->name, option);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'r') {                  // Shared memory
    } else if (device == 'X' || device == 'Y') { // Input and output relays on I/O modules
    } else if (device == 'A') {                  // I/O registers on special modules
        // 'D' and 'F' option might not make sence for device 'A'
        //if (option != 'W') { // || option != 'U' || option != 'L'
        //    errlogPrintf("devAiF3RP61: %s ; unsupported option \'%c\'\n", precord->name, option);
        //    precord->pact = 1;
        //    return -1;
        //}
        if (option == 'L' || option == 'F' || option == 'D') {
            dpvt->count /= 2; // we use M3IO_READ_REG_L
        }
    } else {
        errlogPrintf("devAiF3RP61: %s : unsupported device \'%c\'\n", precord->name, device);
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
    // debug
    //if (precord->scan == SCAN_IO_EVENT) {
    //    errlogPrintf("devAiF3RP61: %s : SCAN by I/O intr\n", precord->name);
    //}

    F3RP61_DPVT  *dpvt = precord->dpvt;
    const int8_t  device = dpvt->device;
    const int8_t  option = dpvt->option;
    const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count;

    // Buffers for data read
    uint16_t wdata[4] = {0};
    ulong    ldata[2] = {0};

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (readM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAiF3RP61: %s : readM3ComRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (readM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAiF3RP61: %s : readM3LinkRegister failed [%d]\n", precord->name, errno);
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
            errlogPrintf("devAiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (readM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devAiF3RP61: %s : readM3CpuMemory failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else if (device == 'X') { // Input relays on I/O modules
        M3IO_ACCESS_REG drly = {
            .unitno = getunit(dpvt->addr),
            .slotno = getslot(dpvt->addr),
            .start  = getaddr(dpvt->addr),
            .count  = count,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_INRELAY, &drly) < 0) {
            errlogPrintf("devAiF3RP61: %s : ioctl failed [%d]n", precord->name, errno);
            return -1;
        }
        wdata[0] = drly.u.inrly[0].data;
        if (option == 'L' || option == 'F') { // count == 2
            wdata[1] = drly.u.inrly[1].data;
        }
        if (option == 'D') { // count == 4
            wdata[1] = drly.u.inrly[1].data;
            wdata[2] = drly.u.inrly[2].data;
            wdata[3] = drly.u.inrly[3].data;
        }

    } else if (device == 'Y') { // Output relays on I/O modules
        M3IO_ACCESS_REG drly = {
            .unitno = getunit(dpvt->addr),
            .slotno = getslot(dpvt->addr),
            .start  = getaddr(dpvt->addr),
            .count  = count,
        };
        if (ioctl(f3rp61_fd, M3IO_READ_OUTRELAY, &drly) < 0) {
            errlogPrintf("devAiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#if defined(__powerpc__)
        wdata[0] = drly.u.inrly[0].data;
        if (option == 'L' || option == 'F') { // count == 2
            wdata[1] = drly.u.inrly[1].data;
        }
        if (option == 'D') { // count == 4
            wdata[1] = drly.u.inrly[1].data;
            wdata[2] = drly.u.inrly[2].data;
            wdata[3] = drly.u.inrly[3].data;
        }
#else
        wdata[0] = drly.u.outrly[0].data;
        if (option == 'L' || option == 'F') { // count == 2
            wdata[1] = drly.u.outrly[1].data;
        }
        if (option == 'D') { // count == 4
            wdata[1] = drly.u.outrly[1].data;
            wdata[2] = drly.u.outrly[2].data;
            wdata[3] = drly.u.outrly[3].data;
        }
#endif

    } else {//(device == 'A')   // I/O registers on special modules
        M3IO_ACCESS_REG drly = {
            .unitno = getunit(dpvt->addr),
            .slotno = getslot(dpvt->addr),
            .start  = getaddr(dpvt->addr),
            .count  = count,
        };
        if (option == 'L' || option == 'F' || option == 'D') {
            drly.u.pldata = ldata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG_L, &drly) < 0) {
                errlogPrintf("devAiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
                return -1;
            }
        } else {
            drly.u.pwdata = wdata;
            if (ioctl(f3rp61_fd, M3IO_READ_REG, &drly) < 0) {
                errlogPrintf("devAiF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
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
            uint64_t lval = (((uint64_t)ldata[1])<<32) | (uint64_t)ldata[0];
            memcpy(&val, &lval, sizeof(double));
        } else {
            uint64_t lval = (((uint64_t)wdata[3])<<48) | (((uint64_t)wdata[2])<<32) | ((uint64_t)wdata[1]<<16) | (uint64_t)wdata[0];

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
