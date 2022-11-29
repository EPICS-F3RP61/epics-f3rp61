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
#include <errno.h>
#include <fcntl.h>
//#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <alarm.h>
#include <cantProceed.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExport.h>
#include <errlog.h>
#include <recGbl.h>
#include <recSup.h>
#include <mbbiRecord.h>

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
// allocates private data storage area and sets initial configure
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
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for appending the NULL character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse device
    char device;
    if (sscanf(buf, "SYS,%c,", &device) < 1) {
        errlogPrintf("devMbbiF3RP61SysCtl: can't get device for %s\n", precord->name);
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
        errlogPrintf("devMbbiF3RP61SysCtl: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    precord->dpvt = dpvt;

    return 0;
}

// read_mbbi() is called when there was a request to process a
// record. When called, it reads the value from the driver and stores
// to the VAL field.
static long read_mbbi(mbbiRecord *precord)
{
    F3RP61SysCtl_MBBI_DPVT *dpvt = precord->dpvt;
    const char device = dpvt->device;

    unsigned long data = -1;

    // Issue API function
    if (0) {                                     // dummy

    } else if (device == 'S') {                  // Mode-sw
        if (ioctl(f3rp61SysCtl_fd, M3SC_GET_SW, &data) < 0) {
            errlogPrintf("devMbbiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
        precord->rval = data;
    }

    //
    precord->udf = FALSE;

    return 0;
}
