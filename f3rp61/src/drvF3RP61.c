/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Research Organization (KEK)
*
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* drvF3RP61.c - Driver Support Routines for F3RP61
*
*      Author: Jun-ichi Odagiri
*      Date: 6-30-08
*/

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <unistd.h>

#include <dbCommon.h>
#include <dbScan.h>
#include <drvSup.h>
#include <epicsExport.h>
#include <epicsThread.h>
#include <errlog.h>
#include <iocsh.h>
#include <recSup.h>

#include <drvF3RP61.h>

//
#define M3IO_NUM_CPUS   4

// A single F3RP61/71 module can work with up to two FL-net interface modules.
#define M3IO_NUM_LINKS  2

//
#define NUM_IO_INTR  8

typedef struct {
    int channel;
    dbCommon *prec;
} F3RP61_IO_SCAN;

typedef struct {
    F3RP61_IO_SCAN ioscan[NUM_IO_INTR];
    int count;
} F3RP61_IO_INTR;

static F3RP61_IO_INTR io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

//
typedef struct {
    long mtype;
#if defined(__arm__)
    M3IO_MSG_IO mtext;
#elif defined(__powerpc__)
    M3IO_IO_EVENT mtext;
#else
#  error
#endif
} MSG_BUF;

//
static long report();
static long init();

struct {
    long      number;
    DRVSUPFUN report;
    DRVSUPFUN init;
} drvF3RP61 = {
    2L,
    report,
    init,
};

epicsExportAddress(drvet, drvF3RP61);

int f3rp61_fd = -1;

//
static void msgrcv_thread(void *);
static M3LINKDATACONFIG link_data_config;
static M3COMDATACONFIG com_data_config;
static M3COMDATACONFIG ext_com_data_config;
static void linkDeviceConfigureCallFunc(const iocshArgBuf *);
static void comDeviceConfigureCallFunc(const iocshArgBuf *);
static void getModuleInfoCallFunc(const iocshArgBuf *);
static void linkDeviceConfigure(int, int, int);
static void comDeviceConfigure(int, int, int, int, int);
static void getModuleInfo(int);
static void drvF3RP61RegisterCommands(void);

//
static long report(void)
{
    return 0;
}

//
static long init(void)
{
    static int init_flag = 0;
    if (init_flag) {
        return 0;
    }
    init_flag = 1;

    f3rp61_fd = open("/dev/m3io", O_RDWR);
    if (f3rp61_fd < 0) {
        errlogPrintf("drvF3RP61: can't open /dev/m3io [%d] : %s\n", errno, strerror(errno));
        return -1;
    }

    for (int i = 0; i < M3IO_NUM_CPUS; i++) {
        if (com_data_config.wNumberOfRelay[i] || com_data_config.wNumberOfRegister[i] ||
            ext_com_data_config.wNumberOfRelay[i] || ext_com_data_config.wNumberOfRegister[i]) {

            if (setM3ComDataConfig(&com_data_config, &ext_com_data_config) < 0) {
                errlogPrintf("drvF3RP61: setM3ComDataConfig failed [%d]\n", errno);
                return -1;
            }

            break;
        }
    }

    for (int i = 0; i < M3IO_NUM_LINKS; i++) {
        if (link_data_config.wNumberOfRelay[i] || link_data_config.wNumberOfRegister[i]) {

            if (setM3LinkDeviceConfig(&link_data_config) < 0) {
                errlogPrintf("drvF3RP61: setM3LinkDeviceConfig failed [%d]\n", errno);
                return -1;
            }

            if (setM3FlnSysNo(0, NULL) < 0) {
                errlogPrintf("drvF3RP61: setM3FlnSysNo failed [%d]\n", errno);
                // 414 (invalid number)  : invalid parameter was specified (F3RP71/61)
                // 415 (device mismatch) : specified modules is not FL-net (F3RP71)
                // 416 (number over)     : an excessive number of system is specified (F3RP71)
                // 417 (entry error)     : unable to access module or I/O bus error (F3RP71)
                // 397 (internal error)  : (F3RP61)
                // 394                   : Not documented in the manual; setM3FlnSysNo() was called after m3rfrsTsk(). We'd better to reset the CPU.
                return -1;
            }

            if (m3rfrsTsk(10) < 0) {
                errlogPrintf("drvF3RP61: m3rfrsTsk failed [%d]\n", errno);
                return -1;
            }

            break;
        }

    }

    return 0;
}

//
static void msgrcv_thread(void *arg)
{
    int msqid = (int) arg;

    for (;;) {
        MSG_BUF msgbuf;
        const ssize_t val = msgrcv(msqid, &msgbuf, sizeof(MSG_BUF), M3IO_MSGTYPE_IO, MSG_NOERROR);

        if (val == -1) {
            errlogPrintf("drvF3RP61: msgrcv failed [%d] : %s\n", errno, strerror(errno));
            // msgbuf might be uninitialized if msgrcv() failed.
            continue;
        }

        if (val < 16) {
            // for just in case
            errlogPrintf("drvF3RP61: message received by msgrcv() is too small (%d bytes)\n", val);
            continue;
        }

        const int unit    = msgbuf.mtext.unit;
        const int slot    = msgbuf.mtext.slot;
        const int channel = msgbuf.mtext.channel;

        for (int i = 0; i < io_intr[unit][slot].count; i++) {
            if (io_intr[unit][slot].ioscan[i].channel == channel) {
                dbCommon *prec = io_intr[unit][slot].ioscan[i].prec;
                if (!prec) {
                    errlogPrintf("drvF3RP61: no record for interrupt (U%d,S%d,C%d)\n", unit, slot, channel);
                    break;
                }

                if (prec->scan == SCAN_IO_EVENT) {
                    IOSCANPVT ioscanpvt = *((IOSCANPVT *) prec->dpvt);
                    scanIoRequest(ioscanpvt);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
//
long f3rp61_register_io_interrupt(dbCommon *prec, int unit, int slot, int channel)
{
    char thread_name[32];
    int msqid = 0;
    int count = io_intr[unit][slot].count;

    if (count == NUM_IO_INTR) {
        errlogPrintf("drvF3RP61: no interrupt slot\n");
        return -1;
    }

    // Create a SysV message queue and a thread which reveices the message
    if (count == 0) {
        msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
        if (msqid  == -1) {
            errlogPrintf("drvF3RP61: msgget failed [%d] : %s\n", errno, strerror(errno));
            return -1;
        }

#if defined(__powerpc__)
        if (msqid == 0) {
            // Get another message queue ID when it's 0.
            // Message queue id 0 is valid in SysV IPC but invalid in F3RP61 BSP.
            msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
            if (msqid == -1) {
                errlogPrintf("drvF3RP61: msgget failed [%d] : %s\n", errno, strerror(errno));
                return -1;
            }
        }
#endif

        sprintf(thread_name, "msgrcvr%d", msqid);
        if (epicsThreadCreate(thread_name,
                              epicsThreadPriorityHigh,
                              epicsThreadGetStackSize(epicsThreadStackSmall),
                              (EPICSTHREADFUNC) msgrcv_thread,
                              (void *)msqid) == 0) {
            errlogPrintf("drvF3RP61: epicsThreadCreate failed\n");
            return -1;
        }
    }

    io_intr[unit][slot].ioscan[count].channel = channel;
    io_intr[unit][slot].ioscan[count].prec = prec;
    io_intr[unit][slot].count++;

    for (int i = 0; i < count; i++) {
        if (io_intr[unit][slot].ioscan[i].channel == channel) {
            return 0;
        }
    }

    M3IO_INTER_DEFINE arg = {
        .unitno = unit,
        .slotno = slot,
        .defData.relayNo = channel,
        .msgQId = msqid,
    };

    if (ioctl(f3rp61_fd, M3IO_ENABLE_INTER, &arg) < 0) {
        errlogPrintf("drvF3RP61: ioctl failed [%d]\n", errno);
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Get io interrupt info
//
long f3rp61GetIoIntInfo(int cmd, dbCommon *pxx, IOSCANPVT *ppvt)
{
    if (!pxx->dpvt) {
        errlogPrintf("drvF3RP61: f3rp61GetIoIntInfo is called with null dpvt\n");
        return -1;
    }

    if ( *((IOSCANPVT *) pxx->dpvt) == NULL) {
        scanIoInit((IOSCANPVT *) pxx->dpvt);
    }

    *ppvt = *((IOSCANPVT *) pxx->dpvt);

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61LinkDeviceConfigure'
//
static const iocshArg linkDeviceConfigureArg0 = { "sysNo",iocshArgInt};
static const iocshArg linkDeviceConfigureArg1 = { "nRlys",iocshArgInt};
static const iocshArg linkDeviceConfigureArg2 = { "nRegs",iocshArgInt};
static const iocshArg *linkDeviceConfigureArgs[] = {
    &linkDeviceConfigureArg0,
    &linkDeviceConfigureArg1,
    &linkDeviceConfigureArg2
};

static const iocshFuncDef linkDeviceConfigureFuncDef = {
    "f3rp61LinkDeviceConfigure",
    3,
    linkDeviceConfigureArgs
};

static void linkDeviceConfigureCallFunc(const iocshArgBuf *args)
{
    linkDeviceConfigure(args[0].ival, args[1].ival, args[2].ival);
}

static void linkDeviceConfigure(int sysno, int nrlys, int nregs)
{
    if (sysno <0 || sysno > (M3IO_NUM_LINKS-1) ) {
        errlogPrintf("f3rp61LinkDeviceConfigure: number of FL-net interface out of range\n");
        return;
    }
    if (nrlys < 1 || nrlys > 8192) {
        errlogPrintf("f3rp61LinkDeviceConfigure: number of Link relay out of range\n");
        return;
    }
    if (nregs < 1 || nregs > 8192) {
        errlogPrintf("f3rp61LinkDeviceConfigure: number of Link register out of range\n");
        return;
    }

    link_data_config.wNumberOfRelay[sysno] = nrlys;
    link_data_config.wNumberOfRegister[sysno] = nregs;
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61ComDeviceConfigure'
//
static const iocshArg comDeviceConfigureArg0 = { "cpuNo",     iocshArgInt};
static const iocshArg comDeviceConfigureArg1 = { "nRlys",     iocshArgInt};
static const iocshArg comDeviceConfigureArg2 = { "ext_nRlys", iocshArgInt};
static const iocshArg comDeviceConfigureArg3 = { "nRegs",     iocshArgInt};
static const iocshArg comDeviceConfigureArg4 = { "ext_nRegs", iocshArgInt};
static const iocshArg *comDeviceConfigureArgs[] = {
    &comDeviceConfigureArg0,
    &comDeviceConfigureArg1,
    &comDeviceConfigureArg2,
    &comDeviceConfigureArg3,
    &comDeviceConfigureArg4
};

static const iocshFuncDef comDeviceConfigureFuncDef = {
    "f3rp61ComDeviceConfigure",
    5,
    comDeviceConfigureArgs
};

static void comDeviceConfigureCallFunc(const iocshArgBuf *args)
{
    comDeviceConfigure(args[0].ival, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
}

static void comDeviceConfigure(int cpuno, int nrlys, int nregs, int ext_nrlys, int ext_nregs)
{
    if (cpuno < 0 || cpuno > 3 ||
        nrlys < 0 || nrlys >  2048 || nregs < 0 || nregs > 1024 ||
        ext_nrlys < 0 || ext_nrlys >  2048 || ext_nregs < 0 || ext_nregs > 3072) {
        errlogPrintf("drvF3RP61: comDeviceConfigure: parameter out of range\n");
        return;
    }

    com_data_config.wNumberOfRelay[cpuno] = nrlys;
    com_data_config.wNumberOfRegister[cpuno] = nregs;
    ext_com_data_config.wNumberOfRelay[cpuno] = ext_nrlys;
    ext_com_data_config.wNumberOfRegister[cpuno] = ext_nregs;
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'f3rp61GetModuleInfo'
//
// usage: f3rp61GetModuleInfo [arg]
//
// List FA-M3/e-RT3 modules installed on the system.
// Empty slots are shown if whatever argument is given.
//
static const iocshArg getModuleInfoArg0 = { "empty",     iocshArgString};
static const iocshArg *getModuleInfoArgs[] = {
    &getModuleInfoArg0,
};
static const iocshFuncDef getModuleInfoFuncDef = {
    "f3rp61GetModuleInfo",
    1,
    getModuleInfoArgs
};

static void getModuleInfoCallFunc(const iocshArgBuf *args)
{
    if (args[0].sval) {
        getModuleInfo(1);
    } else {
        getModuleInfo(0);
    }
}

static void getModuleInfo(int verbosity)
{
    printf("%4s %4s %4s %5s %4s %4s %4s\n",
           "Unit", "Slot", "Name", "MSize", "Xreg", "Yreg", "Dreg");
    for (int unit=0; unit<M3IO_NUM_UNIT; unit++) {
        for (int slot=1; slot<M3IO_NUM_SLOT + 1; slot++) {

            M3IO_MODULE_INFORMATION module_info = {
                .unitno = unit,
                .slotno = slot,
            };

            ioctl(f3rp61_fd, M3IO_GET_MODULE_INFO, &module_info);

            if (!module_info.enable) {
                if (!verbosity) {
                    continue;
                }
                module_info.name[3] =
                module_info.name[2] =
                module_info.name[1] =
                module_info.name[0] = '-';
            }

            char name[5];
            memcpy(name, module_info.name, 4);
            name[4] = '\0';
            printf("%4d %4d %4s %5d %4d %4d %4d\n",
                   module_info.unitno,
                   module_info.slotno,
                   name,
                   module_info.msize,
                   module_info.num_xreg,
                   module_info.num_yreg,
                   module_info.num_dreg);
        }
    }
}

static void drvF3RP61RegisterCommands(void)
{
    static int init_flag = 0;
    if (init_flag) {
        return;
    }
    init_flag = 1;

    iocshRegister(&getModuleInfoFuncDef, getModuleInfoCallFunc);
    iocshRegister(&comDeviceConfigureFuncDef, comDeviceConfigureCallFunc);
    iocshRegister(&linkDeviceConfigureFuncDef, linkDeviceConfigureCallFunc);
}

epicsExportRegistrar(drvF3RP61RegisterCommands);
