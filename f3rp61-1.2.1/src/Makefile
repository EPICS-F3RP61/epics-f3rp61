TOP=../

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

ifeq ($(T_A),linux-f3rp61)

LIBRARY_IOC += f3rp61

# install devXxxSoft.dbd into <top>/dbd
DBD += f3rp61.dbd

# The following are compiled and added to the Support library
f3rp61_SRCS += devAiF3RP61.c
f3rp61_SRCS += devAoF3RP61.c
f3rp61_SRCS += devBiF3RP61.c
f3rp61_SRCS += devBoF3RP61.c
f3rp61_SRCS += devLiF3RP61.c
f3rp61_SRCS += devLoF3RP61.c
f3rp61_SRCS += devMbbiDirectF3RP61.c
f3rp61_SRCS += devMbboDirectF3RP61.c
f3rp61_SRCS += devSoF3RP61.c
f3rp61_SRCS += devSiF3RP61.c
f3rp61_SRCS += devWfF3RP61.c
f3rp61_SRCS += drvF3RP61.c
f3rp61_SRCS += devAiF3RP61Seq.c
f3rp61_SRCS += devAoF3RP61Seq.c
f3rp61_SRCS += devBiF3RP61Seq.c
f3rp61_SRCS += devBoF3RP61Seq.c
f3rp61_SRCS += devLiF3RP61Seq.c
f3rp61_SRCS += devLoF3RP61Seq.c
f3rp61_SRCS += devMbbiDirectF3RP61Seq.c
f3rp61_SRCS += devMbboDirectF3RP61Seq.c
f3rp61_SRCS += drvF3RP61Seq.c

endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

