/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBiF3RP61SysCtl.c - Device Support Routines for F3RP61 Binary Input
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
#include <biRecord.h>

#include <drvF3RP61SysCtl.h>

// Create the dset for devBiF3RP61SysCtl
static long init_record();
static long read_bi();
struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_bi;
} devBiF3RP61SysCtl = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_bi
};

epicsExportAddress(dset, devBiF3RP61SysCtl);

typedef struct {
    char device;
    char led;
} F3RP61SysCtl_BI_DPVT;

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configure
// values.
static long init_record(biRecord *precord)
{
    char device = 0; // Valid values: L (LED), R (Status Register) or U (User-LED; F3RP71 only)
    char led = 0;    // Valid values: R (Run), A (Alarm), E (Error), 1-3 (F3RP71 only)

    // Link type must be INST_IO
    if (precord->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devBiF3RP61SysCtl (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->inp;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for appending the NULL character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse 'device' and possibly 'led'
    if (sscanf(buf, "SYS,%c%c,", &device, &led) < 2) {
        if (sscanf(buf, "SYS,%c", &device) < 1) {
            errlogPrintf("devBiF3RP61SysCtl: can't get device for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Allocate private data storage area
    F3RP61SysCtl_BI_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SysCtl_BI_DPVT), "calloc failed");
    dpvt->device = device;

    // Check device (and LED) validity
    if (0) {                                     // dummy

    } else if (device == 'R') {                  // Status Register

    } else if (device == 'L') {                  // LED
        if (led != 'R' && led != 'A' && led != 'E' ) {
            errlogPrintf("devBiF3RP61SysCtl: unsupported LED address \'%c\' for %s\n", led, precord->name);
            precord->pact = 1;
            return -1;
        }

#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
    } else if (device == 'U') {                  // User-LED
        if (led != '1' && led != '2' && led != '3') {
            errlogPrintf("devBiF3RP61SysCtl: unsupported USER LED address \'%c\' for %s\n", led, precord->name);
            precord->pact = 1;
            return -1;
        }
#endif

    } else {
        errlogPrintf("devBiF3RP61SysCtl: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    dpvt->led = led; // for 'L' and 'U' devices
    precord->dpvt = dpvt;

    return 0;
}

// read_bi() is called when there was a request to process a
// record. When called, it reads the value from the driver and stores
// to the VAL field.
static long read_bi(biRecord *precord)
{
    F3RP61SysCtl_BI_DPVT *dpvt = (F3RP61SysCtl_BI_DPVT *) precord->dpvt;
    const char device = dpvt->device;
    const char led = dpvt->led;

    // Buffers for data read
    unsigned long data = -1;

    // Issue API function
    if (0) {                    // dummy

    } else if (device == 'L') { // LED
        if (ioctl(f3rp61SysCtl_fd, M3SC_GET_LED, &data) < 0) {
            errlogPrintf("devBiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
        if (led == 'R') {
            precord->rval = (data & LED_RUN_FLG) ? 1 : 0;
        } else if (led == 'A') {
            precord->rval = (data & LED_ALM_FLG) ? 1 : 0;
        } else {/* led == 'E' */
            precord->rval = (data & LED_ERR_FLG) ? 1 : 0;
        }

#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
    } else if (device == 'U') { // User-LED
        if (ioctl(f3rp61SysCtl_fd, M3SC_GET_US_LED, &data) < 0) {
            errlogPrintf("devBiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
        if (led == '1') {
            precord->rval = (data & LED_US1_FLG) ? 1 : 0;
        } else if (led == '2') {
            precord->rval = (data & LED_US2_FLG) ? 1 : 0;
        } else {/* led == '3' */
            precord->rval = (data & LED_US3_FLG) ? 1 : 0;
        }
#endif

    } else {/*(device == 'R')*/ // Status Register
        if (ioctl(f3rp61SysCtl_fd, M3SC_CHECK_BAT, &data) < 0) {
            errlogPrintf("devBiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, precord->name);
            return -1;
        }
        precord->rval = (data & 0x00000004);
    }

    //
    precord->udf = FALSE;

    return 0;
}
