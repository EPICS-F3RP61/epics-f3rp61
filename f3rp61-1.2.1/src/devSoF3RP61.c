/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
**************************************************************************
* devSoF3RP61.c - Device Support Routines for F3RP61 String Output
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
#include "stringoutRecord.h"
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

/* Create the dset for devSoF3RP61 */
static long init_record();
static long write_so();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_so;
	DEVSUPFUN	special_linconv;
}devSoF3RP61={
	6,
	NULL,
	NULL,
	init_record,
	f3rp61GetIoIntInfo,
	write_so,
	NULL
};
epicsExportAddress(dset,devSoF3RP61);

extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

typedef struct {
  IOSCANPVT ioscanpvt; /* must comes first */
  M3IO_ACCESS_REG drly;
  char device;
} F3RP61_SO_DPVT;


static long init_record(stringoutRecord *pso)
{
  struct link *plink = &pso->out;
  int size;
  char *buf;
  char *pC;
  F3RP61_SO_DPVT *dpvt;
  M3IO_ACCESS_REG *pdrly;
  int unitno, slotno, start;
  char device;

  /* bi.out must be an INST_IO */
  if (pso->out.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pso,
		      "devSoF3RP61 (init_record) Illegal OUT field");
    pso->pact = 1;
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
      errlogPrintf("devSoF3RP61: can't get interrupt source address for %s\n", pso->name);
      pso->pact = 1;
      return (-1);
    }
    if (f3rp61_register_io_interrupt((dbCommon *) pso, unitno, slotno, start) < 0) {
      errlogPrintf("devSoF3RP61: can't register I/O interrupt for %s\n", pso->name);
      pso->pact = 1;
      return (-1);
    }
  }
  if (sscanf(buf, "U%d,S%d,%c%d", &unitno, &slotno, &device, &start) < 4) {
    errlogPrintf("devSoF3RP61: can't get I/O address for %s\n", pso->name);
    pso->pact = 1;
    return (-1);
  }
  if (!(device == 'A')) {
    errlogPrintf("devSoF3RP61: illegal I/O address for %s\n", pso->name);
    pso->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61_SO_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61_SO_DPVT),
					      "calloc failed");
  dpvt->device = device;
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
							 sizeof(char),
							 "calloc failed");

  /*
  pdrly->count = (unsigned short) 40;
  */
  pdrly->count = (unsigned short) 20;

  pso->dpvt = dpvt;

  return(0);
}

static long write_so(stringoutRecord *pso)
{
  F3RP61_SO_DPVT *dpvt = (F3RP61_SO_DPVT *) pso->dpvt;
  M3IO_ACCESS_REG *pdrly = &dpvt->drly;

  strncpy((char *) pdrly->u.pbdata, (char *) &pso->val, 40);

  if (ioctl(f3rp61_fd, M3IO_WRITE_REG, pdrly) < 0) {
    errlogPrintf("devSoF3RP61: ioctl failed [%d] for %s\n", errno, pso->name);
    return (-1);
  }
  pso->udf=FALSE;

  return(0);
}
