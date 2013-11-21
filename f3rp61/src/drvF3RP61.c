/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* drvF3RP61.c - Driver Support Routines for F3RP61
*
*      Author: Jun-ichi Odagiri 
*      Date: 6-30-08
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
#include <asm/fam3rtos/m3iodrv.h>
#include <asm/fam3rtos/m3lib.h>

#include <dbCommon.h>
#include <dbScan.h>
#include <recSup.h>
#include <drvSup.h>
#include <iocsh.h>
#include <epicsThread.h>
#include <errlog.h>
#ifndef EPICS_REVISION
#include <epicsVersion.h>
#endif
#include <epicsExport.h>
#include "drvF3RP61.h"

static long report();
static long init();

struct
{
  long      number;
  DRVSUPFUN report;
  DRVSUPFUN init;
} drvF3RP61 =
{
  2L,
  report,
  init,
};

epicsExportAddress(drvet,drvF3RP61);

int f3rp61_fd;
int use_flnet;

static M3IO_MODULE_INFORMATION module_info[M3IO_NUM_UNIT][M3IO_NUM_SLOT];
static F3RP61_IO_INTR io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];
static int init_flag;
static void msgrcv_thread(void *);
static M3LINKDATACONFIG link_data_config;
static M3COMDATACONFIG com_data_config;
static M3COMDATACONFIG ext_com_data_config;
static void linkDeviceConfigureCallFunc(const iocshArgBuf *);
static void comDeviceConfigureCallFunc(const iocshArgBuf *);
static void linkDeviceConfigure(int, int, int);
static void comDeviceConfigure(int, int, int, int, int);
static void drvF3RP61RegisterCommands(void);


static long report(void)
{

  return (0);
}


static long init(void)
{
  int i, j;
  
  if (init_flag) return (0);
  init_flag = 1;

  f3rp61_fd = open("/dev/m3io", O_RDWR);
  if (f3rp61_fd < 0) {
    errlogPrintf("drvF3RP61: can't open /dev/m3io [%d]\n", errno);
    return (-1);
  }

  for (i = 0; i < M3IO_NUM_UNIT; i++) {
    for (j = 1; j < (M3IO_NUM_SLOT + 1); j++) {
      module_info[i][j].unitno = i;
      module_info[i][j].slotno = j;
      ioctl(f3rp61_fd, M3IO_GET_MODULE_INFO, &module_info[i][j]);
    }
  }

  if (use_flnet) {
    if (setM3FlnSysNo(0, NULL) < 0) {
      errlogPrintf("drvF3RP61: setM3FlnSysNo failed [%d]\n", errno);
      return (-1);
    }

    if (m3rfrsTsk(10) < 0) {
      errlogPrintf("drvF3RP61: m3rfrsTsk failed [%d]\n", errno);
      return (-1);
    }
  }

  return (0);
}


static void msgrcv_thread(void *arg)
{
  int msqid = (int) arg;
  MSG_BUF msgbuf;
  int unit;
  int slot;
  int channel;
  dbCommon *prec;
  IOSCANPVT ioscanpvt;
  int i;

  for (;;) {
    if (msgrcv(msqid, &msgbuf, sizeof(MSG_BUF), M3IO_MSGTYPE_IO, 0) == -1) {
      errlogPrintf("drvF3RP61: msgrcv failed [%d]\n", errno);
    }
    unit = msgbuf.mtext.unit;
    slot = msgbuf.mtext.slot;
    channel = msgbuf.mtext.channel;

    for (i = 0; i < io_intr[unit][slot].count; i++) {
      if (io_intr[unit][slot].ioscan[i].channel == channel) {
	prec = io_intr[unit][slot].ioscan[i].prec;
	if (!prec) {
	  errlogPrintf("drvF3RP61: no record for interrupt (U%d,S%d,C%d)\n", unit, slot, channel);
	  break;
	}
	ioscanpvt = *((IOSCANPVT *) prec->dpvt);
	if (prec->scan == SCAN_IO_EVENT) {
	  scanIoRequest(ioscanpvt);
	}
      }
    }
  }
}


long f3rp61_register_io_interrupt(dbCommon *prec, int unit, int slot, int channel)
{
  M3IO_INTER_DEFINE arg;
  char thread_name[32];
  int msqid = 0;
  int count = io_intr[unit][slot].count;
  int i;

  if (count == NUM_IO_INTR) {
    errlogPrintf("drvF3RP61: no interrupt slot\n");
    return (-1);
  }

  if (count == 0) {
    if ((msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666)) == -1) {
      errlogPrintf("drvF3RP61: msgget failed [%d]\n", errno);
      return (-1);
    }
  /* Add Start */
    if ((msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666)) == -1) {
      errlogPrintf("drvF3RP61: msgget failed[ %d]\n", errno);
      return (-1);
    }
  /* Add End */

    sprintf(thread_name, "msgrcvr%d", msqid);
    if ((epicsThreadCreate(thread_name,
			   epicsThreadPriorityHigh,
			   epicsThreadGetStackSize(epicsThreadStackSmall),
			   (EPICSTHREADFUNC) msgrcv_thread,
			   (void *) msqid)) == (epicsThreadId) 0) {
      errlogPrintf("drvF3RP61: epicsThreadCreate failed\n");
      return (-1);
    }
  }

  io_intr[unit][slot].ioscan[count].channel = channel;
  io_intr[unit][slot].ioscan[count].prec = prec;
  io_intr[unit][slot].count++;

  for (i = 0; i < count; i++) {
    if (io_intr[unit][slot].ioscan[i].channel == channel) {
      return (0);
    }
  }

  arg.unitno = (unsigned short) unit;
  arg.slotno = (unsigned short) slot;
  arg.defData.relayNo = (long) channel;
  arg.msgQId = msqid;

  if (ioctl(f3rp61_fd, M3IO_ENABLE_INTER, &arg) < 0) {
    errlogPrintf("devBiF3RP61: ioctl failed [%d]\n", errno);		        
    return (-1);
  }

  return (0);
}


/*******************************************************************************
 * Get io interrupt info
 *******************************************************************************/
long f3rp61GetIoIntInfo(int cmd, dbCommon * pxx, IOSCANPVT *ppvt)
{
  if (!pxx->dpvt) {
    errlogPrintf("drvF3RP61: f3rp61GetIoIntInfo is called with null dpvt\n");
    return (-1);
  }

  if ( *((IOSCANPVT *) pxx->dpvt) == NULL) {
    scanIoInit((IOSCANPVT *) pxx->dpvt);
  }

  *ppvt = *((IOSCANPVT *) pxx->dpvt);

  return (0);
}



static const iocshArg linkDeviceConfigureArg0 = { "sysNo",iocshArgInt};
static const iocshArg linkDeviceConfigureArg1 = { "nRlys",iocshArgInt};
static const iocshArg linkDeviceConfigureArg2 = { "nRegs",iocshArgInt};
static const iocshArg *linkDeviceConfigureArgs[] =
  {
    &linkDeviceConfigureArg0,
    &linkDeviceConfigureArg1,
    &linkDeviceConfigureArg2
  };

static const iocshFuncDef linkDeviceConfigureFuncDef =
  {
    "f3rp61LinkDeviceConfigure",
    3,
    linkDeviceConfigureArgs
  };

static void linkDeviceConfigureCallFunc(const iocshArgBuf *args)
{
  linkDeviceConfigure(args[0].ival, args[1].ival, args[2].ival);
}

static void linkDeviceConfigure(int sysno, int nrlys, int nregs)
{
  if (sysno < 0 || sysno > 1 || nrlys < 1 || nrlys > 8192 || nregs < 1 || nregs > 8192) {
    errlogPrintf("drvF3RP61: linkDeviceConfigure: parameter out of range\n");
    return;
  }

  link_data_config.wNumberOfRelay[sysno] = nrlys;
  link_data_config.wNumberOfRegister[sysno] = nregs;

  if (setM3LinkDeviceConfig(&link_data_config) < 0) {
    errlogPrintf("drvF3RP61: setM3LinkDeviceConfig failed [%d]\n", errno);
    return;
  }

  use_flnet = 1;
}



static const iocshArg comDeviceConfigureArg0 = { "cpuNo",iocshArgInt};
static const iocshArg comDeviceConfigureArg1 = { "nRlys",iocshArgInt};
static const iocshArg comDeviceConfigureArg2 = { "ext_nRlys",iocshArgInt};
static const iocshArg comDeviceConfigureArg3 = { "nRegs",iocshArgInt};
static const iocshArg comDeviceConfigureArg4 = { "ext_nRegs",iocshArgInt};
static const iocshArg *comDeviceConfigureArgs[] =
  {
    &comDeviceConfigureArg0,
    &comDeviceConfigureArg1,
    &comDeviceConfigureArg2,
    &comDeviceConfigureArg3,
    &comDeviceConfigureArg4
  };

static const iocshFuncDef comDeviceConfigureFuncDef =
  {
    "f3rp61ComDeviceConfigure",
    5,
    comDeviceConfigureArgs
  };

static void comDeviceConfigureCallFunc(const iocshArgBuf *args)
{
  comDeviceConfigure(args[0].ival, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
}

static void comDeviceConfigure(int cpuno, int nrlys, int nregs, int ext_nrlys, int ext_nregs)
{
  if (cpuno < 0 || cpuno > 3 ||
      nrlys < 0 || nrlys >  2048 || nregs < 0 || nregs > 1024 ||
      ext_nrlys < 0 || ext_nrlys >  2048 || ext_nregs < 0 || ext_nregs > 3072) {
    errlogPrintf("drvF3RP61: linkDeviceConfigure: parameter out of range\n");
    return;
  }

  com_data_config.wNumberOfRelay[cpuno] = nrlys;
  com_data_config.wNumberOfRegister[cpuno] = nregs;
  ext_com_data_config.wNumberOfRelay[cpuno] = ext_nrlys;
  ext_com_data_config.wNumberOfRegister[cpuno] = ext_nregs;

  if (setM3ComDataConfig(&com_data_config, &ext_com_data_config) < 0) {
    errlogPrintf("drvF3RP61: setM3ComDataConfig failed [%d]\n", errno);
    return;
  }
}

static void drvF3RP61RegisterCommands(void)
{
  static int firstTime = 1;

  if (firstTime) {
    iocshRegister(&linkDeviceConfigureFuncDef, linkDeviceConfigureCallFunc);
    iocshRegister(&comDeviceConfigureFuncDef, comDeviceConfigureCallFunc);
    firstTime = 0;
  }
}
epicsExportRegistrar(drvF3RP61RegisterCommands);
