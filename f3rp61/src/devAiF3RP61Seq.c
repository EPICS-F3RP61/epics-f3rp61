/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAiF3RP61Seq.c - Device Support Routines for F3RP61 Analog Input
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
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
#include <aiRecord.h>

#include <drvF3RP61Seq.h>

/* Create the dset for devAiF3RP61Seq */
static long init_record();
static long read_ai();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_ai;
    DEVSUPFUN  special_linconv;
} devAiF3RP61Seq = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    read_ai,
    NULL
};

epicsExportAddress(dset, devAiF3RP61Seq);

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(aiRecord *pai)
{
    int srcSlot = 0, destSlot = 0, top = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    /* Link type must be INST_IO */
    if (pai->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, pai,
                          "devAiF3RP61Seq (init_record) Illegal INP field");
        pai->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pai->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse option */
    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devLiF3RP61Seq: can't get option for %s\n", pai->name);
            pai->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'D') { // Double precision floating point
        } else if (option == 'F') { // Single precision floating point
        } else if (option == 'L') { // Long word
        } else if (option == 'U') { // Unsigned integer
        } else {                    // Option not recognized
            errlogPrintf("devLiF3RP61Seq: unsupported option \'%c\' for %s\n", option, pai->name);
            pai->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devAiF3RP61Seq: can't get device address for %s\n", pai->name);
        pai->pact = 1;
        return -1;
    }

    /* Read the slot number of CPU module */
    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devAiF3RP61Seq: ioctl failed [%d] for %s\n", errno, pai->name);
        pai->pact = 1;
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
        errlogPrintf("devAiF3RP61Seq: unsupported device \'%c\' for %s\n", device, pai->name);
        pai->pact = 1;
        return -1;
    }

    switch (option) {
    case 'D':
        pM3ReadSeqdev->accessType = 4;
        pM3ReadSeqdev->dataNum = 2;
        break;
    case 'L':
    case 'F':
        pM3ReadSeqdev->accessType = 4;
        pM3ReadSeqdev->dataNum = 1;
        break;
    default:
        pM3ReadSeqdev->accessType = 2;
        pM3ReadSeqdev->dataNum = 1;
    }

    pM3ReadSeqdev->topDevNo = top;
    callbackSetUser(pai, &dpvt->callback);

    pai->dpvt = dpvt;

    return 0;
}

/*
  read_ai() is called when there was a request to process a record.
  When called, it reads the value from the driver and stores to the
  VAL field, then sets PACT field back to TRUE.
 */
static long read_ai(aiRecord *pai)
{
    F3RP61_SEQ_DPVT *dpvt = pai->dpvt;

    if (pai->pact) { // Second call (PACT is TRUE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devAiF3RP61Seq: read_ai failed for %s\n", pai->name);
            return -1;
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devAiF3RP61Seq: errorCode %d returned for %s\n", pmcmdResponse->errorCode, pai->name);
            return -1;
        }

        /* fill VAL field */
        pai->udf = FALSE;
        const char option = dpvt->option;
        if (option == 'D') {
            uint64_t l0 = pmcmdResponse->dataBuff.lData[0];
            uint64_t l1 = pmcmdResponse->dataBuff.lData[1];
            uint64_t lval = l1 << 32 | l0;
            double val;
            memcpy(&val, &lval, sizeof(double));

            // todo : consider ASLO and AOFF field
            // todo : consider SMOO field
            pai->val = val;
            pai->udf = isnan(pai->val);
            return 2; // no conversion
        } else if (option == 'F') {
            float val;
            memcpy(&val, pmcmdResponse->dataBuff.lData, sizeof(float));

            // todo : consider ASLO and AOFF field
            // todo : consider SMOO field
            pai->val = val;
            pai->udf = isnan(val);
            return 2; // no conversion
        } else if (option == 'L') {
            pai->rval = (int32_t)pmcmdResponse->dataBuff.lData[0];
        } else if (option == 'U') {
            pai->rval = (uint16_t)pmcmdResponse->dataBuff.wData[0];
        } else {
            pai->rval = (int16_t)pmcmdResponse->dataBuff.wData[0];
        }

    } else { // First call (PACT is still FALSE)
        /* Issue read request */
        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devAiF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", pai->name);
            return -1;
        }

        pai->pact = 1;
    }

    return 0; // convert
}
