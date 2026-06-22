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

//
#include <longoutRecord.h>

//
#include <drvF3RP61.h>
#include <devF3RP61bcd.h>

//
static const F3RP61_RW rw = kWrite;
static const F3RP61_ACCESS_TYPE type = kWord;

// Create the dset for devLoF3RP61
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
    f3rp61Init,
    init_record,
    f3rp61GetIoIntInfo,
    write_longout,
    NULL
};

epicsExportAddress(dset, devLoF3RP61);

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(longoutRecord *precord)
{
    //debug
    //printf("%s:%s %s\n", __FILE__, __func__, precord->name);

    //
    struct link *plink = &precord->out;

    // Link type must be INST_IO
    if (plink->type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devLoF3RP61 (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    // Allocate private data storage area
    F3RP61_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_DPVT), "calloc failed");

    //
    const int ret = f3rp61ParseLink(plink, dpvt, rw, type, (dbCommon *)precord, sizeof(int32_t), 1, "devLoF3RP61");
    if (ret < 0) {
        //errlogPrintf("devLoF3RP61: %s : syntax error in INP field\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check conversion specifier
    const int8_t conv = dpvt->conv;
    if (conv == 'W') {        // Dummy for Word access
    } else if (conv == 'B') { // Binary Coded Decimal format
    } else if (conv == 'U') { // Unsigned integer
    } else if (conv == 'L') { // Long word
    //} else if (conv == 'X') { // Long word access for XP01/XP02 modules (might be supported in the future)
    } else {
        errlogPrintf("devLoF3RP61: %s : unsupported conversion specifier \'%c\'\n", precord->name, conv);
        precord->pact = 1;
        return -1;
    }

    //
    precord->dpvt = dpvt;

    return 0;
}

// write_longout() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_longout(longoutRecord *precord)
{
    // debug
    //if (precord->scan == SCAN_IO_EVENT) {
    //    errlogPrintf("devLoF3RP61: %s : SCAN by I/O intr\n", precord->name);
    //}

    F3RP61_DPVT  *dpvt = precord->dpvt;
    const int8_t  device = dpvt->device;
    const int8_t  conv   = dpvt->conv;
    const int32_t cpuno  = dpvt->cpuno; // for Shared memory (or 'Old interface' for shared registers/relays)
    const int32_t count  = dpvt->count;

    // Compose data to write
    uint16_t *wdata   = dpvt->buf;
    uint16_t  mask[2] = {0xffff, 0xffff};
    ulong    *ldata   = dpvt->buf;

    if (conv == 'B') {
        wdata[0] = devF3RP61int2bcd(precord->val, precord);
    } else if (conv == 'L') {
        wdata[0] = (uint16_t)(precord->val>> 0);
        wdata[1] = (uint16_t)(precord->val>>16);
    } else if (conv == 'X') { // long word access for XP01/XP02 modules (might be supported in the future)
#if defined(__powerpc__)
        ulong val = (ulong)precord->val;
        ldata[0] = (val >> 16) | (val << 16); // we need word-swap for F3RP61
#else
        ldata[0] = (uint32_t)precord->val;
#endif
    } else {
        wdata[0] = (uint16_t)precord->val;
    }

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'R') { // Shared registers
        const int32_t addr = dpvt->addr;
        if (writeM3ComRegister(addr, count, wdata) < 0) {
            errlogPrintf("devLoF3RP61: %s : writeM3ComRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'W') { // Link registers
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRegister(addr, count, wdata) < 0) {
            errlogPrintf("devLoF3RP61: %s : writeM3LinkRegister failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'E') { // Shared relays
        const int32_t addr = dpvt->addr;
        if (writeM3ComRelay(addr, count, wdata) < 0) {
            errlogPrintf("devLoF3RP61: %s : writeM3ComRelay failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'L') { // Link relays
        const int32_t addr = dpvt->addr;
        if (writeM3LinkRelay(addr, count, wdata) < 0) {
            errlogPrintf("devLoF3RP61: %s : writeM3LinkRelay failed [%d]\n", precord->name, errno);
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
            errlogPrintf("devLoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t addr = dpvt->addr;
        if (writeM3CpuMemory(cpuno, addr, count, wdata) < 0) {
            errlogPrintf("devLoF3RP61: %s : writeM3CpuMemory failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else if (device == 'Y') { // Output relays on I/O modules
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            .start  = dpvt->addr,
            .count  = count,
        };
        drly.u.outrly[0].data = wdata[0];
        drly.u.outrly[0].mask = mask[0];
        if (conv == 'L') {
            drly.u.outrly[1].data = wdata[1];
            drly.u.outrly[1].mask = mask[1];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_OUTRELAY, &drly) < 0) {
            errlogPrintf("devLoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }

    } else if (device == 'M') { // Mode registers on I/O modules
#if defined(__powerpc__)
        // The F3RP61 Linux BSP Reference manual states that start
        // address is fixed at 1 and the count at 3. However it
        // appears that start address of 2, 3, or 4 are also
        // accepted. Similary, the count can be 1, 2, or 4. Be aware
        // that writing to the address 4 does not make sense, and may
        // cause problems.
        M3IO_ACCESS_REG drly = {
            .unitno = dpvt->unit,
            .slotno = dpvt->slot,
            //.start  = 1,
            //.count  = 3,
            .start  = dpvt->addr,
            .count  = count,
        };
        drly.u.wdata[0] = wdata[0];
        if (conv == 'L') {
            drly.u.wdata[1] = wdata[1];
        }
        if (ioctl(f3rp61_fd, M3IO_WRITE_MODE, &drly) < 0) {
            errlogPrintf("devLoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
#else
        const int32_t unit = dpvt->unit;
        const int32_t slot = dpvt->slot;
        const int32_t addr = dpvt->addr;
        if (writeM3IoModeRegister(unit, slot, addr, count, wdata) < 0) {
            errlogPrintf("devLoF3RP61: %s : writeM3IoModeRegister failed [%d]\n", precord->name, errno);
            return -1;
        }
#endif

    } else {//(device == 'A') // I/O registers on special modules
        if (conv == 'X') {  // long word access for XP01/XP02 modules (might be supported in the future)
            M3IO_ACCESS_REG drly = {
                .unitno   = dpvt->unit,
                .slotno   = dpvt->slot,
                .start    = dpvt->addr,
                .count    = count/2, //
                .u.pldata = ldata,
            };
            if (ioctl(f3rp61_fd, M3IO_WRITE_REG_L, &drly) < 0) {
                errlogPrintf("devLoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
                return -1;
            }
        } else {
            M3IO_ACCESS_REG drly = {
                .unitno   = dpvt->unit,
                .slotno   = dpvt->slot,
                .start    = dpvt->addr,
                .count    = count,
                .u.pwdata = wdata,
            };
            if (ioctl(f3rp61_fd, M3IO_WRITE_REG, &drly) < 0) {
                errlogPrintf("devLoF3RP61: %s : ioctl failed [%d]\n", precord->name, errno);
                return -1;
            }
        }
    }

    //
    precord->udf = FALSE;

    //
    return 0;
}
