/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devMbbiF3RP61SysCtl.c - Device Support Routines for F3RP61 Multi-bit
* Binary Input
*
*      Author: Gregor Kostevc (Cosylab)
*      Date: Dec. 2013
*/
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <alarm.h>
#include <cantProceed.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExport.h>
#include <errlog.h>
#include <recGbl.h>
#include <recSup.h>
#include <mbbiRecord.h>

#include <drvF3RP61SysCtl.h>

/* Create the dset for devMbbiF3RP61SysCtl */
static long init_record();
static long read_mbbi();

struct {
  long       number;
  DEVSUPFUN  report;
  DEVSUPFUN  init;
  DEVSUPFUN  init_record;
  DEVSUPFUN  get_ioint_info;
  DEVSUPFUN  read_mbbi;
  DEVSUPFUN  special_linconv;
} devMbbiF3RP61SysCtl = {
  6,
  NULL,
  NULL,
  init_record,
  NULL,
  read_mbbi,
  NULL
};

epicsExportAddress(dset,devMbbiF3RP61SysCtl);

typedef struct {
  char device;
  char led;
} F3RP61SysCtl_MBBI_DPVT;


static long init_record(mbbiRecord *pmbbi)
{
  struct link *plink = &pmbbi->inp;
  int size;
  char *buf;
  F3RP61SysCtl_MBBI_DPVT *dpvt;
  char device;

  if (pmbbi->inp.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pmbbi,
                      "devMbbiF3RP61SysCtl (init_record) Illegal INP field");
    pmbbi->pact = 1;
    return(S_db_badField);
  }

  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  /* Parse device*/
  if (sscanf(buf, "SYS,%c,", &device) < 1) {
      errlogPrintf("devMbbiF3RP61SysCtl: can't get device for %s\n", pmbbi->name);
      pmbbi->pact = 1;
      return (-1);
  }

  /* Check device validity*/
  if (!(device == 'S')) {
    errlogPrintf("devMbbiF3RP61SysCtl: illegal device for %s\n",
                 pmbbi->name);
    pmbbi->pact = 1;
    return (-1);
  }

  dpvt = (F3RP61SysCtl_MBBI_DPVT *) callocMustSucceed(1,
                                                      sizeof(F3RP61SysCtl_MBBI_DPVT),
                                                      "calloc failed");
  dpvt->device = device;

  pmbbi->dpvt = dpvt;

  return(0);
}

static long read_mbbi(mbbiRecord *pmbbi)
{
  F3RP61SysCtl_MBBI_DPVT *dpvt = (F3RP61SysCtl_MBBI_DPVT *) pmbbi->dpvt;

  char device = dpvt->device;
  int command;
  unsigned long data = 3;

  switch (device) {
  case 'S':
    command = M3SC_GET_SW;
    break;
  default:
    command = M3SC_GET_SW;
    break;
  }

  /* printf("Data before %ld \n", data); */  /*Debug output*/

  if (device == 'S') {
    if (ioctl(f3rp61SysCtl_fd, command, &data) < 0) {
      errlogPrintf("devMbbiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, pmbbi->name);
      return (-1);
    }
  }
  pmbbi->udf=FALSE;

  /* printf("Data after %ld \n", data); */  /*Debug output*/

  switch (device) {
  case 'S':
    pmbbi->rval = (long) data;
    break;
  default:
    pmbbi->rval = (long) data;
    break;
  }

  return(0);
}
