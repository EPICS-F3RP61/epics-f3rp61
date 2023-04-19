/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBiF3RP61Seq.c - Device Support Routines for F3RP61 Binary Input
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
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
#include <biRecord.h>

#include <drvF3RP61Seq.h>

/* Create the dset for devBiF3RP61Seq */
static long init_record();
static long read_bi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_bi;
} devBiF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_bi
};

epicsExportAddress(dset, devBiF3RP61Seq);

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(biRecord *precord)
{
    int srcSlot = 0, destSlot = 0, top = 0;
    char device = 0;

    /* Link type must be INST_IO */
    if (precord->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devBiF3RP61Seq (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse slot, device and register number */
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devBiF3RP61Seq: can't get device address for %s\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    /* Read the slot number of CPU module */
    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devBiF3RP61Seq: ioctl failed [%d] for %s\n", errno, precord->name);
        precord->pact = 1;
        return -1;
    }

    /* Allocate private data storage area */
    F3RP61_SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SEQ_DPVT), "calloc failed");

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
    pM3ReadSeqdev->accessType = 0;

    /* Check device validity and set device type*/
    switch (device)
    {
    case 'I': // internal relays
        pM3ReadSeqdev->devType = 0x09;
        break;
    case 'M': // special relays
        pM3ReadSeqdev->devType = 0x0D;
        break;
    default:
        errlogPrintf("devBiF3RP61Seq: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    pM3ReadSeqdev->dataNum = 1;
    pM3ReadSeqdev->topDevNo = top;
    callbackSetUser(precord, &dpvt->callback);

    precord->dpvt = dpvt;

    return 0;
}

/*
  read_bi() is called when there was a request to process a record.
  When called, it reads the value from the driver and stores to the
  VAL field, then sets PACT field back to TRUE.
*/
static long read_bi(biRecord *precord)
{
    F3RP61_SEQ_DPVT *dpvt = precord->dpvt;

    if (precord->pact) { // Second call (PACT is TRUE)
        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devBiF3RP61Seq: read_bi failed for %s\n", precord->name);
            return -1;
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devBiF3RP61Seq: errorCode %d returned for %s\n", pmcmdResponse->errorCode, precord->name);
            return -1;
        }

        /* fill VAL field */
        precord->udf = FALSE;
        precord->rval = (unsigned long) pmcmdResponse->dataBuff.wData[0];

    } else { // First call (PACT is still FALSE)
        /* Issue read request */
        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devBiF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", precord->name);
            return -1;
        }

        precord->pact = 1;
    }

    return 0;
}
