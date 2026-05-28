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

//
#include <aoRecord.h>

//
#include <drvF3RP61.h>

//
#include <math.h>

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
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_ao,
    NULL
};

epicsExportAddress(dset, devAoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(aoRecord *precord)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, precord->name);

    //
    struct link *plink = &precord->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devAoF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");

    //
    const int ret = f3rp61ParseLink(plink, dpvt, (dbCommon *)precord, "devAoF3RP61");
    if (ret < 0) {
        //errlogPrintf("devAoF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion Option
    const int8_t option = dpvt->option;
    if (option == 'W') {        // Dummy option for Word access
    } else if (option == 'U') { // Unsigned integer, perhaps we'd better disable this
    } else if (option == 'L') { // Long word
    } else if (option == 'F') { // Single precision floating point
    } else if (option == 'D') { // Double precision floating point
    } else {                    // Option not recognized
        errlogPrintf("devAoF3RP61: %s : unsupported option \'%c\'\n", precord->name, option);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    const int8_t device = dpvt->device;
    if (0) {                                     // dummy
    } else if (device == 'R' || device == 'W' || // Shared registers and Link registers
               device == 'r') {                  // Shared registers - Using 'Old' interface
    } else if (device == 'Y') {                  // Output relays on I/O modules
    } else if (device == 'A') {                  // I/O registers on special modules
        // 'D' and 'F' option might not make sence for device 'A'
        //if (option != 'W') { // || option != 'U' || option != 'L'
        //    errlogPrintf("devAoF3RP61: %s : unsupported option \'%c\'\n", precord->name, option);
        //    precord->pact = 1;
        //    return -1;
        //}
        if (option == 'L' || option == 'F' || option == 'D') {
            dpvt->count  /= 2; // we use M3IO_WRITE_REG_L
        }
    } else {
        errlogPrintf("devAoF3RP61: %s : unsupported device \'%c\'\n", precord->name, device);
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
    // debug
    //if (precord->scan == SCAN_IO_EVENT) {
    //    errlogPrintf("devAoF3RP61: %s : SCAN by I/O intr\n", precord->name);
    //}

    F3RP61_DPVT  *dpvt = precord->dpvt;
    const int8_t  device = dpvt->device;
    const int8_t  option = dpvt->option;
    const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count;

    // Compose data to write
    uint16_t wdata[8] = {0}; // 4 would be enough, but readM3IoModeRegister requires 8
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
        const int32_t addr = dpvt->addr;
        if (writeM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAoF3RP61: %s : writeM3ComRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devAoF3RP61: %s : writeM3LinkRegister failed [%d]\n", precord->name, errno);
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
        if (ioctl(f3rp61_fd, M3IO_WRITE_COM, &acom) < 0) {
            errlogPrintf("devAoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (writeM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devAoF3RP61: %s : writeM3CpuMemory failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else if (device == 'Y') { // Output relays on I/O modules
        M3IO_ACCESS_REG drly = {
            .unitno = getunit(dpvt->addr),
            .slotno = getslot(dpvt->addr),
            .start  = getaddr(dpvt->addr),
            .count  = count,
        };
        drly.u.outrly[0].data = wdata[0];
        drly.u.outrly[0].mask = mask[0];
        if (option == 'L' || option == 'F') { // count == 2
            drly.u.outrly[1].data = wdata[1];
            drly.u.outrly[1].mask = mask[1];

        }
        if (option == 'D') { // count == 4
            drly.u.outrly[1].data = wdata[1];
            drly.u.outrly[1].mask = mask[1];
            drly.u.outrly[2].data = wdata[2];
            drly.u.outrly[2].mask = mask[2];
            drly.u.outrly[3].data = wdata[3];
            drly.u.outrly[3].mask = mask[3];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY, &drly) < 0) {
            errlogPrintf("devAoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
    } else {//(device == 'A')   // I/O registers on special modules
        M3IO_ACCESS_REG drly = {
            .unitno = getunit(dpvt->addr),
            .slotno = getslot(dpvt->addr),
            .start  = getaddr(dpvt->addr),
            .count  = count,
        };
        if (option == 'L' || option == 'F' || option == 'D') { // count == 2 || count == 4
            drly.u.pldata = ldata;
            if (ioctl(f3rp61_fd, M3IO_WRITE_REG_L, &drly) < 0) {
                errlogPrintf("devAoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
                return -1;
            }
        } else {
            drly.u.pwdata = wdata;
            if (ioctl(f3rp61_fd, M3IO_WRITE_REG, &drly) < 0) {
                errlogPrintf("devAoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
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
