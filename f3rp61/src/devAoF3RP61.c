/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
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
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>

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
#include <aoRecord.h>

#include <drvF3RP61.h>

/* Create the dset for devAoF3RP61 */
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
    NULL,
    init_record,
    f3rp61GetIoIntInfo,
    write_ao,
    NULL
};

epicsExportAddress(dset, devAoF3RP61);

typedef struct {
    IOSCANPVT ioscanpvt; /* must come first */
    union {
        M3IO_ACCESS_COM acom;
        M3IO_ACCESS_REG drly;
    } u;
    char device;
    char option;
} F3RP61_AO_DPVT;

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(aoRecord *pao)
{
    int unitno = 0, slotno = 0, cpuno = 0, start = 0;
    char device = 0;
    char option = 'W';

    /* Link type must be an INST_IO */
    if (pao->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pao,
                          "devAoF3RP61 (init_record) Illegal OUT field");
        pao->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pao->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse 'option' */
    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devAoF3RP61: can't get option for %s\n", pao->name);
            pao->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'D') { // Double precision floating point
        } else if (option == 'F') { // Single precision floating point
        } else if (option == 'L') { // Long word
        } else if (option == 'U') { // Unsigned integer, perhaps we'd better disable this
        } else {                    // Option not recognized
            errlogPrintf("devAoF3RP61: unsupported option \'%c\' for %s\n", option, pao->name);
            pao->pact = 1;
            return -1;
        }
    }

    /* Parse for possible interrupt source */
    pC = strchr(buf, ':');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
            errlogPrintf("devAoF3RP61: can't get interrupt source address for %s\n", pao->name);
            pao->pact = 1;
            return -1;
        }
        if (f3rp61_register_io_interrupt((dbCommon *) pao, unitno, slotno, start) < 0) {
            errlogPrintf("devAoF3RP61: can't register I/O interrupt for %s\n", pao->name);
            pao->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
        if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
            if (sscanf(buf, "%c%d", &device, &start) < 2) {
                errlogPrintf("devAoF3RP61: can't get I/O address for %s\n", pao->name);
                pao->pact = 1;
                return -1;
            } else if (device != 'W' && device != 'R') {
                errlogPrintf("devAoF3RP61: unsupported device \'%c\' for %s\n", device, pao->name);
                pao->pact = 1;
            }
        } else {
            device = 'r';
        }
    }

    /* Allocate private data storage area */
    F3RP61_AO_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_AO_DPVT), "calloc failed");
    dpvt->device = device;
    dpvt->option = option;

    /* Check device validity and compose data structure for I/O request */
    if (device == 'r') {                         // Shared registers - Using 'Old' interface
        //if (option == 'L' || option == 'F' || option == 'D') {
        //    errlogPrintf("devAiF3RP61: unsupported option \'%c\' for %s\n", option, pai->name);
        //    pai->pact = 1;
        //    return -1;
        //}
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->cpuno = cpuno;
        pacom->start = start;
        pacom->count = 1; // we don't have '&L', '&F', and '&D' option support yet.
    } else if (device == 'R' || device == 'W') { // Shared registers and Link registers
        M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
        pacom->start = start;
        switch (option) {
        case 'D':
            pacom->count = 4;
            break;
        case 'F':
        case 'L':
            pacom->count = 2;
            break;
        default:
            pacom->count = 1;
        }
    } else if (device == 'A') {                  // I/O registers on special modules
        if (option != 'W') {
            errlogPrintf("devAiF3RP61: unsupported option \'%c\' for %s\n", option, pao->name);
            pao->pact = 1;
            return -1;
        }
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        pdrly->count  = 1;
    } else if (device == 'Y') {                  // Output relays on I/O modules
        M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
        pdrly->unitno = unitno;
        pdrly->slotno = slotno;
        pdrly->start  = start;
        if (option == 'D') {
            pdrly->count = 4;
        } else if (option == 'L' || option == 'F') {
            pdrly->count = 2;
        } else {
            pdrly->count = 1;
        }
    } else {
        errlogPrintf("devAoF3RP61: unsupported device \'%c\' for %s\n", device, pao->name);
        pao->pact = 1;
        return -1;
    }

    pao->dpvt = dpvt;

    return 0;
}

/*
  write_ao() is called when there was a request to process a
  record. When called, it sends the value from the VAL field to the
  driver.
*/
static long write_ao(aoRecord *pao)
{
    F3RP61_AO_DPVT *dpvt = pao->dpvt;
    M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
    M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
    char device = dpvt->device;
    char option = dpvt->option;
    int command = M3IO_WRITE_REG;
    unsigned short wdata[4] = {0};
    unsigned long ldata = 0;
    void *p = pdrly;

    /* Compose ioctl request */
    switch (device) {
    case 'W':
    case 'R':
        if (option == 'D') {
            double val = pao->val;
            // todo : consider ASLO and AOFF field

            int64_t lval;
            memcpy(&lval, &val, sizeof(double));
            wdata[0] = (lval>> 0) & 0x0000FFFF;
            wdata[1] = (lval>>16) & 0x0000FFFF;
            wdata[2] = (lval>>32) & 0x0000FFFF;
            wdata[3] = (lval>>48) & 0x0000FFFF;

            pao->udf = isnan(pao->val);
        } else if (option == 'F') {
            float val = pao->val;
            // todo : consider ASLO and AOFF field

            int32_t lval;
            memcpy(&lval, &val, sizeof(float));

            wdata[0] = (lval>> 0) & 0x0000FFFF;
            wdata[1] = (lval>>16) & 0x0000FFFF;

            pao->udf = isnan(pao->val);
        } else if (option == 'L') {
            wdata[0] = (uint16_t)(pao->rval>> 0);
            wdata[1] = (uint16_t)(pao->rval>>16);
        } else {
            wdata[0] = (uint16_t)pao->rval;
        }
        break;
    case 'Y':
        command = M3IO_WRITE_OUTRELAY;
        if (option == 'D') {
            double val = pao->val;
            // todo : consider ASLO and AOFF field

            int64_t lval;
            memcpy(&lval, &val, sizeof(double));

            pdrly->u.outrly[0].data = (lval>> 0) & 0x0000FFFF;
            pdrly->u.outrly[0].mask = 0xFFFF;
            pdrly->u.outrly[1].data = (lval>>16) & 0x0000FFFF;
            pdrly->u.outrly[1].mask = 0xFFFF;
            pdrly->u.outrly[2].data = (lval>>32) & 0x0000FFFF;
            pdrly->u.outrly[2].mask = 0xFFFF;
            pdrly->u.outrly[3].data = (lval>>48) & 0x0000FFFF;
            pdrly->u.outrly[3].mask = 0xFFFF;

            pao->udf = isnan(pao->val);
        } else if (option == 'F') {
            float val = pao->val;
            // todo : consider ASLO and AOFF field

            int32_t lval;
            memcpy(&lval, &val, sizeof(float));

            pdrly->u.outrly[0].data = (lval>> 0) & 0x0000FFFF;
            pdrly->u.outrly[0].mask = 0xFFFF;
            pdrly->u.outrly[1].data = (lval>>16) & 0x0000FFFF;
            pdrly->u.outrly[1].mask = 0xFFFF;

            pao->udf = isnan(pao->val);
        } else if (option == 'L') {
            pdrly->u.outrly[0].data = (uint16_t)(pao->rval>> 0);
            pdrly->u.outrly[0].mask = 0xFFFF;
            pdrly->u.outrly[1].data = (uint16_t)(pao->rval>>16);
            pdrly->u.outrly[1].mask = 0xFFFF;
        } else {
            pdrly->u.outrly[0].data = (uint16_t)pao->rval;
            pdrly->u.outrly[0].mask = 0xFFFF;
        }
        break;
    case 'r':
        command = M3IO_WRITE_COM;
        wdata[0] = (unsigned short) pao->rval;
        pacom->pdata = &wdata[0];
        p = pacom;
        break;
    default:
        switch (option) {
        case 'L':
            command = M3IO_WRITE_REG_L;
            ldata = (unsigned long) pao->rval;
            pdrly->u.pldata = &ldata;
            break;
        default:
            wdata[0] = (uint16_t)pao->rval;
            pdrly->u.pwdata = &wdata[0];
        }
    }

    /* Issue API function */
    if (device == 'R') {        // Shared registers
        if (writeM3ComRegister(pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAoF3RP61: writeM3ComRegister failed [%d] for %s\n", errno, pao->name);
            return -1;
        }
    } else if (device == 'W') { // Link registers
        if (writeM3LinkRegister(pacom->start, pacom->count, &wdata[0]) < 0) {
            errlogPrintf("devAoF3RP61: writeM3LinkRegister failed [%d] for %s\n", errno, pao->name);
            return -1;
        }
    } else {
        if (ioctl(f3rp61_fd, command, p) < 0) {
            errlogPrintf("devAoF3RP61: ioctl failed [%d] for %s\n", errno, pao->name);
            return -1;
        }
    }

    pao->udf = FALSE;

    return 0;
}
