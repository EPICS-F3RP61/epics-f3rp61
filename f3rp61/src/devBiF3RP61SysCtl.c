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
#if defined(_arm_)
#  include <m3sysctl.h>
#  include <m3lib.h>
#elif defined(_ppc_)
#  include <asm/fam3rtos/fam3rtos_sysctl.h>
#  include <asm/fam3rtos/m3lib.h>
#  define M3SC_GET_LED      RP6X_SYSIOC_GETLED
#  define LED_RUN_FLG       RP6X_LED_RUN_MASK
#  define LED_ALM_FLG       RP6X_LED_ALM_MASK
#  define LED_ERR_FLG       RP6X_LED_ERR_MASK
#  define M3SC_LED_RUN_OFF  RP6X_LED_RUN_OFF
#  define M3SC_LED_ALM_OFF  RP6X_LED_ALM_OFF
#  define M3SC_LED_ERR_OFF  RP6X_LED_ERR_OFF
#  define M3SC_LED_RUN_ON   RP6X_LED_RUN_ON
#  define M3SC_LED_ALM_ON   RP6X_LED_ALM_ON
#  define M3SC_LED_ERR_ON   RP6X_LED_ERR_ON
#  define M3SC_CHECK_BAT    RP6X_SYSIOC_STATUSREGREAD
#else
#  error
#endif

extern int f3rp61SysCtl_fd;

/* Create the dset for devBiF3RP61SysCtl */
static long init_record();
static long read_bi();
struct {
  long       number;
  DEVSUPFUN  report;
  DEVSUPFUN  init;
  DEVSUPFUN  init_record;
  DEVSUPFUN  get_ioint_info;
  DEVSUPFUN  read_bi;
} devBiF3RP61SysCtl = {
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
  char device = 'N'; /* Valid states: L (LED), R (Status Register)*/
  char led;          /* Valid states: R (Run), A (Alarm), E (Error)*/

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
  if (device != 'L' && device != 'R'
#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
      && device != 'U'
#endif
      ) {
    errlogPrintf("devBiF3RP61SysCtl: illegal device for %s\n", pbi->name);
    pbi->pact = 1;
    return (-1);
  }

  /* Check 'led' validity*/
  if (device == 'L') {
    if (led != 'R' && led != 'A' && led != 'E' ) {
      errlogPrintf("devBiF3RP61SysCtl: illegal LED address for %s\n", pbi->name);
      pbi->pact = 1;
      return (-1);
    }
  }
#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
  else if (device == 'U') {
    if (led != '1' && led != '2' && led != '3') {
      errlogPrintf("devBiF3RP61SysCtl: illegal USER LED address for %s\n", pbi->name);
      pbi->pact = 1;
      return (-1);
    }
  }
#endif

  dpvt = (F3RP61SysCtl_BI_DPVT *) callocMustSucceed(1,
                          sizeof(F3RP61SysCtl_BI_DPVT),
                          "calloc failed");
  dpvt->device = device;
  if (device == 'L' || device == 'U') {
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
    command = M3SC_GET_LED;
    break;
#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
  case 'U': /* For User LED */
    command = M3SC_GET_US_LED;
    break;
#endif
  default: /* For device 'R'*/
    command = M3SC_CHECK_BAT;
    break;
  }

  if (ioctl(f3rp61SysCtl_fd, command, &data) < 0) {
     errlogPrintf("devBiF3RP61SysCtl: ioctl failed [%d] for %s\n", errno, pbi->name);
     return (-1);
  }

  pbi->udf=FALSE;

  switch (device) {
  case 'L':
    switch (led) {
    case 'R':
      pbi->rval = (unsigned long) (data & LED_RUN_FLG);
      break;
    case 'A':
      pbi->rval = (unsigned long) (data & LED_ALM_FLG);
      break;
    case 'E':
      pbi->rval = (unsigned long) (data & LED_ERR_FLG);
      break;
    }
    break;
#ifdef M3SC_LED_US3_ON /* it is assumed that US1 and US2 are also defined */
  case 'U':
    switch (led) {
    case '1':
      pbi->rval = (unsigned long) (data & LED_US1_FLG);
      break;
    case '2':
      pbi->rval = (unsigned long) (data & LED_US2_FLG);
      break;
    case '3':
      pbi->rval = (unsigned long) (data & LED_US3_FLG);
      break;
    }
    break;
#endif
  case 'R':
    pbi->rval = (unsigned long) (data & 0x00000004);
    break;
  }

  /* convert */
  return(0);
}
