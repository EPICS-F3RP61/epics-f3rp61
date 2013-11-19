Sample EPICS application for Yokogawa F3RP61

 =======
 History
 =======

  2012-08-19 Takashi Nakamoto (Cosylab)
   * Created

 =======
 Summary
 =======

  This sample EPICS application acquires analog data from Analog Input
  module and show 

  This application conceptually demonstrates the communication between
  Sequence CPU and EPICS IOC on F3RP61 (Linux CPU). In this application,
  Analog Input module converts analog signal into digital integer value
  and Sequence CPU converts it into float value. EPICS IOC reads the
  float value from Sequence CPU via shared register and provide it
  to Channel Access clients.

  Other shared relays and registers are used to exchange configuration
  data between Sequence CPU and EPICS IOC.

 ===================
 Directory structure
 ===================

   * README.txt:   This file.

   * EPSAMPLE:     Project directory for WideField3.
                   Program for Sequence CPU is inclded in this directory.

   * iocepsample:  EPICS IOC for F3RP61.

   * opi:          Project directory for Control System Studio.
                   Operator interfaces are included in this directory.

   * shared_memory_map.xlsm:
                   Definition of shared relays and shared registeres.

   * screen_shot.png:
                   Screen shot of operator interface running on Control 
                   System Studio Basic EPICS 3.1.0.

 ============
 Requirements
 ============

 Following 4 (or 3) machines must be prepared to install and run this 
 application. For some of the machines, TCP/IP network must be prepared
 where static IP allocation allowed for F3RP61.

  1. Host Machine for Sequence CPU

   * It must be able to run WideField3 to load PLC program and download
     it to Yokogawa FA-M3 PLC.

   * IT can be a virtual machine running on, for example, VMWare Player.

  2. Host Machine for F3RP61

   * It must be Linux (preferably Cent OS 5.5) to compile EPICS IOC.

   * It can be a virtual machine running on, for example, VMWare Player.

   * See the following page to prepare development environment on this
     machine. EPICS base 3.14.11 (or later), device/driver support
     for F3RP61 (version 1.2.0) and Board Support Package 2.01 for F3RP61
     must be installed in this machine.
     http://www-linac.kek.jp/cont/epics/f3rp61/

   * EPICS base, device/driver support for F3RP61 and Board Support
     Package must be installed in the following directories.

     /opt/epics/base                 : Compiled EPICS base 3.14.11
     /opt/epics/modules/f3rp61-1.2.0 : Compiled device/drier support
                                       for F3RP61 (version 1.2.0).
     /opt/bsp                        : Board Support Package for F3RP61
                                       version 2.01.

  3. Yokogawa FA-M3 PLC

   * It must have the following modules in the given slots.
     [slot 1] Sequence CPU (e.g. F3SP71-4S)
     [slot 2] F3RP61-xx (F3RP61-2R or F3RP61-2L)
     [slot 6] F3AD04-0V

   * Other modules should be installed in slot 3 - 5.

   * Appropriate userland must be installed in CF card in F3RP61.

   * EPICS base 3.14.11 must be compiled and installed in CF card in
     F3RP61. The installation location must be /opt/epics/base.
     See the following page to know how to build EPICS base 3.14.11
     for F3RP61.
     http://www-linac.kek.jp/cont/epics/f3rp61/

   * Device/driver support for F3RP61 (version 1.2.0) must be compiled
     and installed in CF card in F3RP61. The installation location must
     be /opt/epics/modules/f3rp61-1.2.0.
     http://www-linac.kek.jp/cont/epics/f3rp61/

   * F3RP61 must be attached to TCP/IP network where static IP allocation
     is allowed. Use the upper Ethernet port to join the network.
     Configure IP address following the standard Linux convention.

  4. OPI Machine

   * It must be able to run Control System Studio Basic EPICS 3.1.0
     (or later) to see acuired analog data and configure the settins.

   * It can be the same as one of Host Machines.

   * It must be connected to the network where F3RP61 is attached.

 ============
 Installation
 ============

  1. Download the program to Sequence CPU. 

   a) Open a project in EPSAMPLE with WideField3.

   b) Download the program in this project to Sequence CPU via USB or
      Ethernet.

  2. Compile and install EPICS IOC to F3RP61.

   a) Copy iocepsample/ to Host Machine for F3RP61.

   b) Run "make" in iocepsample/.

   c) Copy iocepsample/ to CF card in F3RP61.
      (e.g. copy to /opt/epics/apps/iocepsample)

  3. Run Sequence CPU and EPICS IOC.

   a) Power on Yokogawa FA-M3 PLC.
      The program downloaded to Sequence CPU automatically starts to
      run.

   b) Login Linux on F3RP61 and run "st.cmd" in
      iocepsample/iocBoot/iocepsample/.

  4. Open Operator Interface.

   a) Launch Control System Studio and import opi/ directory into the
      workspace.

   b) Configure addr_list in Preferences appropriately.
     
   c) Open opi/epsample.opi in OPI Runtime mode.
      If everything went well, you will see the operator screen as
      shown in screen_shot.png.
      
