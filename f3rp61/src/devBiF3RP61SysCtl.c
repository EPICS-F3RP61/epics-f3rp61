/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBiF3RP61SysCtl.c - Device Support Routines for  F3RP61 Binary Input
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
#include "biRecord.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsExport.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asm/fam3rtos/fam3rtos_sysctl.h>
#include <asm/fam3rtos/m3lib.h>


extern int f3rp61SysCtl_fd;

/* Create the dset for devBiF3RP61SysCtl */
static long init_record();
static long read_bi();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiF3RP61SysCtl={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_bi
};
epicsExportAddress(dset,devBiF3RP61SysCtl);

typedef struct {
  char device;
  char led;
} F3RP61SysCtl_BI_DPVT;

/* Function init_record initializes record - parses INP/OUT field string,
 * allocates private data storage area and sets initial configure values */
static long init_record(biRecord *pbi)
{
  struct link *plink = &pbi->inp;
  int size;
  char *buf;
  F3RP61SysCtl_BI_DPVT *dpvt;
  char device = 'N';	/* Valid states: L (LED), R (Status Register)*/
  char led;		/* Valid states: R (Run), A (Alarm), E (Error)*/

  if (pbi->inp.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pbi,
		      "devBiF3RP61SysCtl (init_record) Illegal INP field");
    pbi->pact = 1;
    return(S_db_badField);
  }

  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  /* Parse 'device' and possibly 'led'*/
  if (sscanf(buf, "SYS,%c%c,", &device, &led) < 2) {
    if (sscanf(buf, "SYS,%c", &device) < 1) {
      errlogPrintf("devBiF3RP61SysCtl: can't get device for %s\n", pbi->name);
      pbi->pact = 1;
      return (-1);
    }
  }

  /* Check Device validity*/
  if (!(device == 'L' || device == 'R')) {
    errlogPrintf("devBiF3RP61SysCtl: illegal device for %s\n", pbi->name);
    pbi->pact = 1;
    return (-1);
  }

  /* Check 'led' validity*/
  if(device == 'L') {
  	  if (!(led == 'R' || led == 'A' || led == 'E')) {
  		  errlogPrintf("devBiF3RP61SysCtl: illegal LED address for %s\n", pbi->name);
  		  pbi->pact = 1;
  		  return (-1);
  	  }
  }

  dpvt = (F3RP61SysCtl_BI_DPVT *) callocMustSucceed(1,
					      sizeof(F3RP61SysCtl_BI_DPVT),
					      "calloc failed");
  dpvt->device = device;
  if (device == 'L') {
    dpvt->led = led;
  }

  pbi->dpvt = dpvt;

  return(0);
}

/* Function is called when there was request to process the record.
 * According to the device (read in init_record) it sets commands and
 * data that is to be sent to driver, sends it and stores returned
 * values to the records RVAL field. */
static long read_bi(biRecord *pbi)
{
  F3RP61SysCtl_BI_DPVT *dpvt = (F3RP61SysCtl_BI_DPVT *) pbi->dpvt;
  char device = dpvt->device;
  char led = dpvt->led;
  int command;
  unsigned long data;

  switch (device) {
  case 'L':
    command = RP6X_SYSIOC_GETLED;
    break;
  default:	/* For device 'R'*/
	  command = RP6X_SYSIOC_STATUSREGREAD;
	  break;
  }

  if (device == 'L') {
    if (ioctl(f3rp61SysCtl_fd, command, &data) < 0) {
      errlogPrintf("devBiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, pbi->name);
      return (-1);
    }
  }
  else if (device == 'R') {
	 if (ioctl(f3rp61SysCtl_fd, command, &data) < 0) {
	    errlogPrintf("devBiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, pbi->name);
	    return (-1);
	 }
  }

  pbi->udf=FALSE;

  switch (device) {
  case 'L':
	  switch(led) {
	  case 'R':
		  pbi->rval = (unsigned long) (data & RP6X_LED_RUN_MASK);
		  break;
	  case 'A':
		  pbi->rval = (unsigned long) (data & RP6X_LED_ALM_MASK);
		  break;
	  case 'E':
		  pbi->rval = (unsigned long) (data & RP6X_LED_ERR_MASK);
		  break;
	  }
	  break;
  case 'R':
	  pbi->rval = (unsigned long) (data & 0x00000004);
	  break;
  }

  /* convert */
  return(0);
}
