/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiF3RP61Seq.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
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
#include <mbbiRecord.h>

#include <drvF3RP61Seq.h>

/* Create the dset for devMbbiF3RP61Seq */
static long init_record();
static long read_mbbi();

struct {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  read_mbbi;
} devMbbiF3RP61Seq = {
    5,
    NULL,
    NULL,
    init_record,
    NULL,
    read_mbbi
};

epicsExportAddress(dset, devMbbiF3RP61Seq);

/* Function init_record initializes record - parses INP/OUT field string,
 * allocates private data storage area with sets initial values */
static long init_record(mbbiRecord *pmbbi)
{
    int srcSlot, destSlot, top;
    char device;

    /* Input link type must be INST_IO */
    if (pmbbi->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, pmbbi,
                          "devMbbiF3RP61Seq (init_record) Illegal INP field");
        pmbbi->pact = 1;
        return (S_db_badField);
    }

    struct link *plink = &pmbbi->inp;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse device */
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devMbbiF3RP61Seq: can't get device address for %s\n",
                     pmbbi->name);
        pmbbi->pact = 1;
        return (-1);
    }

    F3RP61_SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SEQ_DPVT), "calloc failed");

    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devMbbiF3RP61Seq: ioctl failed [%d]\n", errno);
        pmbbi->pact = 1;
        return (-1);
    }

    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
    pmcmdStruct->timeOut = 1;

    MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
    pmcmdRequest->formatCode = 0xf1;
    pmcmdRequest->responseOption = 1;
    pmcmdRequest->srcSlot = (unsigned char) srcSlot;
    pmcmdRequest->destSlot = (unsigned char) destSlot;
    pmcmdRequest->mainCode = 0x26;
    pmcmdRequest->subCode = 0x01;
    pmcmdRequest->dataSize = 10;

    M3_READ_SEQDEV *pM3ReadSeqdev = (M3_READ_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
    pM3ReadSeqdev->accessType = 2;

    /* Check device validity */
    switch (device)
    {
    case 'D':
        pM3ReadSeqdev->devType = 0x04;
        break;
    case 'B':
        pM3ReadSeqdev->devType = 0x02;
        break;
    default:
        errlogPrintf("devMbbiF3RP61Seq: unsupported device in %s\n",
                 pmbbi->name);
        pmbbi->pact = 1;
        return (-1);
    }

    pM3ReadSeqdev->dataNum = 1;
    pM3ReadSeqdev->topDevNo = top;
    callbackSetUser(pmbbi, &dpvt->callback);

    pmbbi->dpvt = dpvt;

    return (0);
}

/* Function is called when there was a request to process a record.
 * When called, it sets a callback function and sets PACT field to TRUE.
 * When called again, stores the value returned by the driver to RVAL field
 * and PACT field back to FALSE.
 *  */
static long read_mbbi(mbbiRecord *pmbbi)
{
    F3RP61_SEQ_DPVT *dpvt = pmbbi->dpvt;
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;

    if (pmbbi->pact) {  /* Second call; PACT is set to TRUE, so this is a completion request */
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devMbbiF3RP61Seq: read_mbbi failed for %s\n",
                         pmbbi->name);
            return (-1);
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devMbbiF3RP61Seq: errorCode %d returned for %s\n",
                         pmcmdResponse->errorCode, pmbbi->name);
            return (-1);
        }

        pmbbi->rval = (unsigned long) pmcmdResponse->dataBuff.wData[0];

        pmbbi->udf = FALSE;
    }
    else {  /* First call - PACT is set to FALSE */
        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devMbbiF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n",
                         pmbbi->name);
            return (-1);
        }

        pmbbi->pact = 1;
    }

    return (0);
}
