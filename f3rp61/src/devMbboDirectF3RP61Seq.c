/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devMbboDirectF3RP61Seq.c - Device Support Routines for  F3RP61 Multi-bit
* Binary Output
*
*      Author: Jun-ichi Odagiri 
*      Date: 6-30-08
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
#include "mbboDirectRecord.h"
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

/* Create the dset for devMbboDirectF3RP61Seq */
static long init_record();
static long write_mbboDirect();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbboDirect;
}devMbboDirectF3RP61Seq={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbboDirect
};
epicsExportAddress(dset,devMbboDirectF3RP61Seq);



static long init_record(mbboDirectRecord *pmbboDirect)
{
  struct link *plink = &pmbboDirect->out;
  int size;
  char *buf;
  F3RP61_SEQ_DPVT *dpvt;
  MCMD_STRUCT *pmcmdStruct;
  MCMD_REQUEST *pmcmdRequest;
  M3_WRITE_SEQDEV *pM3WriteSeqdev;
  int srcSlot, destSlot, top;
  char device;

  if (pmbboDirect->out.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pmbboDirect,
		      "devMbboDirectF3RP61Seq (init_record) Illegal OUT field");
    pmbboDirect->pact = 1;
    return(S_db_badField);
  }
  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
    errlogPrintf("devMbboDirectF3RP61Seq: can't get device addresses for %s\n",
		 pmbboDirect->name);
    pmbboDirect->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_SEQ_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61_SEQ_DPVT),
					      "calloc failed");

  if (ioctl(f3rp61_fd, M3IO_GET_MYCPUNO, &srcSlot) < 0) {
    errlogPrintf("devMbboDirectF3RP61Seq: ioctl failed [%d]\n", errno);
    pmbboDirect->pact = 1;
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
      errlogPrintf("devMbboDirectF3RP61Seq: unsupported device in %s\n",
		   pmbboDirect->name);
      pmbboDirect->pact = 1;
      return (-1);
    }
  pM3WriteSeqdev->dataNum = 1;
  pM3WriteSeqdev->topDevNo = top;
  callbackSetUser(pmbboDirect, &dpvt->callback);

  pmbboDirect->dpvt = dpvt;

  return(0);
}



static long write_mbboDirect(mbboDirectRecord *pmbboDirect)
{
  F3RP61_SEQ_DPVT *dpvt = (F3RP61_SEQ_DPVT *) pmbboDirect->dpvt;
  MCMD_STRUCT *pmcmdStruct = &dpvt->mcmdStruct;
  MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
  MCMD_RESPONSE *pmcmdResponse;
  M3_WRITE_SEQDEV *pM3WriteSeqdev;

  if (pmbboDirect->pact) {
    pmcmdResponse = &pmcmdStruct->mcmdResponse;

    if (dpvt->ret < 0) {
      errlogPrintf("devMbboDirectF3RP61Seq: write_mbboDirect failed for %s\n",
		   pmbboDirect->name);
      return (-1);
    }

    if (pmcmdResponse->errorCode) {
      errlogPrintf("devMbboDirectF3RP61Seq: errorCode %d returned for %s\n",
		   pmcmdResponse->errorCode, pmbboDirect->name);
      return (-1);
    }

    pmbboDirect->udf=FALSE;
  }
  else {
    pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];
    pM3WriteSeqdev->dataBuff.wData[0] = (unsigned short) pmbboDirect->rval;

    if (f3rp61Seq_queueRequest(dpvt) < 0) {
      errlogPrintf("devMbboDirectF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n",
		   pmbboDirect->name);
      return (-1);
    }

    pmbboDirect->pact = 1;
  }

  return(0);
}
