/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devWfF3RP61.c - Device Support Routines for F3RP61 Analog Input
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
#include "dbAccessDefs.h"
#include "dbScan.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "dbFldTypes.h"
#include "waveformRecord.h"
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

/* Create the dset for devWfF3RP61 */
static long init_record();
static long read_wf();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
	DEVSUPFUN	special_linconv;
}devWfF3RP61={
	6,
	NULL,
	NULL,
	init_record,
	f3rp61GetIoIntInfo,
	read_wf,
	NULL
};
epicsExportAddress(dset,devWfF3RP61);

extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

typedef struct {
  IOSCANPVT ioscanpvt; /* must comes first */
  union {
    M3IO_ACCESS_COM acom;
    M3IO_ACCESS_REG drly;
  } u;
  char device;
  void *pdata;
} F3RP61_WF_DPVT;


static long init_record(waveformRecord *pwf)
{
  struct link *plink = &pwf->inp;
  int size;
  char *buf;
  char *pC;
  F3RP61_WF_DPVT *dpvt;
  int ftvl = pwf->ftvl;
  M3IO_ACCESS_COM *pacom;
  M3IO_ACCESS_REG *pdrly;
  int unitno, slotno, cpuno, start;
  char device;
  char option = 'W';
  void *pdata;

  if (pwf->inp.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pwf,
		      "devWfF3RP61 (init_record) Illegal INP field");
    pwf->pact = 1;
    return(S_db_badField);
  }

  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc fwfled");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  pC = strchr(buf, ':');
  if (pC) {
    *pC++ = '\0';
    if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
      errlogPrintf("devWfF3RP61: can't get interrupt source address for %s\n", pwf->name);
      pwf->pact = 1;
      return (-1);
    }
    if (f3rp61_register_io_interrupt((dbCommon *) pwf, unitno, slotno, start) < 0) {
      errlogPrintf("devWfF3RP61: can't register I/O interrupt for %s\n", pwf->name);
      pwf->pact = 1;
      return (-1);
    }
  }
  if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
    if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
      if (sscanf(buf, "%c%d", &device, &start) < 2) {
	errlogPrintf("devWfF3RP61: can't get I/O address for %s\n", pwf->name);
	pwf->pact = 1;
	return (-1);
      }
      else if (device != 'W' && device != 'R') {
	errlogPrintf("devWfF3RP61: unsupported device \'%c\' for %s\n", device,
		     pwf->name);
	pwf->pact = 1;
      }
    }
    else {
      device = 'r';
    }
  }
  if (!(device == 'A' ||device == 'r' || device == 'W' || device == 'R')) {
    errlogPrintf("devWfF3RP61: illegal I/O address for %s\n", pwf->name);
    pwf->pact = 1;
    return (-1);
  }
  if (!(option == 'W' || option == 'L' || option == 'U' || option == 'F' || option == 'D')) {
    errlogPrintf("devWfF3RP61: illegal option for %s\n", pwf->name);
    pwf->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_WF_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61_WF_DPVT),
					      "calloc failed");
  dpvt->device = device;

  pdata = callocMustSucceed(pwf->nelm, dbValueSize(pwf->ftvl), "calloc failed");
  dpvt->pdata = pdata;

  if (device == 'r') {
    pacom = &dpvt->u.acom;
    pacom->cpuno = (unsigned short) cpuno;
    pacom->start = (unsigned short) start;
    pacom->count = (unsigned short) (pwf->nelm * 1);
  }
  else if (device == 'W' || device == 'R') {
    pacom = &dpvt->u.acom;
    pacom->start = (unsigned short) start;
    switch (ftvl) {
    case DBF_DOUBLE:
      pacom->count = (unsigned short) (pwf->nelm * 4);
      break;
    case DBF_FLOAT:
    case DBF_ULONG:
    case DBF_LONG:
      pacom->count = (unsigned short) (pwf->nelm * 2);
      break;
    default:
      pacom->count = (unsigned short) (pwf->nelm * 1);
    }
  }
  else {
    pdrly = &dpvt->u.drly;
    pdrly->unitno = (unsigned short) unitno;
    pdrly->slotno = (unsigned short) slotno;
    pdrly->start = (unsigned short) start;
    pdrly->count = (unsigned short) (pwf->nelm * 1);
  }

  pwf->dpvt = dpvt;

  return(0);
}

static long read_wf(waveformRecord *pwf)
{
  F3RP61_WF_DPVT *dpvt = (F3RP61_WF_DPVT *) pwf->dpvt;
  M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
  M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
  char device = dpvt->device;
  int ftvl = pwf->ftvl;
  int command = M3IO_READ_REG;
  unsigned short *pwdata = dpvt->pdata;
  unsigned long *pldata = dpvt->pdata;
  void *p = (void *) pdrly;

  switch (device) {
  case 'r':
    command = M3IO_READ_COM;
    pacom->pdata = pwdata;
    p = (void *) pacom;
    break;
  case 'W':
  case 'R':
    break;
  default:
    switch (ftvl) {
    case DBF_ULONG:
      command = M3IO_READ_REG_L;
      pdrly->u.pldata = pldata;
      break;
    case DBF_USHORT:
    case DBF_SHORT:
      pdrly->u.pwdata = pwdata;
      break;
    default:
      errlogPrintf("%s:unsupported field type of value\n", pwf->name);
      pwf->pact = 1;
      return(-1);
    }
  }

  if (device != 'W' && device != 'R') {
    if (ioctl(f3rp61_fd, command, p) < 0) {
      errlogPrintf("devWfF3RP61: ioctl failed [%d] for %s\n", errno, pwf->name);
      return (-1);
    }
  }
  else if (device == 'W') {
    if (readM3LinkRegister((int) pacom->start, pacom->count, pwdata) < 0) {
      errlogPrintf("devWfF3RP61: readM3LinkRegister failed [%d] for %s\n", errno, pwf->name);
      return (-1);
    }
  }
  else {
    if (readM3ComRegister((int) pacom->start, pacom->count, pwdata) < 0) {
      errlogPrintf("devWfF3RP61: readM3ComRegister failed [%d] for %s\n", errno, pwf->name);
      return (-1);
    }
  }
  pwf->udf=FALSE;

  switch (device) {
    unsigned char *p1;
    unsigned long *p2;
    unsigned short *p3;
    int i;
  case 'r':
  case 'W':
  case 'R':
    switch (ftvl) {
    case DBF_DOUBLE:
      p1 = (unsigned char *) pwf->bptr;
      for (i = 0; i < pwf->nelm; i++) {
	*p1++ = (pwdata[3 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[3 + (4 * i)] & 0xff;
	*p1++ = (pwdata[2 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[2 + (4 * i)] & 0xff;
	*p1++ = (pwdata[1 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[1 + (4 * i)] & 0xff;
	*p1++ = (pwdata[0 + (4 * i)] >> 8) & 0xff; *p1++ = pwdata[0 + (4 * i)] & 0xff;
      }
      break;
    case DBF_FLOAT:
      p1 = (unsigned char *) pwf->bptr;
      for (i = 0; i < pwf->nelm; i++) {
	*p1++ = (pwdata[1 + (2 * i)] >> 8) & 0xff; *p1++ = pwdata[1 + (2 * i)] & 0xff;
	*p1++ = (pwdata[0 + (2 * i)] >> 8) & 0xff; *p1++ = pwdata[0 + (2 * i)] & 0xff;
      }
      break;
    case DBF_ULONG:
    case DBF_LONG:
      p2 = (unsigned long *) pwf->bptr;
      for (i = 0; i < pwf->nelm; i++) {
	p2[i] = ((pwdata[1 + (2 *i)] << 16) & 0xffff0000)  |  (pwdata[0 + (2 * i)] & 0x0000ffff);
      }
      break;
    case DBF_USHORT:
    case DBF_SHORT:
      p3 = (unsigned short *) pwf->bptr;
      for (i = 0; i < pwf->nelm; i++) {
	p3[i] = pwdata[0 + (1 * i)];
      }
      break;
    default:
      errlogPrintf("%s:unsupported field type of value\n", pwf->name);
      pwf->pact = 1;
      return(-1);
    }
    break;
  default:
    switch (ftvl) {
    case DBF_ULONG:
      p2 = (unsigned long *) pwf->bptr;
      for (i = 0; i < pwf->nelm; i++) {
	p2[i] = pldata[i];
      }
      break;
    case DBF_USHORT:
    case DBF_SHORT:
      p3 = (unsigned short *) pwf->bptr;
      for (i = 0; i < pwf->nelm; i++) {
	p3[i] = pwdata[i];
      }
      break;
    default:
      errlogPrintf("%s:unsupported field type of value\n", pwf->name);
      pwf->pact = 1;
      return(-1);
    }
  }

  pwf->nord = pwf->nelm;

  return(0);
}
