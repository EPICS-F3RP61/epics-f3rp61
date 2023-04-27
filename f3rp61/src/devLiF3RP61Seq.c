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
#include <recGbl.h>
#include <recSup.h>
#include <longinRecord.h>

#include <drvF3RP61Seq.h>
#include <devF3RP61bcd.h>

// Create the dset for devLiF3RP61Seq
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

// init_record() initializes record - parses INP/OUT field string,
// allocates private data storage area and sets initial configuration
// values.
static long init_record(longinRecord *precord)
{
    int srcSlot = 0, destSlot = 0, top = 0;
    char device = 0;
    char option = 'W'; // Dummy option for Word access

    // Link type must be INST_IO
    if (precord->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, precord,
                          "devLiF3RP61Seq (init_record) Illegal INP field");
        precord->pact = 1;
        return S_db_badField;
    }

    struct link *plink = &precord->inp;
    int   size = strlen(plink->value.instio.string) + 1; // + 1 for terminating null character
    char *buf  = callocMustSucceed(size, sizeof(char), "calloc failed");
    strncpy(buf, plink->value.instio.string, size);
    buf[size - 1] = '\0';

    // Parse option
    char *popt = strchr(buf, '&');
    if (popt) {
        *popt++ = '\0';
        if (sscanf(popt, "%c", &option) < 1) {
            errlogPrintf("devLiF3RP61Seq: can't get option for %s\n", precord->name);
            precord->pact = 1;
            return -1;
        }

        if (option == 'W') {        // Dummy option for Word access
        } else if (option == 'B') { // Binary Coded Decimal format
        } else if (option == 'U') { // Unsigned integer
        } else if (option == 'L') { // Long word
        } else {                    // Option not recognized
            errlogPrintf("devLiF3RP61Seq: unsupported option \'%c\' for %s\n", option, precord->name);
            precord->pact = 1;
            return -1;
        }
    }

    // Parse slot, device and register number
    if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
        errlogPrintf("devLiF3RP61Seq: can't get device address for %s\n", precord->name);
        precord->pact = 1;
        return -1;
    }

    // Check device validity
    switch (device)
    {
    case 'D': // data registers
    case 'B': // file registers
    case 'F': // cache registers
    case 'Z': // special registers
    case 'I': // internal relays
        break;
    default:
        errlogPrintf("devLiF3RP61Seq: unsupported device \'%c\' for %s\n", device, precord->name);
        precord->pact = 1;
        return -1;
    }

    // Read the slot number of CPU module
    if (ioctl(f3rp61Seq_fd, M3CPU_GET_NUM, &srcSlot) < 0) {
        errlogPrintf("devLiF3RP61Seq: ioctl failed [%d] for %s\n", errno, precord->name);
        precord->pact = 1;
        return -1;
    }

    // Allocate private data storage area
    F3RP61_SEQ_DPVT *dpvt = callocMustSucceed(1, sizeof(F3RP61_SEQ_DPVT), "calloc failed");
    dpvt->option = option;

    // Compose data structure for I/O request to CPU module
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
    pM3ReadSeqdev->accessType = kWord;
    if (option == 'L') {
        pM3ReadSeqdev->dataNum = 2;
    } else {
        pM3ReadSeqdev->dataNum = 1;
    }
    pM3ReadSeqdev->devType = device - '@'; // 'D'=>0x04, 'B'=>0x02, 'F'=>0x06, 'Z'=>0x1A, 'I'=>0x09
    pM3ReadSeqdev->topDevNo = top;

    //
    callbackSetUser(precord, &dpvt->callback);
    precord->dpvt = dpvt;

    return 0;
}

// read_longin() is called when there was a request to process a record.
// When called, it reads the value from the driver and stores to the
// VAL field, then sets PACT field back to TRUE.
static long read_longin(longinRecord *precord)
{
    F3RP61_SEQ_DPVT *dpvt = precord->dpvt;

    if (precord->pact) { // Second call (PACT is TRUE)
        if (dpvt->ret < 0) {
            errlogPrintf("devLiF3RP61Seq: read_longin failed for %s\n", precord->name);
            return -1;
        }

        MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
        MCMD_RESPONSE *pmcmdResponse = &pmcmdStruct->mcmdResponse;
        uint16_t *wdata = pmcmdResponse->dataBuff.wData;

        if (pmcmdResponse->errorCode) {
            errlogPrintf("devLiF3RP61Seq: errorCode 0x%04x returned for %s\n", pmcmdResponse->errorCode, precord->name);
            return -1;
        }

        //
        precord->udf = FALSE;

        // fill VAL field
        const char option = dpvt->option;
        if (option == 'B') {
            precord->val = devF3RP61bcd2int(wdata[0], precord);

        } else if (option == 'L') {
            precord->val = wdata[1]<<16 | wdata[0];

        } else if (option == 'U') {
            precord->val = (uint16_t)wdata[0];

        } else {
            precord->val = (int16_t)wdata[0];

        }

    } else { // First call (PACT is still FALSE)
        // Issue read request
        if (f3rp61Seq_queueRequest(dpvt) < 0) {
            errlogPrintf("devLiF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", precord->name);
            return -1;
        }

        precord->pact = 1;
    }

    return 0;
}
