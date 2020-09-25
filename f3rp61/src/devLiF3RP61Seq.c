/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devLiF3RP61Seq.c - Device Support Routines for F3RP61 Long Input
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
#include <math.h>
#include <recGbl.h>
#include <recSup.h>
#include <longinRecord.h>

#include <drvF3RP61Seq.h>

/* Create the dset for devLiF3RP61Seq */
static long init_record();
static long read_longin();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_longin;
} devLiF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_longin
};

epicsExportAddress(dset, devLiF3RP61Seq);

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(longinRecord *plongin)
{
    int srcSlot = 0, destSlot = 0, top = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    /* Link type must be INST_IO */
    if (plongin->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, plongin,
                          "devLiF3RP61Seq (init_record) Illegal INP field");
        plongin->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &plongin->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse option */
    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devLiF3RP61Seq: can't get option for %s\n", plongin->name);
            plongin->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'L') { // Long word
        } else if (option == 'U') { // Unsigned integer
        } else if (option == 'B') { // Binary Coded Decimal format
        } else {                    // Option not recognized
            errlogPrintf("devLiF3RP61Seq: unsupported option \'%c\' for %s\n", option, plongin->name);
            plongin->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devLiF3RP61Seq: can't get device address for %s\n", plongin->name);
        plongin->pact = 1;
        return -1;
    }

    /* Read the slot number of CPU module */
    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devLiF3RP61Seq: ioctl failed [%d] for %s\n", errno, plongin->name);
        plongin->pact = 1;
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
    pmcmdRequest->subCode = 0x01;
    pmcmdRequest->dataSize = 10;

    M3_READ_SEQDEV *pM3ReadSeqdev = (M3_READ_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

    /* Check device validity and set device type*/
    switch (device)
    {
    case 'D': // data register
        pM3ReadSeqdev->devType = 0x04;
        break;
    case 'B': // file register
        pM3ReadSeqdev->devType = 0x02;
        break;
    default:
        errlogPrintf("devLiF3RP61Seq: unsupported device \'%c\' for %s\n", device, plongin->name);
        plongin->pact = 1;
        return -1;
    }

    switch (option) {
    case 'L':
        pM3ReadSeqdev->accessType = 4;
        pM3ReadSeqdev->dataNum = 1;
        break;
    default:
        pM3ReadSeqdev->accessType = 2;
        pM3ReadSeqdev->dataNum = 1;
    }

    pM3ReadSeqdev->topDevNo = top;
    callbackSetUser(plongin, &dpvt->callback);

    plongin->dpvt = dpvt;

    return 0;
}

/*
  read_longin() is called when there was a request to process a
  record. When called, it reads the value from the driver and stores
  to the VAL field, then sets PACT field back to TRUE.
*/
static long read_longin(longinRecord *plongin)
{
    F3RP61_SEQ_DPVT *dpvt = plongin->dpvt;

    if (plongin->pact) { // Second call (PACT is TRUE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devLiF3RP61Seq: read_longin failed for %s\n", plongin->name);
            return -1;
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devLiF3RP61Seq: errorCode %d returned for %s\n", pmcmdResponse->errorCode, plongin->name);
            return -1;
        }

        /* fill VAL field */
        plongin->udf = FALSE;
        const char option = dpvt->option;
        if (option == 'B') {
            /* Decode BCD to decimal */
            unsigned short i = 0;
            unsigned long dataFromBCD = 0;  /* For storing returned value in binary-coded-decimal format */
            unsigned short data_temp = pmcmdResponse->dataBuff.wData[0];
            while (i < 4) {  /* max is 9999 */
                if (((unsigned short) (0x0000000f & data_temp)) > 9) {
                    dataFromBCD += 9 * pow(10, i);
                    recGblSetSevr(plongin,HIGH_ALARM,INVALID_ALARM);
                } else {
                    dataFromBCD += (unsigned short) ((0x0000000f & data_temp) * pow(10, i));
                }
                data_temp = data_temp >> 4;
                i++;
            }
            plongin->val = dataFromBCD;
        } else if (option == 'L') {
            plongin->val = (int32_t)pmcmdResponse->dataBuff.lData[0];
        } else if (option == 'U') {
            plongin->val = (uint16_t)pmcmdResponse->dataBuff.wData[0];
        } else {
            plongin->val = (int16_t)pmcmdResponse->dataBuff.wData[0];
        }

    } else { // First call (PACT is still FALSE)
        /* Issue read request */
        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devLiF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", plongin->name);
            return -1;
        }

        plongin->pact = 1;
    }

    return 0;
}
