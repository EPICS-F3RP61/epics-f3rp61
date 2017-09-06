/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* devBoF3RP61SysCtl.c - Device Support Routines for F3RP61 Binary Output
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
#include "boRecord.h"
#include "cantProceed.h"
#include "errlog.h"
#include "epicsExport.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(_arm_)
#  include <m3sysctl.h>
#  include <m3lib.h>
#elif defined(_ppc_)
#  include <asm/fam3rtos/fam3rtos_sysctl.h>
#  include <asm/fam3rtos/m3lib.h>
#  define M3SC_SET_LED      RP6X_SYSIOC_SETLED
#  define M3SC_LED_RUN_OFF  RP6X_LED_RUN_OFF
#  define M3SC_LED_ALM_OFF  RP6X_LED_ALM_OFF
#  define M3SC_LED_ERR_OFF  RP6X_LED_ERR_OFF
#  define M3SC_LED_RUN_ON   RP6X_LED_RUN_ON
#  define M3SC_LED_ALM_ON   RP6X_LED_ALM_ON
#  define M3SC_LED_ERR_ON   RP6X_LED_ERR_ON
#else
#  error
#endif

extern int f3rp61SysCtl_fd;

/* Create the dset for devBoF3RP61SysCtl */
static long init_record();
static long read_bo();

struct {
  long       number;
  DEVSUPFUN  report;
  DEVSUPFUN  init;
  DEVSUPFUN  init_record;
  DEVSUPFUN  get_ioint_info;
  DEVSUPFUN  read_bo;
  DEVSUPFUN  special_linconv;
} devBoF3RP61SysCtl = {
  6,
  NULL,
  NULL,
  init_record,
  NULL,
  read_bo,
  NULL
};

epicsExportAddress(dset,devBoF3RP61SysCtl);

typedef struct {
  char device;
  char led;
} F3RP61SysCtl_BO_DPVT;

/* Function init_record initializes record - parses INP/OUT field string,
 * allocates private data storage area and sets initial configure values */
static long init_record(boRecord *pbo)
{
  struct link *plink = &pbo->out;
  int size;
  char *buf;
  F3RP61SysCtl_BO_DPVT *dpvt;
  char device;
  char led;

  if (pbo->out.type != INST_IO) {
    recGblRecordError(S_db_badField,(void *)pbo,
                      "devBoF3RP61SysCtl (init_record) Illegal OUT field");
    pbo->pact = 1;
    return(S_db_badField);
  }

  size = strlen(plink->value.instio.string) + 1;
  buf = (char *) callocMustSucceed(size, sizeof(char), "calloc failed");
  strncpy(buf, plink->value.instio.string, size);
  buf[size - 1] = '\0';

  /* Parse 'device' and possibly 'led'*/
  if (sscanf(buf, "SYS,%c%c,", &device, &led) < 2) {
    if (sscanf(buf, "SYS,%c", &device) < 1) {
      errlogPrintf("devBoF3RP61SysCtl: can't get device for %s\n", pbo->name);
      pbo->pact = 1;
      return (-1);
    }
  }

  /* Check device validity*/
  if (!(device == 'L')) {
    errlogPrintf("devBoF3RP61SysCtl: illegal device for %s\n",
                 pbo->name);
    pbo->pact = 1;
    return (-1);
  }

  /* Check 'led' validity*/
  if (device == 'L') {
    if (!(led == 'R' || led == 'A' || led == 'E')) {
      errlogPrintf("devBoF3RP61SysCtl: illegal LED address for %s\n", pbo->name);
      pbo->pact = 1;
      return (-1);
    }
  }

  dpvt = (F3RP61SysCtl_BO_DPVT *) callocMustSucceed(1,
                  sizeof(F3RP61SysCtl_BO_DPVT),
                  "calloc failed");
  dpvt->device = device;
  if (device == 'L') {
    dpvt->led = led;
  }

  pbo->dpvt = dpvt;

  return(0);
}

/* Function is called when there was request to process the record.
 * According to the device (read in init_record) it sets commands and
 * data that is to be sent to driver and sends it. */
static long read_bo(boRecord *pbo)
{
  F3RP61SysCtl_BO_DPVT *dpvt = (F3RP61SysCtl_BO_DPVT *) pbo->dpvt;

  char device = dpvt->device;
  char led = dpvt->led;
  int command;
  unsigned long data = 0;

  /* Set command and data*/
  switch (device) {
  case 'L':
    command = M3SC_SET_LED;
    if (!(pbo->val)) {  /* When VAL field is 0*/
      switch (led) {
      case 'R':  /* Run LED*/
        data = M3SC_LED_RUN_OFF;
        break;
      case 'A':  /* Alarm LED*/
        data = M3SC_LED_ALM_OFF;
        break;
      default:   /* For 'E' Error LED*/
        data = M3SC_LED_ERR_OFF;
        break;
      }
    }
    else if(pbo->val == 1) {  /* When VAL field is 1. Should not use only 'else' because then Invalid Value will be treated as True also*/
      switch (led) {
      case 'R':  /* Run LED*/
        data = M3SC_LED_RUN_ON;
        break;
      case 'A':  /* Alarm LED*/
        data = M3SC_LED_ALM_ON;
        break;
      default:   /* For 'E' Error LED*/
        data = M3SC_LED_ERR_ON;
        break;
      }
    }
    break;
  default:
    command = M3SC_SET_LED;
    break;
  }

  /* Write to device*/
  if (device == 'L') {
    if (ioctl(f3rp61SysCtl_fd, command, &data) < 0) {
      errlogPrintf("devBoF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, pbo->name);
      return (-1);
    }
  }
  pbo->udf=FALSE;

  return(0);
}
