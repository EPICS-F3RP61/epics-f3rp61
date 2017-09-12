/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devLiF3RP61Seq.c - Device Support Routines for  F3RP61 Long Input
*
*      Author: Jun-ichi Odagiri
*      Date: 31-03-09
*
*      Modified: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

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
#if defined(_arm_)
#  include <m3io.h>
#  include <m3lib.h>
#elif defined(_ppc_)
#  include <asm/fam3rtos/m3iodrv.h>
#  include <asm/fam3rtos/m3mcmd.h>
#else
#  error
#endif
#include "drvF3RP61Seq.h"

extern int f3rp61_fd;

/* Create the dset for devLiF3RP61Seq */
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

epicsExportAddress(dset,devLiF3RP61Seq);



static long init_record(longinRecord *plongin)
{
  struct link *plink = &plongin->inp;
  int size;
  char *buf;
  char *pC;
  F3RP61_SEQ_DPVT *dpvt;
  MCMD_STRUCT *pmcmdStruct;
  MCMD_REQUEST *pmcmdRequest;
  M3_READ_SEQDEV *pM3ReadSeqdev;
  int srcSlot, destSlot, top;
  char device;
  char option;
  short BCD = 0;

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

  /* Parse option*/
  pC = strchr(buf, '&');
  if (pC) {
    *pC++ = '\0';
    if (sscanf(pC, "%c", &option) < 1) {
      errlogPrintf("devLiF3RP61Seq: can't get option for %s\n", plongin->name);
      plongin->pact = 1;
      return (-1);
    }
    if (option == 'B') {
      BCD = 1; /* flag is used for the possible double option case */
    }
  }

  /* Parse slot, device and register number*/
  if (sscanf(buf, "CPU%d,%c%d", &destSlot, &device, &top) < 3) {
    errlogPrintf("devLiF3RP61Seq: can't get device addresses for %s\n",
                 plongin->name);
    plongin->pact = 1;
    return (-1);
  }

  /* Allocate private data storage area*/
  dpvt = (F3RP61_SEQ_DPVT *) callocMustSucceed(1,
                                               sizeof(F3RP61_SEQ_DPVT),
                                               "calloc failed");

  /* Read the slot number of CPU module*/
  if (ioctl(f3rp61_fd, M3IO_GET_MYCPUNO, &srcSlot) < 0) {
    errlogPrintf("devLiF3RP61Seq: ioctl failed [%d]\n", errno);
    plongin->pact = 1;
    return (-1);
  }

  dpvt->BCD = BCD;
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
  unsigned long dataFromBCD = 0;  /* For storing returned value in binary-coded-decimal format*/
  unsigned short i, data_temp;  /* Used when calculating BCD value*/
  short BCD = dpvt->BCD;

  if (plongin->pact) {  /* If pact=1 this is a completion request.*/
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

    /* Decode BCD to decimal*/
    if (BCD) {
      i = 0;
      data_temp = pmcmdResponse->dataBuff.wData[0];
      while (i < 4) {  /* max is 9999 */
        if (((unsigned short) (0x0000000f & data_temp)) > 9) {
          dataFromBCD += 9 * pow(10, i);
          recGblSetSevr(plongin,HIGH_ALARM,INVALID_ALARM);
        }
        else {
          dataFromBCD += (unsigned short) ((0x0000000f & data_temp) * pow(10, i));
        }
        data_temp = data_temp >> 4;
        i++;
      }
      plongin->val = dataFromBCD;
    }
    else {
      plongin->val = (unsigned long) pmcmdResponse->dataBuff.wData[0];
    }

    plongin->udf=FALSE;
  }
  else {  /* Arrange callbacks and set pact=1 to let know record support we're waiting for completion*/
    if (f3rp61Seq_queueRequest(dpvt) < 0) {
      errlogPrintf("devLiF3RP61Seq: f3rp61Seq_queueRequest failed for %s\n",
                   plongin->name);
      return (-1);
    }

    plongin->pact = 1;  /* Setting pact to 1 to return to processing*/
  }

  return(0);
}
