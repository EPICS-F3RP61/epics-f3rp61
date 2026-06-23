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

#include <linux/version.h>
#include <sys/utsname.h>

//
//#include <alarm.h>
//#include <callback.h>
#include <cantProceed.h>
#include <dbAccess.h>
#include <dbCommon.h>
//#include <dbDefs.h>
#include <dbScan.h>
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

//
typedef enum {
    // Thse values have no significance, yet they are kept consistent
    // with drvF3RP61Seq.h
    kRead  = 0x01,
    kWrite = 0x02,
} F3RP61_RW;

// Access type for module access
typedef enum {
    // Thse values have no significance, yet they are kept consistent
    // with drvF3RP61Seq.h
    kBit  = 0x00,
    kWord = 0x02,
} F3RP61_ACCESS_TYPE;

//
typedef struct {
    uint8_t   conv;    // conversion specifier
    // Device for I/O
    uint8_t   device;  // device type
    uint8_t   unit;    // unit number     (0, 1, ..., 7)
    uint8_t   slot;    // slot number     (1, 2, ..., 16)
    uint32_t  addr;    // position number (0, ....)
    uint8_t   count;   // data width      (1, 2, or 4)
    uint8_t   cpuno;   // for Shared memory (or 'Old interface' for shared registers/relays)
    // Source of I/O interrupt
    uint8_t   irqunit; // unit number     (0, 1, ..., 7)
    uint8_t   irqslot; // slot number     (1, 2, ..., 16)
    uint8_t   irqaddr; // position number (1, 2, ..., 32)
    //
    void     *buf;     // buffer for I/O
} F3RP61_DPVT;

//
#define PARSE_ERROR -1
#define OPTION_ERROR -2

// callback functions
long f3rp61Init(int after);
long f3rp61GetIoIntInfo(int, dbCommon *, IOSCANPVT *);

// helper function(s)
int f3rp61ParseLink(const struct link *, F3RP61_RW, F3RP61_ACCESS_TYPE, const dbCommon *, const uint32_t);

//
extern int f3rp61_fd;

#endif // DRVF3RP61_H
