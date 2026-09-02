<!-- -*- coding: utf-8-unix -*- -->

Installing Device and Driver Support for F3RP70, F3RP71, and F3RP61
===================================================================

<!-- npx doctoc /path/to/Install.md -->
**Table of Contents**
<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [Introduction](#introduction)
- [Prerequisites](#prerequisites)
  - [Preparation for cross-compiling for F3RP71](#preparation-for-cross-compiling-for-f3rp71)
  - [Preparation for cross-compiling for F3RP61](#preparation-for-cross-compiling-for-f3rp61)
- [Cross-Building EPICS Base for F3RP71 and/or F3RP61](#cross-building-epics-base-for-f3rp71-andor-f3rp61)
  - [Extracting distribution file](#extracting-distribution-file)
  - [Install taget-specific definition files](#install-taget-specific-definition-files)
  - [Site-specific build configuration](#site-specific-build-configuration)
  - [Cross building EPICS base](#cross-building-epics-base)
- [Building the Device and Driver Support Library](#building-the-device-and-driver-support-library)
- [Using the Device and Driver Support with your IOC Application](#using-the-device-and-driver-support-with-your-ioc-application)
- [Using real-time scheduling with F3RP70/F3RP71/F3RP61-based IOC](#using-real-time-scheduling-with-f3rp70f3rp71f3rp61-based-ioc)
  - [References](#references)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Introduction

This document descibes the instructions for building the device and
driver support for F3RP70, F3RP71, and F3RP61. It covers following schenarios:
- Native-buinding on F3RP70.
  - Optionally, cross-buliding for F3RP70; however, setting up a cross-development environment for F3RP70 target is beyond the scope of this document, this will not be covered here.
- Cross-building on linux-x86_64 (or linux-x86) for F3RP71 and/or F3RP61.

# Prerequisites
- This device and driver support has been developed with EPICS base R7.0.10 and R3.15.9.
- To native-compile on F3RP70, one should prepare in the same way as installing EPICS on a Linux-machine. Refer to the following document: [Installation on Linux / MacOS](https://docs.epics-controls.org/en/latest/getting-started/installation-linux.html)
- To cross-compile for F3RP71 and/or F3RP61, install the Linux BSP and cross-development environment on the host machine.

## Preparation for cross-compiling for F3RP71
- This device and driver support requires Linux BSP for F3RP71 (SFRD12), R1.03 or later
- Refer to User's Manual for installation of Xilinx SDK and Linux BSP:
  - **IM 34M06M52-22E_002**, "e-RT3 Linux BSP (SFRD12) Programming Manual", 2. Building development environment
- In this document, it is assumed that Xilinx SDK and Linux BSP are installed in `/opt/Xilinx` on the host development environment.
  - We don't use Eclipse but GNU toolchain installed along with the SDK.

- Once you have installed the SDK and BSP, you may need to modify the location of header files:
```shell
cd /opt/Xilinx/SDK/2013.4/gnu/arm/lin/arm-xilinx-linux-gnueabi/libc/usr/include
mkdir ert3
cd ert3
ln -fs ../m3*.h .
```

## Preparation for cross-compiling for F3RP61
- This device and driver support requires Linux BSP for F3RP61 (SFRD11), R2.0x
- Refer to User's Manual for installation of development environment and Linux BSP:
  - **IM 34M06M51-43E**, "RTOS-CPU module (F3RP61-␣␣) Linux BSP Start-up Manual", 6. Introduction Methods
  - **IM 34M06M51-44E**, "RTOS-CPU module (F3RP61-␣␣) Linux BSP Reference Manual", 6. PLC Device Access, 6.4 User Interface
- In this document, it is assumed that Linux BSP is installed in `/opt/f3rp6x` on the host development environment.

The device and driver support depends on a run-time library,
libm3.so.1.0.0, which has to be installed manually in both the
development environment and the target userland.

- Install the library in the development environment:
```shell
cd /opt/f3rp6x/ppc_6xx/usr/lib
cp /path/to/BSP/yokogawa/library/libm3.so.1.0.0 .
ln -s libm3.so.1.0.0 libm3.so.1
ln -s libm3.so.1.0.0 libm3.so
```

- Install the library in the target userland:
```shell
cd /opt
mv libm3.so.1* /usr/lib
cd /usr/lib
ln -s libm3.so.1.0.0 libm3.so.1
ln -s libm3.so.1.0.0 libm3.so
ldconfig
```

# Cross-Building EPICS Base for F3RP71 and/or F3RP61
## Extracting distribution file
Untar the tar ball, `epics-f3rp61-2.0.0.tar.gz`, to an appropriate directory, e.g., `${EPICS_BASE}/../modules/src`:
```shell
mkdir -p ${EPICS_BASE}/../modules/src
tar -C ${EPICS_BASE}/../modules/src -x -f epics-f3rp61-2.0.0.tar.gz
```

Go to the top-level directory of the device / driver support:
```shell
cd ${EPICS_BASE}/../modules/src/epics-f3rp61-2.0.0/f3rp61
```

## Install taget-specific definition files
Copy target-specific definition files from top-level directory to `${EPICS_BASE}/configure/os/`:
- Definition files for F3RP71:
```
CONFIG.Common.linux-f3rp71
CONFIG.linux-f3rp71.Common
CONFIG.linux-x86.linux-f3rp71
CONFIG.linux-x86_64.linux-f3rp71
CONFIG_SITE.Common.linux-f3rp71
```
- Definition files for F3RP61:
```
CONFIG.Common.linux-f3rp61
CONFIG.linux-f3rp61.Common
CONFIG.linux-x86.linux-f3rp61
CONFIG.linux-x86_64.linux-f3rp61
CONFIG.linux-f3rp61.linux-f3rp61
```

## Site-specific build configuration

Edit `${EPICS_BASE}/configure/CONFIG_SITE.local` (or `$(EPICS_BASE)/configure/CONFIG_SITE`) and add
target-architectures to `CROSS_COMPILER_TARGET_ARCHS` variable.
- adding F3RP71 to the target:
```makefile
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp71
```

- adding F3RP61 to the target:
```makefile
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp61
```

- or adding both F3RP71 and F3RP61 to the target:
```makefile
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp71
CROSS_COMPILER_TARGET_ARCHS += linux-f3rp61
```

## Cross building EPICS base

If you have already built EPICS base for the host system, you will need to run `make distclean` in `$EPICS_BASE`:
```shell
cd ${EPICS_BASE}
make distclean
```

Now you are ready to build the EPICS base for linux-f3rp71 and/or linux-f3rp61:
```shell
cd ${EPICS_BASE}
make
```

# Building the Device and Driver Support Library

Edit
`${EPICS_BASE}/../modules/src/epics-f3rp61-2.0.0/f3rp61/configure/RELESE`
so that `EPICS_BASE` variable points your `$EPICS_BASE` correctly:

```makefile
EPICS_BASE=/path/to/epics/base
```

and build the device and driver support library:

```shell
cd ${EPICS_BASE}/../modules/src/epics-f3rp61-2.0.0/f3rp61
make
```

# Using the Device and Driver Support with your IOC Application

This section describes how to use the device and driver support components to your IOC application, assuming that the IOC source code has been generated using makeBaseApp.pl:
```
mkdir <top>
cd <top>
makeBaseApp.pl -t ioc my_ioc
makeBaseApp.pl -i -t ioc my_ioc
```

Enter you preferred target architecture when prompted. For example, `linux-f3p71`.

- In the `<top>configure/RELEASE` file set `F3RP61` to the location of the device and diver support:

```makefile
F3RP61 = ${EPICS_BASE}/../modules/src/epics-f3rp61-2.0.0/f3rp61
```

- In the `<top>configure/CONFIG_SITE` file specify your prefered target to `CROSS_COMPILER_TARGET_ARCHS`, e.g.:

```makefile
CROSS_COMPILER_TARGET_ARCHS = linux-f3rp71
```

- Modify the `<app>App/src/Makefile` to build only for the preferred target architecture. Don't forget to add device support dbd file and library:
```makefile
...
PROD_IOC = $(PROD_IOC_$(T_A))
PROD_IOC_linux-f3rp71 = my_ioc
...
my_ioc_DBD += f3rp61.dbd
...
my_ioc_LIBS += f3rp61
```

An example Makefile for ```exampleApp``` is included in this distribution:
```
epics-f3rp61-2.0.0/f3rp61/Makefile.exampleApp
```

# Using real-time scheduling with F3RP70/F3RP71/F3RP61-based IOC

F3RP70, F3RP71, and F3RP61 all support real-time linux kernel with
```CONFIG_PREEMPT_RT```.. If you choose this option, you might want to
choose a priority-based scheduling policy for real-time
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
