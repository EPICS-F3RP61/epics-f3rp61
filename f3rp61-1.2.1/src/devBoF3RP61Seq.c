/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devBoF3RP61Seq.c - Device Support Routines for  F3RP61 Binary Output
*
*      Author: Jun-ichi Odagiri 
*      Date: 31-03-09
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbScan.h"
#include "callback.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "boRecord.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsExport.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/fam3rtos/m3iodrv.h>
#include <asm/fam3rtos/m3mcmd.h>
#include "drvF3RP61Seq.h"

extern int f3rp61_fd;

/* Create the dset for devBoF3RP61Seq */
static long init_record();
static long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoF3RP61Seq={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo
};
epicsExportAddress(dset,devBoF3RP61Seq);



static long init_record(boRecord *pbo)
{
  struct link *plink = &pbo->out;
  int size;
  char *buf;
  F3RP61_SEQ_DPVT *dpvt;
  MCMD_STRUCT *pmcmdStruct;
  MCMD_REQUEST *pmcmdRequest;
  M3_WRITE_SEQDEV *pM3WriteSeqdev;
  int srcSlot, destSlot, top;
  char device;

  if (pbo->out.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pbo,
		      "devBoF3RP61Seq (init_record) Illegal OUT field");
    pbo->pact = 1;
    return(S_db_badField);
  }
  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
    errlogPrintf("devBoF3RP61Seq: can't get device addresses for %s\n", pbo->name);
    pbo->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_SEQ_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61_SEQ_DPVT),
					      "calloc failed");

  if (ioctl(f3rp61_fd, M3IO_GET_MYCPUNO, &srcSlot) < 0) {
    errlogPrintf("devBoF3RP61Seq: ioctl failed [%d]\n", errno);
    pbo->pact = 1;
    return (-1);
  }
  pmcmdStruct = &dpvt->mcmdStruct;
  pmcmdStruct->timeOut = 1;
  pmcmdRequest = &pmcmdStruct->mcmdRequest;
  pmcmdRequest->formatCode = 0xf1;
  pmcmdRequest->responseOption = 1;
  pmcmdRequest->srcSlot = (unsigned char) srcSlot;
  pmcmdRequest->destSlot = (unsigned char) destSlot;
  pmcmdRequest->mainCode = 0x26;
  pmcmdRequest->subCode = 0x02;
  pmcmdRequest->dataSize = 12;
  pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
  pM3WriteSeqdev->accessType = 0;
  switch (device)
    {
    case 'I':
      pM3WriteSeqdev->devType = 0x09;
      break;
    default:
      errlogPrintf("devAiF3RP61Seq: unsupported device in %s\n", pbo->name);
      pbo->pact = 1;
      return (-1);
    }
  pM3WriteSeqdev->devType = 0x09;
  pM3WriteSeqdev->dataNum = 1;
  pM3WriteSeqdev->topDevNo = top;
  callbackSetUser(pbo, &dpvt->callback);

  pbo->dpvt = dpvt;

  return(0);
}



static long write_bo(boRecord *pbo)
{
  F3RP61_SEQ_DPVT *dpvt = (F3RP61_SEQ_DPVT *) pbo->dpvt;
  MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
  MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
  MCMD_RESPONSE *pmcmdResponse;
  M3_WRITE_SEQDEV *pM3WriteSeqdev;

  if (pbo->pact) {
    pmcmdResponse = &pmcmdStruct->mcmdResponse;
    /*
    printf("devBoF3RP61Seq: callback for %s\n", pbo->name);
    */
    if (dpvt->ret < 0) {
      errlogPrintf("devBoF3RP61Seq: write_bo failed for %s\n", pbo->name);
      return (-1);
    }

    if (pmcmdResponse->errorCode) {
      errlogPrintf("devBoF3RP61Seq: errorCode %d returned for %s\n",
		   pmcmdResponse->errorCode, pbo->name);
      return (-1);
    }

    pbo->udf=FALSE;
  }
  else {
    pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
    pM3WriteSeqdev->dataBuff.wData[0] = (unsigned short) pbo->rval;

    if (f3rp61Seq_queueRequest(dpvt) < 0) {
      errlogPrintf("devBoF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n", pbo->name);
      return (-1);
    }
    /*
    printf("devBoF3RP61Seq: request queued for %s\n", pbo->name);
    */

    pbo->pact = 1;
  }

  return (0);
}
