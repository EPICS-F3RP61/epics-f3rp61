/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiF3RP61.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
*
*      Author: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
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
#include "mbbiRecord.h"
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

/* Create the dset for devMbbiF3RP61 */
static long init_record();
static long read_mbbi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
	DEVSUPFUN	special_linconv;
}devMbbiF3RP61={
	6,
	NULL,
	NULL,
	init_record,
	f3rp61GetIoIntInfo,
	read_mbbi,
	NULL
};
epicsExportAddress(dset,devMbbiF3RP61);

extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

typedef struct {
  IOSCANPVT ioscanpvt; /* must come first */
  union {
    M3IO_ACCESS_COM acom;
    M3IO_ACCESS_REG drly;
  } u;
  char device;
} F3RP61_MBBI_DPVT;

/* Function init_record initializes record - parses INP/OUT field string,
 * allocates private data storage area and sets initial configure values */
static long init_record(mbbiRecord *pmbbi)
{
  struct link *plink = &pmbbi->inp;
  int size;
  char *buf;
  char *pC;
  F3RP61_MBBI_DPVT *dpvt;
  M3IO_ACCESS_COM *pacom;
  M3IO_ACCESS_REG *pdrly;
  int unitno, slotno, cpuno, start;
  char device;

  /* Input link type must be INST_IO */
  if (pmbbi->inp.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pmbbi,
		      "devMbbiF3RP61 (init_record) Illegal INP field");
    pmbbi->pact = 1;
    return(S_db_badField);
  }

  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  /* Parse for possible interrupt source*/
  pC = strchr(buf, ':');
  if (pC) {
    *pC++ = '\0';
    if (sscanf(pC, "U%d,S%d,X%d", &unitno, &slotno, &start) < 3) {
      errlogPrintf("devMbbiF3RP61: can't get interrupt source address for %s\n",
		   pmbbi->name);
      pmbbi->pact = 1;
      return (-1);
    }
    if (f3rp61_register_io_interrupt((dbCommon *) pmbbi, unitno, slotno, start) < 0) {
      errlogPrintf("devMbbiF3RP61: can't register I/O interrupt for %s\n",
		   pmbbi->name);
      pmbbi->pact = 1;
      return (-1);
    }
  }

  /* Parse device*/
  if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
    if (sscanf(buf, "CPU%d,R%d", &cpuno, &start) < 2) {
      if (sscanf(buf, "%c%d", &device, &start) < 2) {
	errlogPrintf("devMbbiF3RP61: can't get I/O address for %s\n", pmbbi->name);
	pmbbi->pact = 1;
	return (-1);
      }
      else if (device != 'W' && device != 'L' && device != 'R' && device != 'E') {
	errlogPrintf("devMbbiF3RP61: unsupported device \'%c\' for %s\n", device,
		     pmbbi->name);
	pmbbi->pact = 1;
	return (-1);
      }
    }
    else {
      device = 'r';
    }
  }

  /* Check device validity*/
  if (!(device == 'X' || device == 'Y' || device == 'A' || device == 'r' ||
	device == 'W' || device == 'L' || device == 'M' || device == 'R' ||
	device == 'E')) {
    errlogPrintf("devMbbiF3RP61: illegal I/O address for %s\n",
		 pmbbi->name);
    pmbbi->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_MBBI_DPVT *) callocMustSucceed(1,
						      sizeof(F3RP61_MBBI_DPVT),
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

  pmbbi->dpvt = dpvt;

  return(0);
}

/* Function is called when there was request to process the record.
 * According to the device (read in init_record) it sets commands and
 * data that is to be sent to driver, sends it and stores returned
 * values to the records RVAL field. */
static long read_mbbi(mbbiRecord *pmbbi)
{
  F3RP61_MBBI_DPVT *dpvt = (F3RP61_MBBI_DPVT *) pmbbi->dpvt;
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
      errlogPrintf("devMbbiF3RP61: ioctl failed [%d] for %s\n", errno, pmbbi->name);
      return (-1);
    }
  }
  else if (device == 'W') {
    if (readM3LinkRegister(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiF3RP61: readM3LinkRegister failed [%d] for %s\n",
		   errno, pmbbi->name);
      return (-1);
    }
  }
  else if (device == 'R') {
    if (readM3ComRegister(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiF3RP61: readM3ComRegister failed [%d] for %s\n",
		   errno, pmbbi->name);
      return (-1);
    }
  }
  else if (device == 'L') {
    if (readM3LinkRelay(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiF3RP61: readM3LinkRelay failed [%d] for %s\n",
		   errno, pmbbi->name);
      return (-1);
    }
  }
  else {
    if (readM3ComRelay(pacom->start, 1, &wdata) < 0) {
      errlogPrintf("devMbbiF3RP61: readM3ComRelay failed [%d] for %s\n",
		   errno, pmbbi->name);
      return (-1);
    }
  }
  pmbbi->udf=FALSE;

  switch (device) {
  case 'X':
    pmbbi->rval = (long) pdrly->u.inrly[0].data;
    break;
  case 'Y':
    pmbbi->rval = (long) pdrly->u.outrly[0].data;
    break;
  case 'M':
    /* need to use old style */
    pmbbi->rval = (long) pdrly->u.wdata[0];
    break;
  case 'r':
  case 'W':
  case 'R':
  case 'L':
  case 'E':
  default:
    pmbbi->rval = (long) wdata;
  }

  /* convert */
  return(0);
}
