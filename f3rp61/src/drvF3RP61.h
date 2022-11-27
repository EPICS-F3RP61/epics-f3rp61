#ifndef DRVF3RP61_H
#define DRVF3RP61_H

#if defined(__arm__)
#  include <m3lib.h>
#elif defined(__powerpc__)
#  include <asm/fam3rtos/m3iodrv.h>
#  include <asm/fam3rtos/m3lib.h>
#else
#  error
#endif

long f3rp61GetIoIntInfo(int, dbCommon *, IOSCANPVT *);
long f3rp61_register_io_interrupt(dbCommon *, int, int, int);

extern int f3rp61_fd;

#endif /* DRVF3RP61_H */
