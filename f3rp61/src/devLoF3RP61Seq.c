/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devLoF3RP61Seq.c - Device Support Routines for F3RP61 Long Output
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*
*      Modified: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
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
#include <longoutRecord.h>

#include <drvF3RP61Seq.h>

/* Create the dset for devLoF3RP61Seq */
static long init_record();
static long write_longout();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  write_longout;
} devLoF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    write_longout
};

epicsExportAddress(dset, devLoF3RP61Seq);

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(longoutRecord *plongout)
{
    int srcSlot = 0, destSlot = 0, top = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    /* Link type must be INST_IO */
    if (plongout->out.type != INST_IO) {
        recGblRecordError(S_db_badField, plongout,
                          "devLoF3RP61Seq (init_record) Illegal OUT field");
        plongout->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &plongout->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse option */
    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devLoF3RP61Seq: can't get option for %s\n", plongout->name);
            plongout->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'L') { // Long word
        } else if (option == 'U') { // Unsigned integer, perhaps we'd better disable this
        } else if (option == 'B') { // Binary Coded Decimal format
        } else {                    // Option not recognized
            errlogPrintf("devLoF3RP61Seq: unsupported option \'%c\' for %s\n", option, plongout->name);
            plongout->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devLoF3RP61Seq: can't get device address for %s\n", plongout->name);
        plongout->pact = 1;
        return -1;
    }

    /* Read the slot number of CPU module */
    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devLoF3RP61Seq: ioctl failed [%d] for %s\n", errno, plongout->name);
        plongout->pact = 1;
        return -1;
    }

    /* Allocate private data storage area */
    F3RP61_SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SEQ_DPVT), "calloc failed");
    dpvt->option = option;

    /* Compose data structure for I/O request to CPU module */
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
    pmcmdStruct->timeOut = 1;

    MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
    pmcmdRequest->formatCode = 0xf1;
    pmcmdRequest->responseOption = 1;
    pmcmdRequest->srcSlot = srcSlot;
    pmcmdRequest->destSlot = destSlot;
    pmcmdRequest->mainCode = 0x26;
    pmcmdRequest->subCode = 0x02;

    M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

    /* Check device validity and set device type*/
    switch (device)
    {
    case 'D': // data register
        pM3WriteSeqdev->devType = 0x04;
        break;
    case 'B': // file register
        pM3WriteSeqdev->devType = 0x02;
        break;
    case 'F': // cache register
        pM3WriteSeqdev->devType = 0x06;
        break;
    default:
        errlogPrintf("devLoF3RP61Seq: unsupported device \'%c\' for %s\n", device, plongout->name);
        plongout->pact = 1;
        return -1;
    }

    switch (option) {
    case 'L':
        pM3WriteSeqdev->accessType = 4;
        pM3WriteSeqdev->dataNum = 1;
        break;
    default:
        pM3WriteSeqdev->accessType = 2;
        pM3WriteSeqdev->dataNum = 1;
    }

    pmcmdRequest->dataSize = 10 + pM3WriteSeqdev->accessType * pM3WriteSeqdev->dataNum;
    pM3WriteSeqdev->topDevNo = top;
    callbackSetUser(plongout, &dpvt->callback);

    plongout->dpvt = dpvt;

    return 0;
}

/*
  write_longout() is called when there was a request to process a
  record. When called, it sends the value from the VAL filed to the
  driver, then sets PACT field back to TRUE.
 */
static long write_longout(longoutRecord *plongout)
{
    F3RP61_SEQ_DPVT *dpvt = plongout->dpvt;
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;

    if (plongout->pact) { // Second call (PACT is TRUE)
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devLoF3RP61Seq: write_longout failed for %s\n", plongout->name);
            return -1;
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devLoF3RP61Seq: errorCode %d returned for %s\n", pmcmdResponse->errorCode, plongout->name);
            return -1;
        }

        plongout->udf = FALSE;

    } else { // First call (PACT is still FALSE)
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

        const char option = dpvt->option;
        if (option == 'B') {
            /* Encode decimal to BCD */
            unsigned short dataBCD = 0;  /* For storing the value decoded from binary-coded-decimal format */
            unsigned short i = 0;
            long data_temp = (long) plongout->val;
            /* Check data range */
            if (data_temp > 9999) {
                data_temp = 9999;
                recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
            } else if (data_temp < 0) {
                data_temp = 0;
                recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
            }

            while (data_temp > 0) {
                dataBCD = dataBCD | (((unsigned long) (data_temp % 10)) << (i*4));
                data_temp /= 10;
                i++;
            }
            pM3WriteSeqdev->dataBuff.wData[0] = dataBCD;
        } else if (option == 'L') {
            pM3WriteSeqdev->dataBuff.lData[0] = (int32_t)plongout->val;
        } else if (option == 'U') {
            pM3WriteSeqdev->dataBuff.wData[0] = (uint16_t)plongout->val;
        } else {
            pM3WriteSeqdev->dataBuff.wData[0] = (int16_t)plongout->val;
        }

        /* Issue write request */
        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devLoF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", plongout->name);
            return -1;
        }

        plongout->pact = 1;
    }

    return 0;
}
