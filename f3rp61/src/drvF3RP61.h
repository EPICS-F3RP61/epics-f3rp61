#ifndef DRVF3RP61_H
#define DRVF3RP61_H

//
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//
#include <alarm.h>
//#include <callback.h>
#include <cantProceed.h>
#include <dbAccess.h>
#include <dbCommon.h>
//#include <dbDefs.h>
#include <dbScan.h>
#include <devSup.h>
#include <drvSup.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <errlog.h>
#include <iocsh.h>
#include <recGbl.h>
#include <recSup.h>

//
#if defined(__arm__)
#  include <ert3/m3lib.h>
#elif defined(__powerpc__)
#  include <asm/fam3rtos/m3iodrv.h>
#  include <asm/fam3rtos/m3lib.h>
#else
#  error
#endif

// public functions
int  f3rp61RegisterIoInterrupt(const dbCommon *, int, int, int);
int  f3rp61EnableIoInterrupt(void);
long f3rp61GetIoIntInfo(int, dbCommon *, IOSCANPVT *);

//
extern int f3rp61_fd;

#endif // DRVF3RP61_H
