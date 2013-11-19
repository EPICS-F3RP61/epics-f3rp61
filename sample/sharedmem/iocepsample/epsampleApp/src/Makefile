TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

ifeq ($(T_A), linux-f3rp61)

#=============================
# Build the IOC application

PROD_IOC = epsample
# epsample.dbd will be created and installed
DBD += epsample.dbd

# epsample.dbd will be made up from these files:
epsample_DBD += base.dbd

# Include dbd files from all support applications:
epsample_DBD += f3rp61.dbd

# Add all the support libraries needed by this IOC
epsample_LIBS += f3rp61

PROD_LDLIBS += -lm3
USR_LDFLAGS += -L/opt/bsp/yokogawa/library

# epsample_registerRecordDeviceDriver.cpp derives from epsample.dbd
epsample_SRCS += epsample_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
epsample_SRCS_DEFAULT += epsampleMain.cpp
epsample_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#epsample_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
epsample_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

