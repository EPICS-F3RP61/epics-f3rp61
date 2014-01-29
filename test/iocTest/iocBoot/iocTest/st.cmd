#!../../bin/linux-x86/Test

## You may have to change Test to something else
## everywhere it appears in this file

#< envPaths

## Register all support components
dbLoadDatabase("../../dbd/Test.dbd",0,0)
Test_registerRecordDeviceDriver(pdbbase) 

## Load record instances
dbLoadRecords("../../db/test.db","user=gregor")
dbLoadRecords("../../db/test_li.db","user=gregor")
dbLoadRecords("../../db/test_lo.db","user=gregor")
dbLoadRecords("../../db/test_mbbi.db","user=gregor")
dbLoadRecords("../../db/test_mbbo.db","user=gregor")
dbLoadRecords("../../db/testSeq.db","user=gregor")
dbLoadRecords("../../db/testSysCtl.db","user=gregor")

f3rp61ComDeviceConfigure(0,512,256,0,1024)
f3rp61ComDeviceConfigure(1,512,256,0,2048)

f3rp61LinkDeviceConfigure(0, 512, 256)
f3rp61LinkDeviceConfigure(1, 512, 256)


iocInit()

## Start any sequence programs
#seq sncTest,"user=gregor"
