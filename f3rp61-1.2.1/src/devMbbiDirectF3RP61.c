/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devMbbiDirectF3RP61.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
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
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "mbbiDirectRecord.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsExport.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/fam3rtos/m3iodrv.h>
#include <asm/fam3rtos/m3lib.h>
#include "drvF3RP61.h"

extern int f3rp61_fd;

/* Create the dset for devMbbiDirectF3RP61 */
static long init_record();
static long read_mbbiDirect();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbiDirect;
	DEVSUPFUN	special_linconv;
}devMbbiDirectF3RP61={
	6,
	NULL,
	NULL,
	init_record,
	f3rp61GetIoIntInfo,
	read_mbbiDirect,
	NULL
};
epicsExportAddress(dset,devMbbiDirectF3RP61);

extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

typedef struct {
  IOSCANPVT ioscanpvt; /* must comes first */
  union {
    M3IO_ACCESS_COM acom;
    M3IO_ACCESS_REG drly;
  } u;
  char device;
} F3RP61_MBBIDIRECT_DPVT;


static long init_record(mbbiDirectRecord *pmbbiDirect)
{
  struct link *plink = &pmbbiDirect->inp;
  int size;
  char *buf;
  char *pC;
  F3RP61_MBBIDIRECT_DPVT *dpvt;
  M3IO_ACCESS_COM *pacom;
  M3IO_ACCESS_REG *pdrly;
  int unitno, slotno, cpuno, start;
  char device;

  if (pmbbiDirect->inp.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pmbbiDirect,
		      "devMbbiDirectF3RP61 (init_record) Illegal INP field");
    pmbbiDirect->pact = 1;
    return(S_db_badField);
  }

  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  pC = strchr(buf, ':');
  if (pC) {
    *pC++ = '\0';
    if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
      errlogPrintf("devMbbiDirectF3RP61: can't get interrupt source address for %s\n",
		   pmbbiDirect->name);
      pmbbiDirect->pact = 1;
      return (-1);
    }
    if (f3rp61_register_io_interrupt((dbCommon *) pmbbiDirect, unitno, slotno, start) < 0) {
      errlogPrintf("devMbbiDirectF3RP61: can't register I/O interrupt for %s\n",
		   pmbbiDirect->name);
      pmbbiDirect->pact = 1;
      return (-1);
    }
  }
  if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
    if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
      if (sscanf(buf, "%c%d", &device, &start) < 2) {
	errlogPrintf("devMbbiDirectF3RP61: can't get I/O address for %s\n", pmbbiDirect->name);
	pmbbiDirect->pact = 1;
	return (-1);
      }
      else if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
	errlogPrintf("devMbbiDirectF3RP61: unsupported device \'%c\' for %s\n", device,
		     pmbbiDirect->name);
	pmbbiDirect->pact = 1;
	return (-1);
      }
    }
    else {
      device = 'r';
    }
  }

  if (!(device == 'X' || device == 'Y' || device == 'A' || device == 'r' ||
	device == 'W' || device == 'L' || device == 'M' || device == 'R' ||
	device == 'E')) {
    errlogPrintf("devMbbiDirectF3RP61: illegal I/O address for %s\n",
		 pmbbiDirect->name);
    pmbbiDirect->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_MBBIDIRECT_DPVT *) callocMustSucceed(1,
						      sizeof(F3RP61_MBBIDIRECT_DPVT),
						      "calloc failed");
  dpvt->device = device;

  if (device == 'r') {
    pacom = &dpvt->u.acom;
    pacom->cpuno = (unsigned short) cpuno;
    pacom->start = (unsigned short) start;
    pacom->count = (unsigned short) 1;
  }
  else if (device == 'W' || device == 'L' || device == 'R' || device == 'E') {
    pacom = &dpvt->u.acom;
    pacom->start = (unsigned short) start;
  }
  else {
    pdrly = &dpvt->u.drly;
    pdrly->unitno = (unsigned short) unitno;
    pdrly->slotno = (unsigned short) slotno;
    pdrly->start = (unsigned short) start;
    pdrly->count = (unsigned short) 1;
  }

  pmbbiDirect->dpvt = dpvt;

  return(0);
}

static long read_mbbiDirect(mbbiDirectRecord *pmbbiDirect)
{
  F3RP61_MBBIDIRECT_DPVT *dpvt = (F3RP61_MBBIDIRECT_DPVT *) pmbbiDirect->dpvt;
  M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
  M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
  char device = dpvt->device;
  int command;
  unsigned short wdata;
  void *p = (void *) pdrly;

  switch (device) {
  case 'X':
    command = M3IO_READ_INRELAY;
    break;
  case 'Y':
    command = M3IO_READ_OUTRELAY;
    break;
  case 'r':
    command = M3IO_READ_COM;
    pacom->pdata = &wdata;
    p = (void *) pacom;
    break;
  case 'W':
  case 'R':
  case 'L':
  case 'E':
    break;
  case 'M':
    /* need to use old style */
    command = M3IO_READ_MODE;
    break;
  default:
    command = M3IO_READ_REG;
    pdrly->u.pwdata = &wdata;
  }

  if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
    if (ioctl(f3rp61_fd, command, p) < 0) {
      errlogPrintf("devMbbiDirectF3RP61: ioctl failed [%d] for %s\n", errno, pmbbiDirect->name);
      return (-1);
    }
  }
  else if (device == 'W') {
    if (readM3LinkRegister(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiDirectF3RP61: readM3LinkRegister failed [%d] for %s\n",
		   errno, pmbbiDirect->name);
      return (-1);
    }
  }
  else if (device == 'R') {
    if (readM3ComRegister(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiDirectF3RP61: readM3ComRegister failed [%d] for %s\n",
		   errno, pmbbiDirect->name);
      return (-1);
    }
  }
  else if (device == 'L') {
    if (readM3LinkRelay(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiDirectF3RP61: readM3LinkRelay failed [%d] for %s\n",
		   errno, pmbbiDirect->name);
      return (-1);
    }
  }
  else {
    if (readM3ComRelay(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiDirectF3RP61: readM3ComRelay failed [%d] for %s\n",
		   errno, pmbbiDirect->name);
      return (-1);
    }
  }
  pmbbiDirect->udf=FALSE;

  switch (device) {
  case 'X':
    pmbbiDirect->rval = (long) pdrly->u.inrly[0].data;
    break;
  case 'Y':
    pmbbiDirect->rval = (long) pdrly->u.outrly[0].data;
    break;
  case 'M':
    /* need to use old style */
    pmbbiDirect->rval = (long) pdrly->u.wdata[0];
    break;
  case 'r':
  case 'W':
  case 'R':
  case 'L':
  case 'E':
  default:
    pmbbiDirect->rval = (long) wdata;
  }

  /* convert */
  return(0);
}
