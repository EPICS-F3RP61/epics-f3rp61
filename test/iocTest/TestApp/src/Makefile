TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

ifeq ($(T_A),linux-f3rp61)

PROD_IOC = Test
# Test.dbd will be created and installed
DBD += Test.dbd

# Test.dbd will be made up from these files:
Test_DBD += base.dbd

# Include dbd files from all support applications:
Test_DBD += f3rp61.dbd

# Add all the support libraries needed by this IOC
Test_LIBS += f3rp61

PROD_LDLIBS += -lm3

USR_LDFLAGS += -L/opt/bsp/yokogawa/library

# Test_registerRecordDeviceDriver.cpp derives from Test.dbd
Test_SRCS += Test_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
Test_SRCS_DEFAULT += TestMain.cpp
Test_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#Test_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
Test_LIBS += $(EPICS_BASE_IOC_LIBS)

endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

