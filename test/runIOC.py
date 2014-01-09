################################################################
# Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
#
# f3rp61-test 1.3.0
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
################################################################


#import subprocess
import pexpect
import sys
import os
import time

from manualTestCom import commandTestCases

from wrapper import log


IP="10.5.2.100"
PASSWORD="root12"

# Run 'make' command to build the application
def buildApp():
    cwd = os.getcwd()
    os.chdir(cwd+"/iocTest")
    os.system("make clean uninstall")
    os.system("make")
    os.chdir(cwd)

# Use pexpect module to 'scp' application to f3rp61
def scpApp():
    scpp = pexpect.spawn('scp -r ./iocTest root@%s:/opt/epics/' % IP, timeout=30)
    scpp.logfile = sys.stdout
    try:
        i = scpp.expect(['password:', 'continue connecting (yes/no)?'])
        if i == 0:
            scpp.sendline(PASSWORD)
            scpp.expect(pexpect.EOF)
        elif i == 1:
            scpp.sendline('yes')
            scpp.expect('password:')
            scpp.sendline(PASSWORD)
            scpp.expect(pexpect.EOF)
    except pexpect.EOF:
        scpp.close()

# Using pexpect module 'ssh' to f3rp61 and start IOC
def runIOCautomated():
    sshh = pexpect.spawn('ssh root@%s' % IP)
    fd = open("/dev/null", "w")
    sshh.logfile = fd#sys.stdout
    try:
        i = sshh.expect(['password:', 'continue connecting (yes/no)?'])
        if i == 0 :
            sshh.sendline(PASSWORD)
        elif i == 1:
            sshh.sendline('yes')
            sshh.expect('password:')
            sshh.sendline(PASSWORD)
        #time.sleep(1)
        sshh.expect('-bash-')
        sshh.sendline('cd /opt/epics/iocTest/iocBoot/iocTest/')
        sshh.expect('-bash-')
        sshh.sendline('../../bin/linux-f3rp61/Test st.cmd')
        
        while(True):
            time.sleep(1)
                
        
    except pexpect.EOF:
        print("error")
        sshh.close()
        
def runIOCmanual():
    sshh = pexpect.spawn('ssh root@%s' % IP)
    fd = open("/dev/null", "w")
    sshh.logfile = fd#sys.stdout
    try:
        i = sshh.expect(['password:', 'continue connecting (yes/no)?'])
        if i == 0 :
            sshh.sendline(PASSWORD)
        elif i == 1:
            sshh.sendline('yes')
            sshh.expect('password:')
            sshh.sendline(PASSWORD)
        #time.sleep(1)
        sshh.expect('-bash-')
        sshh.sendline('cd /opt/epics/iocTest/iocBoot/iocTest/')
        sshh.expect('-bash-')
        sshh.sendline('../../bin/linux-f3rp61/Test st.cmd')
        
        Log = log("manualTestCom_Report.txt")
        commandTestCases(sshh, Log)
        Log.report()
        Log.close()
        print "Finished testing iocsh commands - proceed with the test in the other terminal."
        
        #sshh.interact()
        time.sleep(3)
        ans = 'e'
        while ans != 'exit':
            ans = raw_input("Type 'exit' when the test in the other terminal has finished : ")
        sshh.close()
                
    except pexpect.EOF:
        print("error")
        sshh.close()


if __name__ == "__main__":
    buildApp()
    scpApp()
    runIOCmanual()

