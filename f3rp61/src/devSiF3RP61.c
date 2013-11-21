/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devSiF3RP61.c - Device Support Routines for F3RP61 String Input
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
#include "stringinRecord.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsExport.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/fam3rtos/m3iodrv.h>
#include "drvF3RP61.h"

extern int f3rp61_fd;

/* Create the dset for devSiF3RP61 */
static long init_record();
static long read_si();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_si;
	DEVSUPFUN	special_linconv;
}devSiF3RP61={
	6,
	NULL,
	NULL,
	init_record,
	f3rp61GetIoIntInfo,
	read_si,
	NULL
};
epicsExportAddress(dset,devSiF3RP61);

extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

typedef struct {
  IOSCANPVT ioscanpvt; /* must comes first */
  M3IO_ACCESS_REG drly;
} F3RP61_SI_DPVT;


static long init_record(stringinRecord *psi)
{
  struct link *plink = &psi->inp;
  int size;
  char *buf;
  char *pC;
  F3RP61_SI_DPVT *dpvt;
  M3IO_ACCESS_REG *pdrly;
  int unitno, slotno, start;
  char device;

  /* si.inp must be an INST_IO */
  if (psi->inp.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)psi,
		      "devSiF3RP61 (init_record) Illegal INP field");
    psi->pact = 1;
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
      errlogPrintf("devSiF3RP61: can't get interrupt source address for %s\n", psi->name);
      psi->pact = 1;
      return (-1);
    }
    if (f3rp61_register_io_interrupt((dbCommon *) psi, unitno, slotno, start) < 0) {
      errlogPrintf("devSiF3RP61: can't register I/O interrupt for %s\n", psi->name);
      psi->pact = 1;
      return (-1);
    }
  }
  if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
    errlogPrintf("devSiF3RP61: can't get I/O address for %s\n", psi->name);
    psi->pact = 1;
    return (-1);
  }
  if (!(device == 'A')) {
    errlogPrintf("devSiF3RP61: illegal I/O address for %s\n", psi->name);
    psi->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_SI_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61_SI_DPVT),
					      "calloc failed");
  pdrly = &dpvt->drly;
  pdrly->unitno = (unsigned short) unitno;
  pdrly->slotno = (unsigned short) slotno;
  pdrly->start = (unsigned short) start;
  /*
  pdrly->u.pbdata = (unsigned char *) callocMustSucceed(40,
							sizeof(unsigned char),
							"calloc failed");
  */
  pdrly->u.pwdata = (unsigned short *) callocMustSucceed(40,
							 sizeof(unsigned char),
							 "calloc failed");
  /*
  pdrly->count = (unsigned short) 40;
  */
  pdrly->count = (unsigned short) 20;

  psi->dpvt = dpvt;

  return(0);
}

static long read_si(stringinRecord *psi)
{
  F3RP61_SI_DPVT *dpvt = (F3RP61_SI_DPVT *) psi->dpvt;
  M3IO_ACCESS_REG *pdrly = &dpvt->drly;
  /*
  printf("devSiF3RP61: read_si() entered for %s\n", psi->name);
  */
  /*
  if (ioctl(f3rp61_fd, M3IO_READ_REG_B, pdrly) < 0) {
  */
  if (ioctl(f3rp61_fd, M3IO_READ_REG, pdrly) < 0) {
    errlogPrintf("devSiF3RP61: ioctl failed [%d] for %s\n", errno, psi->name);
    return (-1);
  }
  strncpy((char *) &psi->val, (char *) pdrly->u.pbdata, 40);

  psi->udf=FALSE;

  return(0);
}
