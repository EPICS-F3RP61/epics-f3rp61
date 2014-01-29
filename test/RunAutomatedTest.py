################################################################
# Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
#
# f3rp61-test 1.3.0
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
################################################################

import time
import sys
import os
from multiprocessing import Process

import runIOC


def main():
    runIOC.buildApp()
    runIOC.scpApp()
    
    #cmd="gnome-terminal -x " + sys.executable + " runIOC.py"
    #p = subprocess.Popen(cmd.split(), stdin=subprocess.PIPE)
    #p = Process(target=runIOC.runIOC, args=('0'))
    p = Process(target=runIOC.runIOCautomated)
    p.start()
    #os.system("gnome-terminal -x " + sys.executable + " runIOC.py")
    
    # wait for IOC to start
    time.sleep(5)
    os.system(sys.executable + " automatedTest.py &> generated-reports/automatedTest_Output.txt")
    
    p.terminate()
    #os.killpg(p.pid, signal.SIGTERM)

if __name__ == "__main__":
    main()
    print "\n\n***********************************************"
    print "\n**********  FINISHED  **********\n"
    print "***********************************************\n\n"
