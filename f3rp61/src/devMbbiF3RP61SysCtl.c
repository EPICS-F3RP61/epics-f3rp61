/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiF3RP61SysCtl.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
*
*      Author: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/

//
#include <mbbiRecord.h>

//
#include <drvF3RP61SysCtl.h>

// Create the dset for devMbbiF3RP61SysCtl
static long init_record();
static long read_mbbi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_mbbi;
    DEVSUPFUN  special_linconv;
} devMbbiF3RP61SysCtl = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    read_mbbi,
    NULL
};

epicsExportAddress(dset, devMbbiF3RP61SysCtl);

typedef struct {
    char device;
} F3RP61SysCtl_MBBI_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(mbbiRecord *precord)
{
    // Link type must be INST_IO
    if (precord->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devMbbiF3RP61SysCtl (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->inp;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse device
    char device;
    if (sscanf(buf, "SYS,%c,", &device) < 1) {
        errlogPrintf("devMbbiF3RP61SysCtl: %s : can't get device\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Allocate private data storage area
    F3RP61SysCtl_MBBI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SysCtl_MBBI_DPVT), "calloc failed");
    dpvt->device = device;

    // Check device validity
    if (0) {                                     // dummy

    } else if (device == 'S') {                  // Mode-SW

    } else {
        errlogPrintf("devMbbiF3RP61SysCtl: %s : unsupported device \'%c\'s\n", precord->name, device);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// read_mbbi() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field.
static long read_mbbi(mbbiRecord *precord)
{
    F3RP61SysCtl_MBBI_DPVT *dpvt = precord->dpvt;
    const char device = dpvt->device;

    unsigned long data = -1;

    // Issue API function
    if (0) {                                     // dummy

    } else if (device == 'S') {                  // Mode-sw
        if (ioctl(f3rp61SysCtl_fd, M3SC_GET_SW, &data) < 0) {
            errlogPrintf("devMbbiF3RP61SysCtl: %s : ioctl failed [%d]\n", precord->name, errno);
            return -1;
        }
        precord->rval = data;
    }

    //
    precord->udf = FALSE;

    return 0;
}
