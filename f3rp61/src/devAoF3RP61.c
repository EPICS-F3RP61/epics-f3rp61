/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devAoF3RP61.c - Device Support Routines for F3RP61 Analog Output
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
#include "aoRecord.h"
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

/* Create the dset for devAoF3RP61 */
static long init_record();
static long write_ao();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoF3RP61={
	6,
	NULL,
	NULL,
	init_record,
	f3rp61GetIoIntInfo,
	write_ao,
	NULL
};
epicsExportAddress(dset,devAoF3RP61);

extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

typedef struct {
  IOSCANPVT ioscanpvt; /* must comes first */
  union {
    M3IO_ACCESS_COM acom;
    M3IO_ACCESS_REG drly;
  } u;
  char device;
  char option;
} F3RP61_AO_DPVT;


static long init_record(aoRecord *pao)
{
  struct link *plink = &pao->out;
  int size;
  char *buf;
  char *pC;
  F3RP61_AO_DPVT *dpvt;
  M3IO_ACCESS_COM *pacom;
  M3IO_ACCESS_REG *pdrly;
  int unitno, slotno, cpuno, start;
  char device;
  char option = 'W';

  /* bi.out must be an INST_IO */
  if (pao->out.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pao,
		      "devAoF3RP61 (init_record) Illegal OUT field");
    pao->pact = 1;
    return(S_db_badField);
  }

  size = strlen(plink->value.instio.string) + 1;

  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  pC = strchr(buf, '&');
  if (pC) {
    *pC++ = '\0';
    if (sscanf(pC, "%c", &option) < 1) {
      errlogPrintf("devAoF3RP61: can't get option for %s\n", pao->name);
      pao->pact = 1;
      return (-1);
    }
  }
  pC = strchr(buf, ':');
  if (pC) {
    *pC++ = '\0';
    if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
      errlogPrintf("devAoF3RP61: can't get interrupt source address for %s\n", pao->name);
      pao->pact = 1;
      return (-1);
    }
    if (f3rp61_register_io_interrupt((dbCommon *) pao, unitno, slotno, start) < 0) {
      errlogPrintf("devAoF3RP61: can't register I/O interrupt for %s\n", pao->name);
      pao->pact = 1;
      return (-1);
    }
  }
  if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
    if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
      if (sscanf(buf, "%c%d", &device, &start) < 2) {
	errlogPrintf("devAoF3RP61: can't get I/O address for %s\n", pao->name);
	pao->pact = 1;
	return (-1);
      }
      else if (device != 'W' && device != 'R') {
	errlogPrintf("devAoF3RP61: unsupported device \'%c\' for %s\n", device,
		     pao->name);
	pao->pact = 1;
      }
    }
    else {
      device = 'r';
    }
  }
  if (!(device == 'Y' || device == 'A' || device == 'r' || device == 'W' ||
	device == 'R')) {
    errlogPrintf("devAoF3RP61: illegal I/O address for %s\n", pao->name);
    pao->pact = 1;
    return (-1);
  }
  if (!(option == 'W' || option == 'L' || option == 'F' || option == 'D')) {
    errlogPrintf("devAoF3RP61: illegal option for %s\n", pao->name);
    pao->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_AO_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61_AO_DPVT),
					      "calloc failed");
  dpvt->device = device;
  dpvt->option = option;
  dpvt->option = option;

  if (device == 'r') {
    pacom = &dpvt->u.acom;
    pacom->cpuno = (unsigned short) cpuno;
    pacom->start = (unsigned short) start;
    pacom->count = (unsigned short) 1;
  }
  else if (device == 'W' || device == 'R') {
    pacom = &dpvt->u.acom;
    pacom->start = (unsigned short) start;
    switch (option) {
    case 'D':
      pacom->count = 4;
      break;
    case 'F':
    case 'L':
      pacom->count = 2;
      break;
    default:
      pacom->count = 1;
    }
  }
  else {
    pdrly = &dpvt->u.drly;
    pdrly->unitno = (unsigned short) unitno;
    pdrly->slotno = (unsigned short) slotno;
    pdrly->start = (unsigned short) start;
    pdrly->count = (unsigned short) 1;
  }

  pao->dpvt = dpvt;

  return(0);
}

static long write_ao(aoRecord *pao)
{
  F3RP61_AO_DPVT *dpvt = (F3RP61_AO_DPVT *) pao->dpvt;
  M3IO_ACCESS_COM *pacom = &dpvt->u.acom;
  M3IO_ACCESS_REG *pdrly = &dpvt->u.drly;
  char device = dpvt->device;
  char option = dpvt->option;
  int command = M3IO_WRITE_REG;
  unsigned short wdata[4];
  unsigned long ldata;
  void *p = (void *) pdrly;

  switch (device) {
  case 'Y':
    command = M3IO_WRITE_OUTRELAY;
    pdrly->u.wdata[0] = (unsigned short) pao->rval;
    break;
  case 'r':
    command = M3IO_WRITE_COM;
    wdata[0] = (unsigned short) pao->rval;
    pacom->pdata = &wdata[0];
    p = (void *) pacom;
    break;
  case 'W':
  case 'R':
    switch (option) {
      float fval;
      unsigned char *p;
    case 'D':
      p = (unsigned char *) &pao->val;
      wdata[3] = (*p++ << 8) & 0xff00; wdata[3] |= *p++ & 0xff;
      wdata[2] = (*p++ << 8) & 0xff00; wdata[2] |= *p++ & 0xff;
      wdata[1] = (*p++ << 8) & 0xff00; wdata[1] |= *p++ & 0xff;
      wdata[0] = (*p++ << 8) & 0xff00; wdata[0] |= *p++ & 0xff;
      break;
    case 'F':
      fval = (float) pao->val;
      p = (unsigned char *) &fval;
      wdata[1] = (*p++ << 8) & 0xff00; wdata[1] |= *p++ & 0xff;
      wdata[0] = (*p++ << 8) & 0xff00; wdata[0] |= *p++ & 0xff;
      break;
    case 'L':
      wdata[0] = (unsigned short) (pao->rval & 0x0000ffff);
      wdata[1] = (unsigned short) ((pao->rval >> 16) & 0x0000ffff);
      break;
    default:
      wdata[0] = (unsigned short) pao->rval;
    }
    break;
  default:
    switch (option) {
    case 'L':
      command = M3IO_WRITE_REG_L;
      ldata = (unsigned long) pao->rval;
      pdrly->u.pldata = &ldata;
      break;
    default:
      wdata[0] = (unsigned short) pao->rval;
      pdrly->u.pwdata = &wdata[0];
    }
  }

  if (device != 'W' && device != 'R') {
    if (ioctl(f3rp61_fd, command, p) < 0) {
      errlogPrintf("devAoF3RP61: ioctl failed [%d] for %s\n", errno, pao->name);
      return (-1);
    }
  }
  else if (device == 'W') {
    if (writeM3LinkRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
      errlogPrintf("devAoF3RP61: writeM3LinkRegister failed [%d] for %s\n", errno, pao->name);
      return (-1);
    }
  }
  else {
    if (writeM3ComRegister((int) pacom->start, pacom->count, &wdata[0]) < 0) {
      errlogPrintf("devAoF3RP61: writeM3ComRegister failed [%d] for %s\n", errno, pao->name);
      return (-1);
    }
  }
  pao->udf=FALSE;

  return(0);
}
