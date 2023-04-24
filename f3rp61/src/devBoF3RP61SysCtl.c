/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBoF3RP61SysCtl.c - Device Support Routines for F3RP61 Binary Output
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
#include <boRecord.h>

#include <drvF3RP61SysCtl.h>

// Create the dset for devBoF3RP61SysCtl
static long init_record();
static long write_bo();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_bo;
    DEVSUPFUN  special_linconv;
} devBoF3RP61SysCtl = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    write_bo,
    NULL
};

epicsExportAddress(dset, devBoF3RP61SysCtl);

typedef struct {
    char device;
    char led;
} F3RP61SysCtl_BO_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(boRecord *precord)
{
    char device = 0; // Valid states: L (LED), U (User-LED; F3RP71 only)
    char led = 0;    // Valid states: R (Run), A (Alarm), E (Error)

    // Link type must be INST_IO
    if (precord->out.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devBoF3RP61SysCtl (init_record) Illegal OUT field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->out;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse 'device' and possibly 'led'
    if (sscanf(buf, "SYS,%c%c,", &device, &led) < 2) {
        if (sscanf(buf, "SYS,%c", &device) < 1) {
            errlogPrintf("devBoF3RP61SysCtl: can't get device for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Allocate private data storage area
    F3RP61SysCtl_BO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SysCtl_BO_DPVT), "calloc failed");
    dpvt->device = device;

    // Check device (and LED) validity
    if (0) {                                     // dummy

    } else if (device == 'L') {                  // LED
        if (led != 'R' && led != 'A' && led != 'E') {
            errlogPrintf("devBoF3RP61SysCtl: unsupported LED address \'%c\' for %s\n", device, precord->name);
            precord->pact = 1;
            return -1;
        }

#ifdef M3SC_LED_US3_ON // it is assumed that US1 and US2 are also defined
    } else if (device == 'U') {                  // User-LED
        if (led != '1' && led != '2' && led != '3') {
            errlogPrintf("devBoF3RP61SysCtl: unsupported LED address \'%c\' for %s\n", device, precord->name);
            precord->pact = 1;
            return -1;
        }
#endif

    } else {
        errlogPrintf("devBoF3RP61SysCtl: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    dpvt->led = led; // for 'L' and 'U' devices
    precord->dpvt = dpvt;

    return 0;
}

// write_bo() is called when there was a request to process a record.
// When called, it sends the value from the VAL field to the driver.
static long write_bo(boRecord *precord)
{
    F3RP61SysCtl_BO_DPVT *dpvt = precord->dpvt;
    const char device = dpvt->device;
    const char led = dpvt->led;

    // Data to write
    unsigned long data = 0;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'L') { // LED
        if (led == 'R') {
            data = (precord->val) ? M3SC_LED_RUN_ON : M3SC_LED_RUN_OFF;
        } else if (led == 'A') {
            data = (precord->val) ? M3SC_LED_ALM_ON : M3SC_LED_ALM_OFF;
        } else {//(led == 'E')
            data = (precord->val) ? M3SC_LED_ERR_ON : M3SC_LED_ERR_OFF;
        }
        if (ioctl(f3rp61SysCtl_fd, M3SC_SET_LED, &data) < 0) {
            errlogPrintf("devBoF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }

#ifdef M3SC_LED_US3_ON // it is assumed that US1 and US2 are also defined
    } else if (device == 'U') { // User-LED
        if (led == '1') {
            data = (precord->val) ? M3SC_LED_US1_ON : M3SC_LED_US1_OFF;
        } else if (led == '2') {
            data = (precord->val) ? M3SC_LED_US2_ON : M3SC_LED_US2_OFF;
        } else {//(led == '3')
            data = (precord->val) ? M3SC_LED_US3_ON : M3SC_LED_US3_OFF;
        }
        if (ioctl(f3rp61SysCtl_fd, M3SC_SET_US_LED, &data) < 0) {
            errlogPrintf("devBoF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
#endif

    } else {                    // this may not happen

    }

    //
    precord->udf = FALSE;

    return 0;
}
