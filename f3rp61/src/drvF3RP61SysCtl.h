#ifndef DRVF3RP61SYSCTL_H
#define DRVF3RP61SYSCTL_H

#if defined(__arm__)
#  include <m3lib.h>
#elif defined(__powerpc__)
#  include <asm/fam3rtos/fam3rtos_sysctl.h>
#  include <asm/fam3rtos/m3lib.h>
#else
#  error
#endif

#if defined(__powerpc__)
/* ioctl */
#  define M3SC_SET_LED      RP6X_SYSIOC_SETLED
#  define M3SC_GET_LED      RP6X_SYSIOC_GETLED
#  define M3SC_GET_SW       RP6X_SYSIOC_GETSW
#  define M3SC_CHECK_BAT    RP6X_SYSIOC_STATUSREGREAD
/* led parameter */
#  define LED_RUN_FLG       RP6X_LED_RUN_MASK
#  define LED_ALM_FLG       RP6X_LED_ALM_MASK
#  define LED_ERR_FLG       RP6X_LED_ERR_MASK
#  define M3SC_LED_RUN_OFF  RP6X_LED_RUN_OFF
#  define M3SC_LED_ALM_OFF  RP6X_LED_ALM_OFF
#  define M3SC_LED_ERR_OFF  RP6X_LED_ERR_OFF
#  define M3SC_LED_RUN_ON   RP6X_LED_RUN_ON
#  define M3SC_LED_ALM_ON   RP6X_LED_ALM_ON
#  define M3SC_LED_ERR_ON   RP6X_LED_ERR_ON
#endif

extern int f3rp61SysCtl_fd;

#endif /* DRVF3RP61SYSCTL_H */
