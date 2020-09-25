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

/*
  init_record() initializes record - parses INP/OUT field string,
  allocates private data storage area and sets initial configure
  values.
*/
static long init_record(aoRecord *pao)
{
    int srcSlot = 0, destSlot = 0, top = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    /* Link type must be INST_IO */
    if (pao->out.type != INST_IO) {
        recGblRecordError(S_db_badField, pao,
                          "devAoF3RP61Seq (init_record) Illegal OUT field");
        pao->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &pao->out;
    int   size = strlen(plink->value.instio.string) + 1;
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    /* Parse option */
    char *pC = strchr(buf, '&');
    if (pC) {
        *pC++ = '\0';
        if (sscanf(pC, "%c", &option) < 1) {
            errlogPrintf("devAoF3RP61Seq: can't get option for %s\n", pao->name);
            pao->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'L') { // Long word
        } else if (option == 'U') { // Unsigned integer , perhaps we'd better disable this
        } else if (option == 'F') { // Single precision floating point
        } else if (option == 'D') { // Double precision
        } else {                    // Option not recognized
            errlogPrintf("devAoF3RP61Seq: unsupported option \'%c\' for %s\n", option, pao->name);
            pao->pact = 1;
            return -1;
        }
    }

    /* Parse slot, device and register number */
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devAoF3RP61Seq: can't get device address for %s\n", pao->name);
        pao->pact = 1;
        return -1;
    }

    /* Read the slot number of CPU module */
    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devAoF3RP61Seq: ioctl failed [%d] for %s\n", errno, pao->name);
        pao->pact = 1;
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
    default:
        errlogPrintf("devAoF3RP61Seq: unsupported device \'%c\' for %s\n", device, pao->name);
        pao->pact = 1;
        return -1;
    }

    switch (option) {
    case 'D':
        pM3WriteSeqdev->accessType = 4;
        pM3WriteSeqdev->dataNum = 2;
        break;
    case 'L':
    case 'F':
        pM3WriteSeqdev->accessType = 4;
        pM3WriteSeqdev->dataNum = 1;
        break;
    default:
        pM3WriteSeqdev->accessType = 2;
        pM3WriteSeqdev->dataNum = 1;
    }

    pmcmdRequest->dataSize = 10 + pM3WriteSeqdev->accessType * pM3WriteSeqdev->dataNum;
    pM3WriteSeqdev->topDevNo = top;
    callbackSetUser(pao, &dpvt->callback);

    pao->dpvt = dpvt;

    return 0;
}

/*
  write_ao() is called when there was a request to process a
  record. When called, it sends the value from the VAL filed to the
  driver, then sets PACT field back to TRUE.
 */
static long write_ao(aoRecord *pao)
{
    F3RP61_SEQ_DPVT *dpvt = pao->dpvt;
    MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
    int retval = 0; // convert

    if (pao->pact) { // Second call (PACT is TRUE)
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;

        if (dpvt->ret < 0) {
            errlogPrintf("devAoF3RP61Seq: write_ao failed for %s\n", pao->name);
            return -1;
        }

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devAoF3RP61Seq: errorCode %d returned for %s\n", pmcmdResponse->errorCode, pao->name);
            return -1;
        }

        pao->udf = FALSE;

    } else { // First call (PACT is still FALSE)
        MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
        M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

        const char option = dpvt->option;
        if (option == 'D') {
            double val = pao->val;
            // todo : consider ASLO and AOFF field

            int64_t lval;
            memcpy(&lval, &val, sizeof(double));
            uint32_t l0 = (lval>> 0) & 0xFFFFFFFF;
            uint32_t l1 = (lval>>32) & 0xFFFFFFFF;
            pM3WriteSeqdev->dataBuff.lData[0] = l0;
            pM3WriteSeqdev->dataBuff.lData[1] = l1;

            pao->udf = isnan(val);
            // do we have to return 2?
            //retval = 2; // no conversion
        } else if (option == 'F') {
            float val = pao->val;
            // todo : consider ASLO and AOFF field

            uint32_t lval;
            memcpy(&lval, &val, sizeof(float));
            pM3WriteSeqdev->dataBuff.lData[0] = lval;

            pao->udf = isnan(val);
            // do we have to return 2?
            //retval = 2; // no conversion
        } else if (option == 'L') {
            pM3WriteSeqdev->dataBuff.lData[0] = (int16_t)pao->val;
        } else if (option == 'U') {
            pM3WriteSeqdev->dataBuff.wData[0] = (uint16_t)pao->val;
        } else {
            pM3WriteSeqdev->dataBuff.wData[0] = (int16_t)pao->rval;
            //pM3WriteSeqdev->dataBuff.wData[0] = (unsigned short) pao->rval;
        }

        /* Issue write request */
        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devAoF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", pao->name);
            return -1;
        }

        pao->pact = 1;
    }

    return retval;
}
