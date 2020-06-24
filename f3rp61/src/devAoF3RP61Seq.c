/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devAoF3RP61Seq.c - Device Support Routines for F3RP61 Analog Output
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
#include <aoRecord.h>

#include <drvF3RP61Seq.h>

/* Create the dset for devAoF3RP61Seq */
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
} devAoF3RP61Seq = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    write_ao,
    NULL
};

epicsExportAddress(dset, devAoF3RP61Seq);

/* */
static long init_record(aoRecord *pao)
{
    int srcSlot, destSlot, top;
    char device;

    if (pao->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pao,
                          "devAoF3RP61Seq (init_record) Illegal OUT field");
        pao->pact = 1;
        return (S_db_badField);
    }

    struct link *plink = &pao->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devAoF3RP61Seq: can't get device address for %s\n", pao->name);
        pao->pact = 1;
        return (-1);
    }

    F3RP61_SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SEQ_DPVT), "calloc failed");

    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devAoF3RP61Seq: ioctl failed [%d]\n", errno);
        pao->pact = 1;
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
    pmcmdRequest->subCode = 0x02;
    pmcmdRequest->dataSize = 12;

    M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
    pM3WriteSeqdev->accessType = 2;

    switch (device)
    {
    case 'D':
        pM3WriteSeqdev->devType = 0x04;
        break;
    case 'B':
        pM3WriteSeqdev->devType = 0x02;
        break;
    default:
        errlogPrintf("devAoF3RP61Seq: unsupported device in %s\n", pao->name);
        pao->pact = 1;
        return (-1);
    }

    pM3WriteSeqdev->dataNum = 1;
    pM3WriteSeqdev->topDevNo = top;
    callbackSetUser(pao, &dpvt->callback);

    pao->dpvt = dpvt;

    return (0);
}

static long write_ao(aoRecord *pao)
{
    F3RP61_SEQ_DPVT *dpvt = pao->dpvt;
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
    MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;

    if (pao->pact) {
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devAoF3RP61Seq: write_ao failed for %s\n", pao->name);
            return (-1);
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devAoF3RP61Seq: errorCode %d returned for %s\n",
                         pmcmdResponse->errorCode, pao->name);
            return (-1);
        }

        pao->udf = FALSE;
    }
    else {
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        pM3WriteSeqdev->dataBuff.wData[0] = (unsigned short) pao->rval;

        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devAoF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", pao->name);
            return (-1);
        }

        pao->pact = 1;
    }

    return (0);
}
