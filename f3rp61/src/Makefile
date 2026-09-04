TOP=../

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

USR_CFLAGS += -std=c99
USR_CFLAGS += -Wall
#USR_CFLAGS += -Werror
USR_CPPFLAGS += -DUSE_TYPED_RSET -DUSE_TYPED_DSET

#==================================================
# build a support library

LIBRARY_IOC = $(LIBRARY_IOC_$(T_A))

LIBRARY_IOC_linux-arm += f3rp61
LIBRARY_IOC_linux-f3rp70 += f3rp61
LIBRARY_IOC_linux-f3rp71 += f3rp61
LIBRARY_IOC_linux-f3rp61 += f3rp61

f3rp61_SYS_LIBS_Linux += m3

# install devXxxSoft.dbd into <top>/dbd
DBD += f3rp61.dbd

# install header file(s) into <top>/include
INC += drvF3RP61.h

# The following are compiled and added to the Support library
f3rp61_SRCS += devAaiF3RP61.c
f3rp61_SRCS += devAaoF3RP61.c
f3rp61_SRCS += devAiF3RP61.c
f3rp61_SRCS += devAoF3RP61.c
f3rp61_SRCS += devBiF3RP61.c
f3rp61_SRCS += devBoF3RP61.c
f3rp61_SRCS += devLiF3RP61.c
f3rp61_SRCS += devLoF3RP61.c
f3rp61_SRCS += devMbbiF3RP61.c
f3rp61_SRCS += devMbboF3RP61.c
f3rp61_SRCS += devMbbiDirectF3RP61.c
f3rp61_SRCS += devMbboDirectF3RP61.c
f3rp61_SRCS += devSoF3RP61.c
f3rp61_SRCS += devSiF3RP61.c
f3rp61_SRCS += devWfF3RP61.c
f3rp61_SRCS += drvF3RP61.c

f3rp61_SRCS += devAaiF3RP61Seq.c
f3rp61_SRCS += devAaoF3RP61Seq.c
f3rp61_SRCS += devAiF3RP61Seq.c
f3rp61_SRCS += devAoF3RP61Seq.c
f3rp61_SRCS += devBiF3RP61Seq.c
f3rp61_SRCS += devBoF3RP61Seq.c
f3rp61_SRCS += devLiF3RP61Seq.c
f3rp61_SRCS += devLoF3RP61Seq.c
f3rp61_SRCS += devMbbiF3RP61Seq.c
f3rp61_SRCS += devMbboF3RP61Seq.c
f3rp61_SRCS += devMbbiDirectF3RP61Seq.c
f3rp61_SRCS += devMbboDirectF3RP61Seq.c
f3rp61_SRCS += devWfF3RP61Seq.c
f3rp61_SRCS += drvF3RP61Seq.c

f3rp61_SRCS += devBiF3RP61SysCtl.c
f3rp61_SRCS += devBoF3RP61SysCtl.c
f3rp61_SRCS += devMbbiF3RP61SysCtl.c
f3rp61_SRCS += drvF3RP61SysCtl.c
f3rp61_SRCS += devF3RP61util.c

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
