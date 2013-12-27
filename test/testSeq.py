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
    # TEST:liSeq_B               # TEST:liSeq_B_optB
    # TEST:liSeq_D               # TEST:liSeq_D_optB
    # TEST:loSeq_B               # TEST:loSeq_B_optB
    # TEST:loSeq_D               # TEST:loSeq_D_optB
    # TEST:mbbiSeq_B             # TEST:mbbiSeq_D
    # TEST:mbboSeq_B             # TEST:mbboSeq_D
    
    
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
# Test class for Seq driver ------------------------------------------------
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
class SeqTest(unittest.TestCase):
    
    ### DEFINITIONS ######
    SCAN_TIME = 0.15    # Input records scan time in seconds + 0.01 second
    ######################
    
    pv_loSeq_D_optB = pv('TEST:loSeq_D_optB')
    pv_liSeq_D_optB = pv('TEST:liSeq_D_optB')
    pv_loSeq_D = pv('TEST:loSeq_D')
    pv_liSeq_D = pv('TEST:liSeq_D')
    pv_loSeq_B_optB = pv('TEST:loSeq_B_optB')
    pv_liSeq_B_optB = pv('TEST:liSeq_B_optB')
    pv_loSeq_B = pv('TEST:loSeq_B')
    pv_liSeq_B = pv('TEST:liSeq_B')
    pv_mbboSeq_D = pv('TEST:mbboSeq_D')
    pv_mbbiSeq_D = pv('TEST:mbbiSeq_D')
    pv_mbboSeq_B = pv('TEST:mbboSeq_B')
    pv_mbbiSeq_B = pv('TEST:mbbiSeq_B')
    
    SeqPVs=[pv_loSeq_D_optB, pv_liSeq_D_optB, pv_loSeq_D, pv_liSeq_D, pv_loSeq_B, pv_liSeq_B,\
             pv_loSeq_B_optB, pv_liSeq_B_optB, pv_mbboSeq_D, pv_mbbiSeq_D, pv_mbboSeq_B, pv_mbbiSeq_B]
    
    def setUp(self):
        print('Set up.')
        for i, val in enumerate(self.SeqPVs):
            self.SeqPVs[i].write_silent(0, 0)
        time.sleep(2*self.SCAN_TIME)
        
    def tearDown(self):
        time.sleep(0.01)
        print('\nClean up.')
        for i, val in enumerate(self.SeqPVs):
            self.SeqPVs[i].write_silent(0, 0)
        time.sleep(2*self.SCAN_TIME)
        
# This test case uses longout record with BCD option to write to internal registers 'B' and 'D' of sequence CPU
# and longin record to read that value from register.
    def test_TC_01(self):
        print ('TC-01: reg D, longout BCD, longin, longin BCD')
        print ('TC-01 uses longout with BCD option to write to internal')
        print ('register D and longin record to read that value from')
        print ('the internal register as BCD and decimal, respectively.')
        
        # Put 145 as BCD to longout
        self.pv_loSeq_D_optB.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_liSeq_D_optB.read(), 100, 'Read BCD value from D not correct.')
        self.assertEqual(self.pv_liSeq_D.read(), 256, 'Read value from D does not match - not properly converted to/from BCD.')

    def test_TC_02(self):
        print ('TC-02: reg B, longout BCD, longin, longin BCD')
        print ('TC-02 uses longout with BCD option to write to internal')
        print ('register B and longin record to read that value from')
        print ('the internal register as BCD and decimal, respectively.')
        
        # Put 145 as BCD to longout
        self.pv_loSeq_B_optB.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_liSeq_B_optB.read(), 100, 'Read BCD value from B not correct.')
        self.assertEqual(self.pv_liSeq_B.read(), 256, 'Read value from B does not match - not properly converted to/from BCD.')

    def test_TC_03(self):
        print ('TC-03: reg D, longout, longin, longin BCD')
        print ('TC-03 uses longout with no options to write to internal register')
        print ('D and longin to read that value as BCD and decimal, respectively.')
        
        # Put 145 to longout
        self.pv_loSeq_D.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_liSeq_D.read(), 100, 'Value for D not written or read properly.')
        self.assertEqual(self.pv_liSeq_D_optB.read(), 64, 'BCD value for D not written or read properly.')
        
    def test_TC_04(self):
        print ('TC-04: reg B, longout, longin, longin BCD')
        print ('TC-04 uses longout with no options to write to internal register')
        print ('B and longin to read that value as BCD and decimal, respectively.')
        
        # Put 145 to longout
        self.pv_loSeq_B.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_liSeq_B_optB.read(), 64, 'BCD value for B not written or read properly.')
        self.assertEqual(self.pv_liSeq_B.read(), 100, 'Value for B not written or read properly.')

    def test_TC_05(self):
        print ('TC-05: reg D, invalid values, longout, longin, longin BCD')
        print ('TC-05 checks behaviour of longin nad longout when invalid values')
        print ('are passed to it (i.e. char).')
        
        self.pv_loSeq_D.write("t", self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_liSeq_D.read(), 0, 'Value should not have changed.')
        self.assertEqual(self.pv_liSeq_D_optB.read(), 0, 'BCD value should not have changed.')
        
    def test_TC_06(self):
        print ('TC-06: reg D, invalid values, longout, longin, longin BCD')
        print ('TC-06 checks behaviour of longin nad longout with BCD option')
        print ('when invalid values are passed to it (i.e. strings).')
        
        self.pv_loSeq_D_optB.write("three", self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_liSeq_D_optB.read(), 0, 'BCD value should not have changed.')
        self.assertEqual(self.pv_liSeq_D.read(), 0, 'Value should not have changed.')
        
    def test_TC_07(self):
        print ('TC-07: reg D, out-of-range values, longout, longin')
        print ('TC-07 checks behaviour of longin and longout without option')
        print ('when values out-of-16-bit-range are passed to it.')
        
        self.pv_loSeq_D.write(32767, self.SCAN_TIME)
        
        self.assertEqual(self.pv_liSeq_D.read(), 32767, 'Values do not match.')
        
        self.pv_loSeq_D.write(2147483647, self.SCAN_TIME)
        
        self.assertEqual(self.pv_liSeq_D.read(), 65535, 'Value not max.') # Register D is 16-bit
        
        self.pv_loSeq_D.write(-2147483645, self.SCAN_TIME)
        
        self.assertEqual(self.pv_liSeq_D.read(), 3, 'Value not correct.')
    
    def test_TC_08(self):
        print ('TC-08: reg D, valid/invalid values, mbbo, mbbi')
        print ('TC-08 checks behaviour of mbbi and mbbo for register D')
        print ('for valid and invalid values.')
        
        self.pv_mbboSeq_D.write("8", self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbiSeq_D.read("STRING"), '8', 'Values do not match.')
        
        self.pv_mbboSeq_D.write("42", self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbiSeq_D.read("STRING"), '8', 'Value should not have changed.')
        
    
