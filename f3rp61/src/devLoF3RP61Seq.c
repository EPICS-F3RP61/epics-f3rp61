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

/* */
static long init_record(longoutRecord *plongout)
{
    int srcSlot, destSlot, top;
    char device;
    char option;
    int  bcd = 0;

    if (plongout->out.type != INST_IO) {
        recGblRecordError(S_db_badField, plongout,
                          "devLoF3RP61Seq (init_record) Illegal OUT field");
        plongout->pact = 1;
        return (S_db_badField);
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
            return (-1);
        }

        if (option == 'B') { /* Binary Coded Decimal format flag */
            bcd = 1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devLoF3RP61Seq: can't get device address for %s\n",
                     plongout->name);
        plongout->pact = 1;
        return (-1);
    }

    F3RP61_SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SEQ_DPVT), "calloc failed");

    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devLoF3RP61Seq: ioctl failed [%d]\n", errno);
        plongout->pact = 1;
        return (-1);
    }

    dpvt->bcd = bcd;

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
        errlogPrintf("devLoF3RP61Seq: unsupported device in %s\n", plongout->name);
        plongout->pact = 1;
        return (-1);
    }

    pM3WriteSeqdev->dataNum = 1;
    pM3WriteSeqdev->topDevNo = top;
    callbackSetUser(plongout, &dpvt->callback);

    plongout->dpvt = dpvt;

    return (0);
}

static long write_longout(longoutRecord *plongout)
{
    F3RP61_SEQ_DPVT *dpvt = plongout->dpvt;
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
    unsigned short dataBCD = 0;  /* For storing the value decoded from binary-coded-decimal format */
    int bcd = dpvt->bcd;

    if (plongout->pact) {
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devLoF3RP61Seq: write_longout failed for %s\n", plongout->name);
            return (-1);
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devLoF3RP61Seq: errorCode %d returned for %s\n",
                         pmcmdResponse->errorCode, plongout->name);
            return (-1);
        }

        plongout->udf = FALSE;
    }
    else {
        if (bcd) {
            /* Encode decimal to BCD */
            unsigned short i = 0;
            long data_temp = (long) plongout->val;
            /* Check data range */
            if (data_temp > 9999) {
                data_temp = 9999;
                recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
            }
            else if (data_temp < 0) {
                data_temp = 0;
                recGblSetSevr(plongout, HW_LIMIT_ALARM, INVALID_ALARM);
            }

            while (data_temp > 0) {
                dataBCD = dataBCD | (((unsigned long) (data_temp % 10)) << (i*4));
                data_temp /= 10;
                i++;
            }
        }

        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
        if (bcd) {
            pM3WriteSeqdev->dataBuff.wData[0] = dataBCD;
        }
        else {
            pM3WriteSeqdev->dataBuff.wData[0] = (unsigned short) plongout->val;
        }

        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devLoF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n",
                         plongout->name);
            return (-1);
        }

        plongout->pact = 1;
    }

    return (0);
}
