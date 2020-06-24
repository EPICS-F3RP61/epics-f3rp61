/*************************************************************************
* Copyright (c) 2008 High Energy Accelerator Reseach Organization (KEK)
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

#define M3IO_NUM_CPUS   4
/*
#define M3IO_NUM_LINKS  2
*/
#define M3IO_NUM_LINKS  1

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

int f3rp61_fd;

static F3RP61_IO_INTR io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];
static int init_flag;
static void msgrcv_thread(void *);
static M3LINKDATACONFIG link_data_config;
static M3COMDATACONFIG com_data_config;
static M3COMDATACONFIG ext_com_data_config;
static void linkDeviceConfigureCallFunc(const iocshArgBuf *);
static void comDeviceConfigureCallFunc(const iocshArgBuf *);
static void getModuleInfoCallFunc(const iocshArgBuf *);
static void linkDeviceConfigure(int, int, int);
static void comDeviceConfigure(int, int, int, int, int);
static void getModuleInfo(void);
static void drvF3RP61RegisterCommands(void);

/* */
static long report(void)
{
    return (0);
}

static long init(void)
{
    if (init_flag) {
        return (0);
    }
    init_flag = 1;

    f3rp61_fd = open("/dev/m3io", O_RDWR);
    if (f3rp61_fd < 0) {
        errlogPrintf("drvF3RP61: can't open /dev/m3io [%d]\n", errno);
        return (-1);
    }

    for (int i = 0; i < M3IO_NUM_CPUS; i++) {
        if (com_data_config.wNumberOfRelay[i] || com_data_config.wNumberOfRegister[i] ||
            ext_com_data_config.wNumberOfRelay[i] || ext_com_data_config.wNumberOfRegister[i]) {

            if (setM3ComDataConfig(&com_data_config, &ext_com_data_config) < 0) {
                errlogPrintf("drvF3RP61: setM3ComDataConfig failed [%d]\n", errno);
                return (-1);
            }

            break;
        }
    }

    for (int i = 0; i < M3IO_NUM_LINKS; i++) {
        if (link_data_config.wNumberOfRelay[i] || link_data_config.wNumberOfRegister[i]) {

            if (setM3LinkDeviceConfig(&link_data_config) < 0) {
                errlogPrintf("drvF3RP61: setM3LinkDeviceConfig failed [%d]\n", errno);
                return (-1);
            }

            if (setM3FlnSysNo(0, NULL) < 0) {
                errlogPrintf("drvF3RP61: setM3FlnSysNo failed [%d]\n", errno);
                return (-1);
            }

            if (m3rfrsTsk(10) < 0) {
                errlogPrintf("drvF3RP61: m3rfrsTsk failed [%d]\n", errno);
                return (-1);
            }

            break;
        }
    }

    return (0);
}


static void msgrcv_thread(void *arg)
{
    int msqid = (int) arg;

    for (;;) {
        MSG_BUF msgbuf;
        if (msgrcv(msqid, &msgbuf, sizeof(MSG_BUF), M3IO_MSGTYPE_IO, 0) == -1) {
            errlogPrintf("drvF3RP61: msgrcv failed [%d]\n", errno);
        }

        int unit    = msgbuf.mtext.unit;
        int slot    = msgbuf.mtext.slot;
        int channel = msgbuf.mtext.channel;

        for (int i = 0; i < io_intr[unit][slot].count; i++) {
            if (io_intr[unit][slot].ioscan[i].channel == channel) {
                dbCommon *prec = io_intr[unit][slot].ioscan[i].prec;
                if (!prec) {
                    errlogPrintf("drvF3RP61: no record for interrupt (U%d,S%d,C%d)\n",
                                 unit, slot, channel);
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

long f3rp61_register_io_interrupt(dbCommon *prec, int unit, int slot, int channel)
{
    char thread_name[32];
    int msqid = 0;
    int count = io_intr[unit][slot].count;

    if (count == NUM_IO_INTR) {
        errlogPrintf("drvF3RP61: no interrupt slot\n");
        return (-1);
    }

    if (count == 0) {
        msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
        if (msqid  == -1) {
            errlogPrintf("drvF3RP61: msgget failed [%d]\n", errno);
            return (-1);
        }

        /* Add Start */
        msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
        if (msqid == -1) {
            errlogPrintf("drvF3RP61: msgget failed[ %d]\n", errno);
            return (-1);
        }
        /* Add End */

        sprintf(thread_name, "msgrcvr%d", msqid);
        if (epicsThreadCreate(thread_name,
                              epicsThreadPriorityHigh,
                              epicsThreadGetStackSize(epicsThreadStackSmall),
                              (EPICSTHREADFUNC) msgrcv_thread,
                              (void *)msqid) == 0) {
            errlogPrintf("drvF3RP61: epicsThreadCreate failed\n");
            return (-1);
        }
    }

    io_intr[unit][slot].ioscan[count].channel = channel;
    io_intr[unit][slot].ioscan[count].prec = prec;
    io_intr[unit][slot].count++;

    for (int i = 0; i < count; i++) {
        if (io_intr[unit][slot].ioscan[i].channel == channel) {
            return (0);
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
        return (-1);
    }

    return (0);
}

/*******************************************************************************
 * Get io interrupt info
 *******************************************************************************/
long f3rp61GetIoIntInfo(int cmd, dbCommon *pxx, IOSCANPVT *ppvt)
{
    if (!pxx->dpvt) {
        errlogPrintf("drvF3RP61: f3rp61GetIoIntInfo is called with null dpvt\n");
        return (-1);
    }

    if ( *((IOSCANPVT *) pxx->dpvt) == NULL) {
        scanIoInit((IOSCANPVT *) pxx->dpvt);
    }

    *ppvt = *((IOSCANPVT *) pxx->dpvt);

    return (0);
}

/*******************************************************************************
 * Register iocsh command 'f3rp61LinkDeviceConfigure'
 *******************************************************************************/
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
    /*
      if (sysno < 0 || sysno > 1 || nrlys < 1 || nrlys > 8192 || nregs < 1 || nregs > 8192) {
    */
    if (sysno < 0 || sysno > 0 || nrlys < 1 || nrlys > 8192 || nregs < 1 || nregs > 8192) {
        errlogPrintf("drvF3RP61: linkDeviceConfigure: parameter out of range\n");
        return;
    }

    link_data_config.wNumberOfRelay[sysno] = nrlys;
    link_data_config.wNumberOfRegister[sysno] = nregs;
}

/*******************************************************************************
 * Register iocsh command 'f3rp61ComDeviceConfigure'
 *******************************************************************************/
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

/*******************************************************************************
 * Register iocsh command 'f3rp61GetModuleInfo'
 *******************************************************************************/
static const iocshFuncDef getModuleInfoFuncDef = {
    "f3rp61GetModuleInfo",
    0,
    NULL
};

static void getModuleInfoCallFunc(const iocshArgBuf *args)
{
    getModuleInfo();
}

static void getModuleInfo(void)
{
    for (int i = 0; i < M3IO_NUM_UNIT; i++) {
        for (int j = 1; j < M3IO_NUM_SLOT + 1; j++) {

            M3IO_MODULE_INFORMATION module_info = {
                .unitno = i,
                .slotno = j,
            };

            ioctl(f3rp61_fd, M3IO_GET_MODULE_INFO, &module_info);

            printf("unitno: %0d  ", module_info.unitno);
            printf("slotno: %02d  ", module_info.slotno);
            printf("enable: %d  ", module_info.enable);

            for (int k = 0; k < 4; k++) {
                if (isalnum(module_info.name[k])) {
                    printf("%c", module_info.name[k]);
                }
                else {
                    printf("%c", ' ');
                }
            }

            printf("  msize: %5d  ", module_info.msize);
            printf("  num_xreg: %02d  ", module_info.num_xreg);
            printf("  num_yreg: %02d  ", module_info.num_yreg);
            printf("  num_dreg: %02d  ", module_info.num_dreg);
            printf("\n");
        }
    }
}

static void drvF3RP61RegisterCommands(void)
{
    static int firstTime = 1;

    if (firstTime) {
        iocshRegister(&getModuleInfoFuncDef, getModuleInfoCallFunc);
        iocshRegister(&comDeviceConfigureFuncDef, comDeviceConfigureCallFunc);
        iocshRegister(&linkDeviceConfigureFuncDef, linkDeviceConfigureCallFunc);
        firstTime = 0;
    }
}

epicsExportRegistrar(drvF3RP61RegisterCommands);
