#ifndef DRVF3RP61_H
#define DRVF3RP61_H

#if defined(_arm_)
#  include <m3lib.h>
#elif defined(_ppc_)
#  include <asm/fam3rtos/m3iodrv.h>
#  include <asm/fam3rtos/m3lib.h>
#else
#  error
#endif

#define NUM_IO_INTR  8

typedef struct {
    int channel;
    dbCommon *prec;
} F3RP61_IO_SCAN;

typedef struct {
    F3RP61_IO_SCAN ioscan[NUM_IO_INTR];
    int count;
} F3RP61_IO_INTR;

typedef struct {
    long mtype;
#if defined(_arm_)
    M3IO_MSG_IO mtext;
#elif defined(_ppc_)
    M3IO_IO_EVENT mtext;
#else
#  error
#endif
} MSG_BUF;

long f3rp61GetIoIntInfo(int, dbCommon *, IOSCANPVT *);
long f3rp61_register_io_interrupt(dbCommon *, int, int, int);

extern int f3rp61_fd;
extern F3RP61_IO_INTR f3rp61_io_intr[M3IO_NUM_UNIT][M3IO_NUM_SLOT];

#endif /* DRVF3RP61_H */
