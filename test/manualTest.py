################################################################
# Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
#
# f3rp61-test 1.3.0
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
################################################################


import time
import datetime
import sys

from wrapper import pv
from wrapper import log



# RELEVANT RECORDS:
    # TEST:biSysCtl_LED_A        # TEST:biSysCtl_LED_E
    # TEST:biSysCtl_LED_R        # TEST:biSysCtl_REG
    # TEST:boSysCtl_LED_A        # TEST:boSysCtl_LED_E
    # TEST:boSysCtl_LED_R        # TEST:mbbiSysCtl_SWITCH
    
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
#  Manual Test class ----------------------------------------------------------
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------

class manualTest():
    
    ### DEFINITIONS ######
    SCAN_TIME = 0.15    # Input records scan time in seconds + 0.01 second
    ######################
    
    pv_biSysCtl_LED_R = pv('TEST:biSysCtl_LED_R')
    pv_biSysCtl_LED_A = pv('TEST:biSysCtl_LED_A')
    pv_biSysCtl_LED_E = pv('TEST:biSysCtl_LED_E')
    
    pv_boSysCtl_LED_R = pv('TEST:boSysCtl_LED_R')
    pv_boSysCtl_LED_A = pv('TEST:boSysCtl_LED_A')
    pv_boSysCtl_LED_E = pv('TEST:boSysCtl_LED_E')
    
    pv_mbbiSysCtl_SW = pv('TEST:mbbiSysCtl_SWITCH')
    pv_biSysCtl_REG = pv('TEST:biSysCtl_REG')
    
    
    SysCtlPVs=[pv_biSysCtl_LED_R, pv_biSysCtl_LED_A, pv_biSysCtl_LED_E, pv_boSysCtl_LED_R, pv_boSysCtl_LED_A, pv_boSysCtl_LED_E, pv_mbbiSysCtl_SW, pv_biSysCtl_REG]
    
    def setup_method(self):
        print('\nSet up.')
        for i, val in enumerate(self.SysCtlPVs):
            self.SysCtlPVs[i].write_silent(0, 0)
        time.sleep(2*self.SCAN_TIME)
            
    def test_TC_01(self, log_):
        log_ ('TC-01: RUN LED, bo')
        log_ ('TC-01 Turns RUN LED on and acquires user input on the LED status.')
        
        log_ ('Turning on RUN LED...')
        new_state = 1
        self.pv_boSysCtl_LED_R.write(new_state, self.SCAN_TIME)
        
        ans = raw_input("Is RUN LED turned ON and ALARM, ERROR LEDs turned OFF? (y/n): ")
                
        log_.assert_(ans, "y")#, 'RUN LED was not properly set.')
        
    def test_TC_02(self, log_):
        log_ ('TC-02: ALARM LED, bo')
        log_ ('TC-02 Turns ALARM LED on and acquires user input on the LED status.')
        
        log_ ('Turning on ALARM LED...')
        new_state = 1
        self.pv_boSysCtl_LED_A.write(new_state, self.SCAN_TIME)
        
        ans = raw_input("Is ALARM LED turned ON and RUN, ERROR LEDs turned OFF? (y/n): ")
                
        log_.assert_ (ans,"y")#, 'ALARM LED was not properly set.')
    
    def test_TC_03(self, log_):
        log_ ('TC-03: ERROR LED, bo')
        log_ ('TC-03 Turns ERROR LED on and acquires user input on the LED status.')
        
        log_ ('Turning on ERROR LED...')
        new_state = 1
        self.pv_boSysCtl_LED_E.write(new_state, self.SCAN_TIME)
        
        ans = raw_input("Is ERROR LED turned ON and RUN, ALARM LEDs turned OFF? (y/n): ")
                
        log_.assert_ (ans,"y")#, 'ERROR LED was not properly set.')        
    
    def test_TC_04(self, log_):
        log_ ('TC-04: SWITCH, mbbi')
        log_ ('TC-04 Reads switch position and acquires user input on it.')
        
        log_ ('Reading switch position...')
        
        pos = self.pv_mbbiSysCtl_SW.read("STRING")
        
        ans = raw_input("Enter position of the switch during f3rp61-linux boot time (0-15): ")
                
        log_.assert_ (ans, pos)#, 'Entered Switch position does not match read position.')
        
    def test_TC_05(self, log_):
        log_ ('TC-05: REGISTER, bi')
        log_ ('TC-05 Reads register status and acquires user input on it.')
        
        log_ ('Reading register status...')
        
        status = self.pv_biSysCtl_REG.read("STRING")
        
        #ans = raw_input("Enter status of system register (OK/ALARM): ")
        ans = raw_input("Enter status of battery LED (ON/OFF): ")
        # To match status register enums
        if ans == 'ON':
            ans = 'ALARM'
        if ans == 'OFF':
            ans = 'OK'
        log_.assert_ (ans, status)#, 'Entered status does not match read status.')

#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
    
if __name__ == '__main__':
    log = log("manualTest_Report.txt")
    # Write timestamp to log
    start = datetime.datetime.now()
    log("Manual Test started on " + start.strftime("%Y-%m-%d %H:%M:%S"))
    # start the test
    manualTest = manualTest()
    
    # Add new test cases to this list in order for them to be run
    Test_Cases = [manualTest.test_TC_01, manualTest.test_TC_02, manualTest.test_TC_03,\
               manualTest.test_TC_04, manualTest.test_TC_05]
    
    # Run each test case
    for case in Test_Cases:
        manualTest.setup_method()
        case(log)
    
    # Print total and close the file
    log.report()
    log.close()
        
