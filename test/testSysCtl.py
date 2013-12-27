################################################################
# Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
#
# f3rp61-test 1.3.0
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
################################################################


import unittest
import time

from wrapper import pv


# RELEVANT RECORDS:
    # TEST:biSysCtl_LED_A        # TEST:biSysCtl_LED_E
    # TEST:biSysCtl_LED_R        # TEST:biSysCtl_REG
    # TEST:boSysCtl_LED_A        # TEST:boSysCtl_LED_E
    # TEST:boSysCtl_LED_R        # TEST:mbbiSysCtl_SWITCH
    
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
# Test class for SysCtl driver ------------------------------------------------
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
class SysCtlTest(unittest.TestCase):
    
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
    
    
    SysCtlPVs=[pv_biSysCtl_LED_R, pv_biSysCtl_LED_A, pv_biSysCtl_LED_E, pv_boSysCtl_LED_R, pv_boSysCtl_LED_A, pv_boSysCtl_LED_E, pv_mbbiSysCtl_SW]
    
    def setUp(self):
        print('Set up.')
        for i, val in enumerate(self.SysCtlPVs):
            self.SysCtlPVs[i].write_silent(0, 0)
        time.sleep(2*self.SCAN_TIME)
            
    def tearDown(self):
        time.sleep(0.01)
        print('\nClean up.')
        for i, val in enumerate(self.SysCtlPVs):
            self.SysCtlPVs[i].write_silent(0, 0)
        time.sleep(2*self.SCAN_TIME)
                  
    def test_TC_01(self):
        print ('TC-01: RUN LED, bo, bi, R, A, E LEDs')
        print ('TC-01 Turns RUN LED on and reads the status of all three LEDs.')
        
        # RUN LED=1, Check if set and if all others were not changed
        new_state = 1
        self.pv_boSysCtl_LED_R.write(new_state, self.SCAN_TIME)
        
        self.assertEqual(self.pv_biSysCtl_LED_R.read(), new_state, 'The desired status of LED \'R\' '+str(new_state)+' was not set.')
        self.assertNotEqual(self.pv_biSysCtl_LED_A.read(), new_state, 'Status of LED \'A\' should not have changed.')
        self.assertNotEqual(self.pv_biSysCtl_LED_E.read(), new_state, 'Status of LED \'E\' should not have changed.')
    
    def test_TC_02(self):
        print ('TC-02: ALARM LED, bo, bi, R, A, E LEDs')
        print ('TC-02 Turns ALARM LED on and reads the status of all three LEDs.')
        
        # RUN LED=1, Check if set and if all others were not changed
        new_state = 1
        self.pv_boSysCtl_LED_A.write(new_state, self.SCAN_TIME)
        
        self.assertEqual(self.pv_biSysCtl_LED_A.read(), new_state, 'The desired status of LED \'A\' '+str(new_state)+' was not set.')
        self.assertNotEqual(self.pv_biSysCtl_LED_R.read(), new_state, 'Status of LED \'R\' should not have changed.')
        self.assertNotEqual(self.pv_biSysCtl_LED_E.read(), new_state, 'Status of LED \'E\' should not have changed.')
    
    def test_TC_03(self):
        print ('TC-03: ERROR LED, bo, bi, R, A, E LEDs')
        print ('TC-03 Turns ERROR LED on and reads the status of all three LEDs.')
        
        # RUN LED=1, Check if set and if all others were not changed
        new_state = 1
        self.pv_boSysCtl_LED_E.write(new_state, self.SCAN_TIME)
        
        self.assertEqual(self.pv_biSysCtl_LED_E.read(), new_state, 'The desired status of LED \'E\' '+str(new_state)+' was not set.')
        self.assertNotEqual(self.pv_biSysCtl_LED_R.read(), new_state, 'Status of LED \'R\' should not have changed.')
        self.assertNotEqual(self.pv_biSysCtl_LED_A.read(), new_state, 'Status of LED \'A\' should not have changed.')
    
    def test_TC_04(self):
        print ('TC-04: LEDs, invalid input, bo, bi, R, A, E LEDs')
        print ('TC-04 checks LED behavior when invalid input string is given')
        
        # Put string to RUN LED
        new_state = "ON/OF"
        self.pv_boSysCtl_LED_R.write(new_state, self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_biSysCtl_LED_R.read("STRING"), "OFF", 'Status of LED \'R\' should not have changed.')
        self.assertEqual(self.pv_biSysCtl_LED_A.read("STRING"), "OFF", 'Status of LED \'A\' should not have changed.')
        self.assertEqual(self.pv_biSysCtl_LED_E.read("STRING"), "OFF", 'Status of LED \'E\' should not have changed.')
        #time.sleep(0.05)
        
    def test_TC_05(self):
        print ('TC-05: LEDs, out-of-range input, bo, bi, R, A, E LEDs')
        print ('TC-05 checks LED behavior when out-of-range input is given')
        # Put out-of-range integer to RUN LED
        new_state = 4
        self.pv_boSysCtl_LED_R.write(new_state, self.SCAN_TIME)
        
        self.assertEqual(self.pv_biSysCtl_LED_R.read(), 0, 'Status of LED \'R\' should not have changed.')
        self.assertEqual(self.pv_biSysCtl_LED_A.read(), 0, 'Status of LED \'A\' should not have changed.')
        self.assertEqual(self.pv_biSysCtl_LED_E.read(), 0, 'Status of LED \'E\' should not have changed.')
        #time.sleep(0.05)
        
        
        
