#ifndef DRVF3RP61SEQ_H
#define DRVF3RP61SEQ_H

//
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//
#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <dbAccess.h>
#include <dbCommon.h>
//#include <dbDefs.h>
#include <dbScan.h>
#include <devSup.h>
#include <drvSup.h>
#include <epicsEvent.h>
#include <epicsExport.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <errlog.h>
#include <iocsh.h>
#include <recGbl.h>
#include <recSup.h>

//
#if defined(__arm__)
#  include <ert3/m3lib.h>
#  define DEVFILE "/dev/m3cpu"
#elif defined(__powerpc__)
#  include <asm/fam3rtos/m3iodrv.h>
#  include <asm/fam3rtos/m3lib.h>
#  include <asm/fam3rtos/m3mcmd.h>
#  define DEVFILE "/dev/m3mcmd"
#else
#  error
#endif

//
#include "devF3RP61util.h"

//
#if defined(__powerpc__)
#  define M3CPU_ACCS_CMD        MCMD_ACCS
#  define M3CPU_SEND_SIG_EVENT  M3IO_SEND_SIG_EVENT
#  define M3CPU_GET_NUM         M3IO_GET_MYCPUNO
#  define M3CPU_GET_TYPE        M3IO_GET_CPUTYPE
#  define M3CPU_READ_COM        M3IO_READ_COM
#  define M3CPU_WRITE_COM       M3IO_WRITE_COM
#endif

//
typedef struct {
    ELLNODE      node;
    MCMD_STRUCT  mcmdStruct;
    CALLBACK     callback;
    int          ret;
    uint32_t     nord;    // number of elemetns to be read from or written to the PV (within the hardware limitations)
    //uint32_t     offset;  //
    //void        *buf;     // buffer for I/O
    int8_t       conv;
} F3RP61SEQ_DPVT;

//
int f3rp61seqQueueRequest();

// helper function(s)
int    f3rp61seqParseLink(const struct link *, F3RP61_RW, F3RP61_ACCESS_TYPE, dbCommon *, const dbfType, const uint32_t);
int8_t f3rp61seqGetDevice(F3RP61SEQ_DPVT *);

//
extern int f3rp61seqFd;

#endif // DRVF3RP61SEQ_H
