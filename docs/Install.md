<!-- -*- coding: utf-8-unix -*- -->

Install
=======

# Table Of Contents
* [Introduction](#introduction)
* [Software Requirements](#software-requirements)
   * [F3RP71 (e-RT3 plus)](#f3rp71-e-rt3-plus)
   * [F3RP61 (e-RT3 2.0)](#f3rp61-e-rt3-20)
* [Building EPICS Base for F3RP71 and/or F3RP61](#building-epics-base-for-f3rp71-andor-f3rp61)
   * [Extracting distribution file](#extracting-distribution-file)
   * [Install definition files](#install-definition-files)
   * [Site-specific build configuration](#site-specific-build-configuration)
* [Building the Device / Driver Support Library](#building-the-device--driver-support-library)
* [Using the Device / Driver Support with IOC Application](#using-the-device--driver-support-with-ioc-application)
* [Using real-time scheduling with F3RP71/F3RP61-based IOC](#using-real-time-scheduling-with-f3rp71f3rp61-based-ioc)
   * [References](#references)

# Introduction

This document describes instructions for cross-building the device and
driver support for F3RP71 and F3RP61 on linux-x86_64 or linux-x86 host.

# Software Requirements
## F3RP71 (e-RT3 plus)
- EPICS base R3.15.5 or R3.14.12.x
- Linux BSP for F3RP71 (SFRD12), R1.03
  - Refer to User's Manual for installation of BSP:
    - **IM 34M06M52-22E_002**, "e-RT3 Linux BSP (SFRD12) Programming Manual", 2. Building development environment
  - It is assumed that Xilinx SDK and Linux BSP are installed in /opt/Xilinx.

## F3RP61 (e-RT3 2.0)
- EPICS base R3.15.5, R3.14.12.x, or R3.14.11
- Linux BSP for F3RP61 (SFRD11), R2.0x
  - Refer to User's Manual for installation of BSP:
    - **IM 34M06M51-43E**, "RTOS-CPU module (F3RP61-␣␣) Linux BSP Start-up Manual", 6. Introduction Methods
    - **IM 34M06M51-44E**, "RTOS-CPU module (F3RP61-␣␣) Linux BSP Reference Manual", 6. PLC Device Access, 6.4 User Interface
  - It is assumed that Linux BSP is installed in /opt/f3rp6x.

The device and driver support depends on a run-time library,
libm3.so.1.0.0, which has to be installed manually both on the
development environment and userland.

- Install the library on the development environment:
```shell
cd /opt/f3rp6x/ppc_6xx/usr/lib
cp /path/to/BSP/yokogawa/library/libm3.so.1.0.0 .
ln -s libm3.so.1.0.0 libm3.so.1
ln -s libm3.so.1.0.0 libm3.so
```

- Install the library on userland:
```shell
cd /opt
mv libm3.so.1* /usr/lib
cd /usr/lib
ln -s libm3.so.1.0.0 libm3.so.1
ln -s libm3.so.1.0.0 libm3.so
ldconfig
```

# Building EPICS Base for F3RP71 and/or F3RP61
## Extracting distribution file
Untar the tar ball, `epics-f3rp61-2.0.0.tar.gz`, to an appropriate directory, e.g., `${EPICS_BASE}/../modules/instrument`:
```shell
mkdir -p ${EPICS_BASE}/../modules/instrument
tar -C ${EPICS_BASE}/../modules/instrument -x -f epics-f3rp61-2.0.0.tar.gz
```

Go to the top-level directory of the device / driver support:
```shell
cd ${EPICS_BASE}/../modules/instrument/epics-f3rp61-2.0.0/f3rp61
```

## Install definition files
Copy os-architecture specific definition files from top-level directory to `${EPICS_BASE}/configure/os/`:
- Definitions for F3RP71:
```
CONFIG.Common.linux-f3rp71
CONFIG.linux-f3rp71.Common
CONFIG.linux-x86.linux-f3rp71
CONFIG.linux-x86_64.linux-f3rp71
CONFIG_SITE.Common.linux-f3rp71
```
- Definitions for F3RP61:
```
CONFIG.Common.linux-f3rp61
CONFIG.linux-f3rp61.Common
CONFIG.linux-x86.linux-f3rp61
CONFIG.linux-x86_64.linux-f3rp61
CONFIG.linux-f3rp61.linux-f3rp61
```

## Site-specific build configuration

Edit `${EPICS_BASE}/configure/CONFIG_SITE` and add
target-architectures to `CROSS_COMPILER_TARGET_ARCHS` variable.
- for F3RP71 support:
```makefile
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp71
```

- for F3RP61 support:
```makefile
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp61
```

- for both F3RP71 and F3RP61 support:
```makefile
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp71
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp61
```

Now you are ready to build the EPICS base for linux-f3rp71 and/or linux-f3rp61:

```shell
make
```

# Building the Device / Driver Support Library

Edit
`${EPICS_BASE}/../modules/instrument/epics-f3rp61-2.0.0/f3rp61/configure/RELESE`
so that `EPICS_BASE` variable points your `$EPICS_BASE` correctly:

```makefile
EPICS_BASE=/path/to/epics/base
```

and build the device / driver support library:

```shell
cd ${EPICS_BASE}/../modules/instrument/epics-f3rp61-2.0.0/f3rp61
make
```

# Using the Device / Driver Support with IOC Application

This section explains how to include the device / driver support components to your IOC application.
- In the `configure/RELEASE` file add definition for `F3RP61`:

```makefile
F3RP61 = ${EPICS_BASE}/../modules/instrument/epics-f3rp61-2.0.0/f3rp61
```

- In the `configure/CONFIG_SIZE` file add linux-f3rp71 (or linux-f3rp61) to `CROSS_COMPILER_TARGET_ARCHS`:

```makefile
CROSS_COMPILER_TARGET_ARCHS = linux-f3rp71
# CROSS_COMPILER_TARGET_ARCHS = linux-f3rp61
```

- In the `<app>App/src/Makefile` file:
```makefile
...
ifneq ($(filter $(T_A), linux-f3rp61 linux-f3rp71),)
PROD_IOC = <app>
endif
...
<app>_dbd += f3rp61.dbd
...
<app>_LIBS += f3rp61
PROD_LDLIBS += -lm3
```

An example Makefile for ```exampleApp``` is included in the distribution:
```
${EPICS_BASE}/../modules/instrument/epics-f3rp61-2.0.0/f3rp61/Makefile.testApp
SampleMakefileForTestApp.
```

# Using real-time scheduling with F3RP71/F3RP61-based IOC

Both F3RP71 and F3RP61 supports real-time linux kernel with
```CONFIG_PREEMPT_RT``` patch set. If you choose this option, you
might want to choose a priority-based scheduling policy for real-time
responsiveness. The choice of the scheduling policy is subject to
EPICS base. In `${EPICS_BASE}/configure/CONFIG_SITE` change

```makefile
USE_POSIX_THREAD_PRIORITY_SCHEDULING = NO
```
to
```makefile
USE_POSIX_THREAD_PRIORITY_SCHEDULING = YES
```

You might also want to call mlockall() in your <app>Main.cpp to make
your IOC process memory resident.

Note that you need to be careful so as **NOT** to run any relevant
threads that execute a busy loop if you choose the scheduling
policy. Otherwise, what you will have gotten is what you should have
gotten.

## References
- [How To Use Posix Thread Priority Scheduling under Linux](https://wiki-ext.aps.anl.gov/epics/index.php/How_To_Use_Posix_Thread_Priority_Scheduling_under_Linux)
