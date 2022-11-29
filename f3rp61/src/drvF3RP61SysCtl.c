/*************************************************************************
* Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
*
* F3RP61 Device Support 1.3.0
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
**************************************************************************
* drvF3RP61.c - Driver Support Routines for F3RP61
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
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <dbCommon.h>
#include <dbScan.h>
#include <drvSup.h>
#include <epicsExport.h>
#include <errlog.h>
#include <iocsh.h>
#include <recSup.h>

#include <drvF3RP61SysCtl.h>

static long report();
static long init();

struct {
    long      number;
    DRVSUPFUN report;
    DRVSUPFUN init;
} drvF3RP61SysCtl = {
    2L,
    report,
    init,
};

epicsExportAddress(drvet, drvF3RP61SysCtl);

int f3rp61SysCtl_fd = -1;

static void setLEDCallFunc(const iocshArgBuf *);
static void setLED(const char, const int);
static void drvF3RP61SysCtlRegisterCommands(void);

//
static long report(void)
{
    return 0;
}

// Open and store m3sysctl file descriptor
static long init(void)
{
    static int init_flag = 0;
    if (init_flag) {
        return 0;
    }
    init_flag = 1;

    f3rp61SysCtl_fd = open("/dev/m3sysctl", O_RDWR);
    if (f3rp61SysCtl_fd < 0) {
        errlogPrintf("drvF3RP61SysCtl: can't open /dev/m3sysctl [%d] : %s\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Register iocsh command 'setLED'
//
// Usage: setLED <led> <value>
//      led: R|A|E|1|2|3
//      value: 0=OFF, non-zero=ON
//
static const iocshArg setLEDArg0 = {"led",   iocshArgString};
static const iocshArg setLEDArg1 = {"value", iocshArgInt};
static const iocshArg *setLEDArgs[] = {
    &setLEDArg0,
    &setLEDArg1
};

static const iocshFuncDef setLEDFuncDef = {
    "f3rp61SetLED",
    2,
    setLEDArgs
};

static void setLEDCallFunc(const iocshArgBuf *args)
{
    if (! args[0].sval) {
        printf("Usage: %s %s %s\n", setLEDFuncDef.name, setLEDArg0.name, setLEDArg1.name);
        return;
    }

    setLED(args[0].sval[0], args[1].ival);
}

static void setLED(const char led, const int value)
{
    // Check 'led' validity
    if (led != 'R' && led != 'A' && led != 'E'
#ifdef M3SC_LED_US3_ON // it is assumed that US1 and US2 are also defined
        && led != '1' && led != '2' && led != '3'
#endif
        ) {
        errlogPrintf("drvF3RP61SysCtl: f3rp61SetLED: invalid led\n");
        return;
    }

    // Check 'value' validity
    //if (!(value == 1 || value == 0)) {
    //    errlogPrintf("drvF3RP61SysCtl: f3rp61SetLED: value out of range\n");
    //    return;
    //}


    // Compose data to write
    unsigned long cmd = 0;
    unsigned long data = 0;

    switch (led) {
#ifdef M3SC_LED_US3_ON // it is assumed that US1 and US2 are also defined
    case '1':  // US1 LED
        cmd = M3SC_SET_US_LED;
        data = value ? M3SC_LED_US1_ON : M3SC_LED_US1_OFF;
        break;
    case '2':  // US2 LED
        cmd = M3SC_SET_US_LED;
        data = value ? M3SC_LED_US2_ON : M3SC_LED_US2_OFF;
        break;
    case '3':  // US3 LED
        cmd = M3SC_SET_US_LED;
        data = value ? M3SC_LED_US3_ON : M3SC_LED_US3_OFF;
        break;
#endif
    case 'R':  // Run LED
        cmd = M3SC_SET_LED;
        data = value ? M3SC_LED_RUN_ON : M3SC_LED_RUN_OFF;
        break;
    case 'A':  // Alarm LED
        cmd = M3SC_SET_LED;
        data = value ? M3SC_LED_ALM_ON : M3SC_LED_ALM_OFF;
        break;
    default:   //Error LED
        cmd = M3SC_SET_LED;
        data = value ? M3SC_LED_ERR_ON : M3SC_LED_ERR_OFF;
        break;
    }

    // Issue API function
    if (ioctl(f3rp61SysCtl_fd, cmd, &data) < 0) {
        errlogPrintf("drvF3RP61SysCtl: ioctl failed [%d] for f3rp61setLED\n", errno);
        return;
    }
}

static void drvF3RP61SysCtlRegisterCommands(void)
{
    static int init_flag = 0;
    if (init_flag) {
        return;
    }
    init_flag = 1;

    iocshRegister(&setLEDFuncDef, setLEDCallFunc);
}

epicsExportRegistrar(drvF3RP61SysCtlRegisterCommands);
