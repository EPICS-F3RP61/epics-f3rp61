################################################################
# Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
#
# f3rp61-test 1.3.0
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
################################################################

import datetime

#------------------------------------------------------------------------------

def commandTestCases(sshh_, log_): # NOT TO BE ADDED TO Test_Cases list but called from runIOC.py file
    # Write timestamp to log
    start = datetime.datetime.now()
    log_("--------------------------------------------------")
    log_("Manual Command Test started on " + start.strftime("%Y-%m-%d %H:%M:%S"))
    log_("--------------------------------------------------\n")

    # RUN LED
    sshh_.sendline('f3rp61SetLED R 1')
    log_ ('TC_Com1 Using f3rp61SetLED command to turn on RUN LED.')
    ans = raw_input("Is RUN LED turned ON and ALARM, ERROR LEDs turned OFF? (y/n): ")
    sshh_.sendline('f3rp61SetLED R 0')
    log_.assert_(ans, "y")#, 'RUN LED was not properly set.')
    
    # ALARM LED
    sshh_.sendline('f3rp61SetLED A 1')
    log_ ('TC_Com2 Using f3rp61SetLED command to turn on ALARM LED.')
    ans = raw_input("Is ALARM LED turned ON and RUN, ERROR LEDs turned OFF? (y/n): ")
    log_.assert_(ans, "y")#, 'ALARM LED was not properly set.')
    sshh_.sendline('f3rp61SetLED A 0')
    
    # ERROR LED
    sshh_.sendline('f3rp61SetLED E 1')
    log_ ('TC_Com3 Using f3rp61SetLED command to turn on ERROR LED.')
    ans = raw_input("Is ERROR LED turned ON and ALARM, RUN LEDs turned OFF? (y/n): ")
    log_.assert_(ans, "y")#, 'ERROR LED was not properly set.')
    sshh_.sendline('f3rp61SetLED E 0')
    
    # Check turned off
    log_ ('TC_Com4 Check turned-off state of LEDs.')
    ans = raw_input("Are all three LEDs turned OFF? (y/n): ")
    log_.assert_(ans, "y")#, 'LEDs are not being turned off as supposed to.')
    
    
