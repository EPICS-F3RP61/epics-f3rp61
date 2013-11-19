#!../../bin/linux-f3rp61/epsample

## You may have to change epsample to something else
## everywhere it appears in this file

#< envPaths

## Register all support components
dbLoadDatabase("../../dbd/epsample.dbd",0,0)
epsample_registerRecordDeviceDriver(pdbbase)

## Configure shared memory of F3RP61 and other CPUs
f3rp61ComDeviceConfigure(0,128,256,0,0)
f3rp61ComDeviceConfigure(1,128,256,0,0)

## Load record instances
dbLoadRecords("../../db/epsample.db")

iocInit()

## Start any sequence programs
#seq sncepsample,"user=root"
