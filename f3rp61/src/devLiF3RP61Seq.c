/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devLiF3RP61Seq.c - Device Support Routines for  F3RP61 Long Input
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
#include "longinRecord.h"
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

/* Create the dset for devLiF3RP61Seq */
static long init_record();
static long read_longin();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_longin;
}devLiF3RP61Seq={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_longin
};
epicsExportAddress(dset,devLiF3RP61Seq);



static long init_record(longinRecord *plongin)
{
  struct link *plink = &plongin->inp;
  int size;
  char *buf;
  F3RP61_SEQ_DPVT *dpvt;
  MCMD_STRUCT *pmcmdStruct;
  MCMD_REQUEST *pmcmdRequest;
  M3_READ_SEQDEV *pM3ReadSeqdev;
  int srcSlot, destSlot, top;
  char device;

  if (plongin->inp.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)plongin,
		      "devLiF3RP61Seq (init_record) Illegal INP field");
    plongin->pact = 1;
    return(S_db_badField);
  }
  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
    errlogPrintf("devLiF3RP61Seq: can't get device addresses for %s\n",
		 plongin->name);
    plongin->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_SEQ_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61_SEQ_DPVT),
					      "calloc failed");

  if (ioctl(f3rp61_fd, M3IO_GET_MYCPUNO, &srcSlot) < 0) {
    errlogPrintf("devLiF3RP61Seq: ioctl failed [%d]\n", errno);
    plongin->pact = 1;
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
  pmcmdRequest->subCode = 0x01;
  pmcmdRequest->dataSize = 10;
  pM3ReadSeqdev = (M3_READ_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
  pM3ReadSeqdev->accessType = 2;
  switch (device)
    {
    case 'D':
      pM3ReadSeqdev->devType = 0x04;
      break;
    case 'B':
      pM3ReadSeqdev->devType = 0x02;
      break;
    default:
      errlogPrintf("devLiF3RP61Seq: unsupported device in %s\n", plongin->name);
      plongin->pact = 1;
      return (-1);
    }
  pM3ReadSeqdev->dataNum = 1;
  pM3ReadSeqdev->topDevNo = top;
  callbackSetUser(plongin, &dpvt->callback);

  plongin->dpvt = dpvt;

  return(0);
}



static long read_longin(longinRecord *plongin)
{
  F3RP61_SEQ_DPVT *dpvt = (F3RP61_SEQ_DPVT *) plongin->dpvt;
  MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
  MCMD_RESPONSE *pmcmdResponse;

  if (plongin->pact) {
    pmcmdResponse = &pmcmdStruct->mcmdResponse;

    if (dpvt->ret < 0) {
      errlogPrintf("devLiF3RP61Seq: read_longin failed for %s\n", plongin->name);
      return (-1);
    }

    if (pmcmdResponse->errorCode) {
      errlogPrintf("devLiF3RP61Seq: errorCode %d returned for %s\n",
		   pmcmdResponse->errorCode, plongin->name);
      return (-1);
    }

    plongin->val = (unsigned long) pmcmdResponse->dataBuff.wData[0];

    plongin->udf=FALSE;
  }
  else {
    if (f3rp61Seq_queueRequest(dpvt) < 0) {
      errlogPrintf("devLiF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n",
		   plongin->name);
      return (-1);
    }

    plongin->pact = 1;
  }

  return(0);
}
