/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* drvF3RP61Seq.c - Driver Support Routines for F3RP61 Sequence device
*
*      Author: Jun-ichi Odagiri 
*      Date: 09-02-08
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <asm/fam3rtos/m3mcmd.h>

#include <dbCommon.h>
#include <dbScan.h>
#include <callback.h>
#include <recSup.h>
#include <drvSup.h>
#include <iocsh.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <errlog.h>
#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#include <epicsExport.h>
#include "drvF3RP61Seq.h"

static long report();
static long init();

struct
{
  long      number;
  DRVSUPFUN report;
  DRVSUPFUN init;
} drvF3RP61Seq =
{
  2L,
  report,
  init,
};
epicsExportAddress(drvet,drvF3RP61Seq);

int f3rp61Seq_fd;

void showreq(const iocshArgBuf *);
void stopshow(const iocshArgBuf *);
static const iocshFuncDef showreqDef = {"showreq", 0, NULL};
static const iocshFuncDef stopshowDef = {"stopshow", 0, NULL};

static int init_flag;
static int debug_flag;
static unsigned short request_id;

static void mcmd_thread(void *);
static void dump_mcmd_request(MCMD_STRUCT *);
static epicsMutexId f3rp61Seq_queueMutex;
static epicsEventId f3rp61Seq_queueEvent;
static ELLLIST f3rp61Seq_queueList;

static F3RP61_SEQ_DPVT *get_request_from_queue(void);


static long report(void)
{
  return (0);
}


static long init(void)
{
  if (init_flag) return (0);
  init_flag = 1;

  f3rp61Seq_fd = open("/dev/m3mcmd", O_RDWR);
  if (f3rp61Seq_fd < 0) {
    errlogPrintf("drvF3RP61Seq: can't open /dev/m3mcmd\n");
    return (-1);
  }

  if ((f3rp61Seq_queueMutex = epicsMutexCreate()) == (epicsMutexId) 0) {
    errlogPrintf("drvF3RP61Seq: epicsMutexCreate failed\n");
    return (-1);
  }

  if ((f3rp61Seq_queueEvent = epicsEventCreate(epicsEventEmpty)) == (epicsEventId) 0) {
    errlogPrintf("drvF3RP61Seq: epicsEventCreate failed\n");
    return (-1);
  }

  ellInit(&f3rp61Seq_queueList);

  if ((epicsThreadCreate("f3rp61Seq_mcmd",
			 epicsThreadPriorityHigh,
			 epicsThreadGetStackSize(epicsThreadStackSmall),
			 (EPICSTHREADFUNC) mcmd_thread,
			 (void *) NULL)) == (epicsThreadId) 0) {
    errlogPrintf("drvF3RP61Seq: epicsThreadCreate failed\n");
    return (-1);
  }

  iocshRegister(&showreqDef, showreq);
  iocshRegister(&stopshowDef, stopshow);

  return (0);
}


static void mcmd_thread(void *arg)
{
  F3RP61_SEQ_DPVT *dpvt;
  MCMD_STRUCT *pmcmdStruct;
  CALLBACK *pcallback;
  dbCommon *prec;

  for (;;) {
    epicsEventMustWait(f3rp61Seq_queueEvent);

    while ((dpvt = get_request_from_queue())) {
      pmcmdStruct = &dpvt->mcmdStruct;
      pmcmdStruct->mcmdRequest.comId = ++request_id;
      dpvt->ret = 0;

      if (debug_flag) dump_mcmd_request(pmcmdStruct);

      if (ioctl(f3rp61Seq_fd, MCMD_ACCS, pmcmdStruct) < 0) {
	errlogPrintf("drvF3RP61Seq: ioctl failed [%d]\n", errno);
	dpvt->ret = -1;
      }

      if (pmcmdStruct->mcmdResponse.comId != request_id) {
	errlogPrintf("drvF3RP61Seq: comId does not match\n");
	dpvt->ret = -1;
      }

      pcallback = &dpvt->callback;
      callbackGetUser(prec, pcallback);
      callbackRequestProcessCallback(pcallback, priorityLow, prec);
    }
  }
}


long f3rp61Seq_queueRequest(F3RP61_SEQ_DPVT *dpvt)
{
  if (!dpvt) {
    errlogPrintf("drvF3RP61Seq: null request\n");
    return (-1);
  }

  epicsMutexMustLock(f3rp61Seq_queueMutex);
  ellAdd(&f3rp61Seq_queueList, &dpvt->node);
  epicsMutexUnlock(f3rp61Seq_queueMutex);

  epicsEventSignal(f3rp61Seq_queueEvent);

  return (0);
}


static F3RP61_SEQ_DPVT *get_request_from_queue(void)
{
  F3RP61_SEQ_DPVT *dpvt;

  epicsMutexMustLock(f3rp61Seq_queueMutex);
  dpvt = (F3RP61_SEQ_DPVT *) ellGet(&f3rp61Seq_queueList);
  epicsMutexUnlock(f3rp61Seq_queueMutex);

  return dpvt;
}



static void dump_mcmd_request(MCMD_STRUCT *pmcmdStruct)
{
  MCMD_REQUEST *pmcmdRequest = &pmcmdStruct->mcmdRequest;
  M3_WRITE_SEQDEV *pM3WriteSeqdev = (M3_WRITE_SEQDEV *) &pmcmdRequest->dataBuff.bData[0];

  printf("\n");
  printf("formatCode    : 0x%02x\n", pmcmdRequest->formatCode);
  printf("responseOption: %d\n",     pmcmdRequest->responseOption);
  printf("srcSlot       : %d\n",     pmcmdRequest->srcSlot);
  printf("destSlot      : %d\n",     pmcmdRequest->destSlot);
  printf("mainCode      : 0x%02x\n", pmcmdRequest->mainCode);
  printf("subCode       : 0x%02x\n", pmcmdRequest->subCode);
  printf("dataSize      : %d\n",     pmcmdRequest->dataSize);
  printf("accessType    : %d\n",     pM3WriteSeqdev->accessType);
  printf("devType       : 0x%02x\n", pM3WriteSeqdev->devType);
  printf("dataNum       : %d\n",     pM3WriteSeqdev->dataNum);
  printf("topDevNo      : %d\n",     (int) pM3WriteSeqdev->topDevNo);
  printf("\n");
}



void showreq(const iocshArgBuf *args)
{
  debug_flag = 1;
}



void stopshow(const iocshArgBuf *args)
{
  debug_flag = 0;
}
