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


def main():
    # Run runIOC.py in a new terminal
    os.system("gnome-terminal -x " + sys.executable + " runIOC.py")
    #p = Process(target=runIOC.runIOC)
    #p.start()
        
    # wait for IOC to start
    time.sleep(8)
    # wait for user to finish testing iocsh Commands
    ans = 'n'
    while ans != 'y':
        ans = raw_input("Is the test in the other terminal finished (y/n)? ")
        
    os.system(sys.executable + " manualTest.py")# &> reports/automatedTest_Output.txt")
    
    #p.terminate()

if __name__ == "__main__":
    main()
    print "\n\n***********************************************"
    print "**********  FINISHED  **********"
    print "***********************************************\n\n"
