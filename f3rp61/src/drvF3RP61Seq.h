#ifndef DRVF3RP61SEQ_H
#define DRVF3RP61SEQ_H

#include <fcntl.h>
#if defined(__arm__)
#  include <m3lib.h>
#  define DEVFILE "/dev/m3cpu"
#elif defined(__powerpc__)
#  include <asm/fam3rtos/m3iodrv.h>
#  include <asm/fam3rtos/m3mcmd.h>
#  define DEVFILE "/dev/m3mcmd"
#else
#  error
#endif

#if defined(__powerpc__)
#  define M3CPU_ACCS_CMD        MCMD_ACCS
#  define M3CPU_SEND_SIG_EVENT  M3IO_SEND_SIG_EVENT
#  define M3CPU_GET_NUM         M3IO_GET_MYCPUNO
#  define M3CPU_GET_TYPE        M3IO_GET_CPUTYPE
#  define M3CPU_READ_COM        M3IO_READ_COM
#  define M3CPU_WRITE_COM       M3IO_WRITE_COM
#endif

// Access type for sequence CPU device
typedef enum {
    kBit   = 0x00,
    kWord  = 0x02,
    // kLong = 0x04, // F3RP71 native API does not suport long-word access
} F3RP61_ACCESS_TYPE;

//
typedef struct {
    ELLNODE      node;
    MCMD_STRUCT  mcmdStruct;
    dbCommon    *prec;
    CALLBACK     callback;
    int          ret;
    char         option;
} F3RP61_SEQ_DPVT;

int f3rp61Seq_queueRequest();

extern int f3rp61Seq_fd;

#endif // DRVF3RP61SEQ_H
