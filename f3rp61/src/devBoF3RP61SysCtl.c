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

/* Create the dset for devBoF3RP61SysCtl */
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

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(boRecord *pbo)
{
    char device = 0; // Valid states: L (LED), R (Status Register) or U (F3RP71 only)
    char led = 0;    // Valid states: R (Run), A (Alarm), E (Error)

    /* Link type must be INST_IO */
    if (pbo->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pbo,
                          "devBoF3RP61SysCtl (init_record) Illegal OUT field");
        pbo->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pbo->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse 'device' and possibly 'led' */
    if (sscanf(buf, "SYS,%c%c,", &device, &led) < 2) {
        if (sscanf(buf, "SYS,%c", &device) < 1) {
            errlogPrintf("devBoF3RP61SysCtl: can't get device for %s\n", pbo->name);
            pbo->pact = 1;
            return -1;
        }
    }

    /* Check device validity */
    if (device != 'L'
#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
        && device != 'U'
#endif
        ) {
        errlogPrintf("devBoF3RP61SysCtl: unsupported device \'%c\' for %s\n", device, pbo->name);
        pbo->pact = 1;
        return -1;
    }

    /* Check 'led' validity */
    if (device == 'L') {
        if (!(led == 'R' || led == 'A' || led == 'E')) {
            errlogPrintf("devBoF3RP61SysCtl: unsupported LED address \'%c\' for %s\n", device, pbo->name);
            pbo->pact = 1;
            return -1;
        }
    }
#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
    else if (device == 'U') {
        if (!(led == '1' || led == '2' || led == '3')) {
            errlogPrintf("devBoF3RP61SysCtl: unsupported LED address \'%c\' for %s\n", device, pbo->name);
            pbo->pact = 1;
            return -1;
        }
    }
#endif

    /* Allocate private data storage area */
    F3RP61SysCtl_BO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61SysCtl_BO_DPVT), "calloc failed");
    dpvt->device = device;
    if (device == 'L' || device == 'U') {
        dpvt->led = led;
    }

    pbo->dpvt = dpvt;

    return 0;
}

/*
  write_bo() is called when there was a request to process a
  record. When called, it sends the value from the VAL field to the
  driver.
*/
static long write_bo(boRecord *pbo)
{
    F3RP61SysCtl_BO_DPVT *dpvt = pbo->dpvt;

    char device = dpvt->device;
    char led = dpvt->led;
    int command = 0;
    unsigned long data = 0;

    /* Set command and data */
    switch (device) {
    case 'L':
        command = M3SC_SET_LED;
        if (!(pbo->val)) {  /* When VAL field is 0 */
            switch (led) {
            case 'R':  /* Run LED */
                data = M3SC_LED_RUN_OFF;
                break;
            case 'A':  /* Alarm LED */
                data = M3SC_LED_ALM_OFF;
                break;
            default:   /* For 'E' Error LED */
                data = M3SC_LED_ERR_OFF;
                break;
            }
        } else if (pbo->val == 1) {  /* When VAL field is 1. Should not use only 'else' because then Invalid Value will be treated as True also */
            switch (led) {
            case 'R':  /* Run LED */
                data = M3SC_LED_RUN_ON;
                break;
            case 'A':  /* Alarm LED */
                data = M3SC_LED_ALM_ON;
                break;
            default:   /* For 'E' Error LED */
                data = M3SC_LED_ERR_ON;
                break;
            }
        }
        break;
#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
    case 'U':
        command = M3SC_SET_US_LED;
        if (!(pbo->val)) {  /* When VAL field is 0 */
            switch (led) {
            case '1':  /* US1 LED */
                data = M3SC_LED_US1_OFF;
                break;
            case '2':  /* US2 LED */
                data = M3SC_LED_US2_OFF;
                break;
            default:   /* For '3' US3 LED */
                data = M3SC_LED_US3_OFF;
                break;
            }
        } else if (pbo->val == 1) {  /* When VAL field is 1. Should not use only 'else' because then Invalid Value will be treated as True also */
            switch (led) {
            case '1':  /* US1 LED */
                data = M3SC_LED_US1_ON;
                break;
            case '2':  /* US2 LED */
                data = M3SC_LED_US2_ON;
                break;
            default:   /* For '3' US3 LED */
                data = M3SC_LED_US3_ON;
                break;
            }
        }
        break;
#endif
    default:
        command = M3SC_SET_LED;
        break;
    }

    /* Write to device */
    if (device == 'L' || device == 'U') {
        if (ioctl(f3rp61SysCtl_fd, command, &data) < 0) {
            errlogPrintf("devBoF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, pbo->name);
            return -1;
        }
    }

    pbo->udf = FALSE;

    return 0;
}
