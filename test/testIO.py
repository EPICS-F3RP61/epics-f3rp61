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
    # TEST:bi_E                  # TEST:bi_X
    # TEST:bi_Y                  # TEST:bo_Y
    # TEST:bo_E                  
    
    # TEST:li_A                  # TEST:li_A_optB
    # TEST:li_R                  # TEST:li_R_optB
    # TEST:li_W                  # TEST:li_W_optB
    # TEST:li_r                  # TEST:li_r_optB
    # TEST:lo_A                  # TEST:lo_A_optB
    # TEST:lo_R                  # TEST:lo_R_optB
    # TEST:lo_W                  # TEST:lo_W_optB
    # TEST:lo_r                  # TEST:lo_r_optB
    
    # TEST:mbbi_A
    # TEST:mbbi_E                # TEST:mbbi_L
    # TEST:mbbi_M                # TEST:mbbi_R
    # TEST:mbbi_W                # TEST:mbbi_X
    # TEST:mbbi_Y                # TEST:mbbi_r
    # TEST:mbbo_A                # TEST:mbbo_E
    # TEST:mbbo_L                # TEST:mbbo_M
    # TEST:mbbo_R                # TEST:mbbo_W
    # TEST:mbbo_Y
    # TEST:mbbo_r
    
    # TEST:li_A_optU            # TEST:lo_A_optL
    # TEST:li_R_optU            # TEST:lo_R_optL
    # TEST:li_W_optU            # TEST:lo_W_optL
    # TEST:li_r_optU            # TEST:lo_r_optL
    # TEST:li_A_optL
    # TEST:li_R_optL
    # TEST:li_W_optL
    # TEST:li_r_optL


#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
# Test class for IO driver ------------------------------------------------
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
class IOTest(unittest.TestCase):
    
    ### DEFINITIONS ######
    SCAN_TIME = 0.15    # Input records scan time in seconds + some time to be on the safe side
    ######################
    
    pv_li_R = pv('TEST:li_R')
    pv_li_R_optB = pv('TEST:li_R_optB')
    pv_li_R_optL = pv('TEST:li_R_optL')
    pv_li_R_optU = pv('TEST:li_R_optU')
    pv_lo_R = pv('TEST:lo_R')
    pv_lo_R_optB = pv('TEST:lo_R_optB')
    pv_lo_R_optL = pv('TEST:lo_R_optL')
    pv_li_r = pv('TEST:li_r')
    pv_li_r_optB = pv('TEST:li_r_optB')
    pv_lo_r = pv('TEST:lo_r')
    pv_lo_r_optB = pv('TEST:lo_r_optB')
     
    pv_mbbi_E = pv('TEST:mbbi_E')
    pv_mbbo_E = pv('TEST:mbbo_E')
    pv_mbbi_M = pv('TEST:mbbi_M')
    pv_mbbo_M = pv('TEST:mbbo_M')
    pv_mbbi_R = pv('TEST:mbbi_R')
    pv_mbbo_R = pv('TEST:mbbo_R')
    pv_mbbi_X = pv('TEST:mbbi_X')
    pv_mbbi_Y = pv('TEST:mbbi_Y')
    pv_mbbo_Y = pv('TEST:mbbo_Y')
    
    pv_bi_E = pv('TEST:bi_E')
    pv_bo_E = pv('TEST:bo_E')
    pv_bi_Y = pv('TEST:bi_Y')
    pv_bo_Y = pv('TEST:bo_Y')
    pv_bi_X = pv('TEST:bi_X')
    
    IOPVs=[pv_li_R, pv_li_R_optL, pv_li_R_optU, pv_lo_R, pv_lo_R_optL, pv_mbbi_E, pv_mbbo_E, \
           pv_mbbi_R, pv_mbbo_R, pv_mbbi_X, pv_mbbi_Y, pv_mbbo_Y, pv_bi_E, pv_bo_E, \
           pv_bi_Y, pv_bo_Y, pv_bi_X, pv_li_R_optB, pv_lo_R_optB, pv_li_r, pv_lo_r,pv_li_r_optB,\
           pv_mbbi_M, pv_mbbo_M, pv_lo_r_optB \
           ]
    
    def setUp(self):
        print('Set up.')
        for i, val in enumerate(self.IOPVs):
            self.IOPVs[i].write_silent(0, 0)
        time.sleep(2*self.SCAN_TIME)
        
    
    def tearDown(self):
        time.sleep(0.01)
        print('\nClean up.')
        
        for i, val in enumerate(self.IOPVs):
            self.IOPVs[i].write_silent(0, 0)
        time.sleep(2*self.SCAN_TIME)
        
        # Set Y outputs to 0:
        # Condition for ladder program - needs to be 1 to change value of Y
        self.pv_bo_E.write_silent(1, 0)
        self.pv_mbbo_E.write_silent(0, 0.2)
        self.pv_bo_E.write_silent(0, 0)
            

    def test_TC_01(self):
        print ('TC-01: reg. R, longout, longin, longin L, longin U')
        print ('TC-01 writes longout without options')
        print ('and reads with longin using options L and U')
        
        new_value = 42
        self.pv_lo_R.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_R.read(), new_value, 'Positive value does not match.')
        
        new_value = -42
        self.pv_lo_R.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_R.read(), -42, 'Value does not match.')
        self.assertEqual(self.pv_li_R_optL.read(), 65494, 'Value for L does not match.')
        self.assertEqual(self.pv_li_R_optU.read(), 65494, 'Value for U does not match.')
        
        
    def test_TC_02(self):
        print ('TC-02: reg. R, longout L, longin, longin L, longin U')
        print ('TC-02 checks behaviour of writing longout with option L')
        print ('and reading with longin using each options L and U')
        
        new_value = 2147483647
        self.pv_lo_R_optL.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_R_optL.read(), new_value, 'Values for L not max.')
        
        new_value = -2147483645
        self.pv_lo_R_optL.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_R_optL.read(), new_value, 'Values for L not min.')
        self.assertEqual(self.pv_li_R_optU.read(), 3, 'Value for U does not match.')
        self.assertEqual(self.pv_li_R.read(), 3, 'Value does not match.')
        
    def test_TC_03(self):
        print ('TC-03: reg. E, valid/invalid value, mbbo, mbbi')
        print ('TC-03 tests mbbo and mbbi for E shared relays writing and reading')
        print ('for valid enum value and for invalid value.')
                
        new_value = "8"
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbi_E.read("STRING"), new_value, 'Values do not match.')
        
        new_value = "25"
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbi_E.read("STRING"), "8", 'Value should not have changed.')
        
    def test_TC_04(self):
        print ('TC-04: reg. R, valid/invalid, mbbo, mbbi')
        print ('TC-04 tests mbbo and mbbi for R shared registers writing and reading')
        print ('for valid enum value and for invalid value.')
                
        new_value = "8"
        self.pv_mbbo_R.write(new_value, self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbi_R.read("STRING"), new_value, 'Values do not match.')
        
        new_value = "25"
        self.pv_mbbo_R.write(new_value, self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbi_R.read("STRING"), "8", 'Value should not have changed.')
        
    def test_TC_05(self):
        print ('TC-05: reg. M, valid/invalid value, mbbo, mbbi')
        print ('TC-05 tests mbbo and mbbi for M special relays writing and reading')
        print ('for valid enum value and for invalid value.')
                
        new_value = "5"
        self.pv_mbbo_M.write(new_value, self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbi_M.read("STRING"), new_value, 'Values do not match.')
        
        new_value = "25"
        self.pv_mbbo_M.write(new_value, self.SCAN_TIME, "STRING")
        
        self.assertEqual(self.pv_mbbi_M.read("STRING"), "5", 'Value should not have changed.')
        
        # trying this because M seems like 3-bit register (?)
        #new_value = "9"
        #self.pv_mbbo_M.write(new_value, self.SCAN_TIME, "STRING")
        
        #self.assertEqual(self.pv_mbbi_M.read("STRING"), new_value, 'Values do not match.')
    
    def test_TC_06(self):
        print ('TC-06: E, bo, bi')
        print ('TC-06 tests bo and bi for E shared relay')
        
        new_value = 1
        self.pv_bo_E.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_bi_E.read(), new_value, 'Read value for E does not match.')
    
    def test_TC_07(self):
        print ('TC-07: mbbo Y, mbbi Y, mbbi X')
        print ('TC-07 tests mbbo and mbbi for Y outputs and X inputs')
        print ('mbbo and mbbi for Y are tested and mbbi for X.')
                
        new_value = 4
        self.pv_mbbo_Y.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_mbbi_Y.read(), new_value, 'Read value for Y does not match.')
        self.assertEqual(self.pv_mbbi_X.read("STRING"), "Xinput_4_65284", 'Read value for X does not match.')
        
    def test_TC_08(self):
        print ('TC-08: bo Y, bi Y, bi X')
        print ('TC-08 tests bo and bi for Y outputs and X inputs')
        print ('bo and bi for Y are tested and bi for X.')
                
        new_value = 4
        self.pv_bo_Y.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_bi_Y.read(), new_value, 'Read value for Y does not match.')
        self.assertEqual(self.pv_bi_X.read(), new_value, 'Read value for X does not match.')
        
    def test_TC_09(self):
        print ('TC-09: Y, mbbi')
        print ('TC-09 tests mbbi input for Y')
        print ('ONLY tests INPUT!')
        print ('It uses ladder program to set outputs on Y.')
                
        # Writes to E00257 to set condotion in ladder program to 1 and
        # then writes to register E00265 whose value is then copied by ladder program to Y
        self.pv_bo_E.write(1, self.SCAN_TIME)
        new_value = 4
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_mbbi_Y.read(), new_value, 'Read value for Y does not match.')
    
    def test_TC_10(self):
        print ('TC-10: X, mbbi')
        print ('TC-10 tests mbbi input for X')
        print ('ONLY tests INPUT!')
        print ('It uses ladder program to set outputs on Y which have loopback to X.')
                
        # Writes to E00257 to set condotion in ladder program to 1 and
        # then writes to register E00265 whose value is then copied by ladder program to Y
        self.pv_bo_E.write(1, self.SCAN_TIME)
        new_value = 4
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME)
        
        #self.assertEqual(self.pv_mbbi_X.read("STRING"), "Xinput_4_65284", 'Read value for X does not match.')
        # Input modules with 8 inputs have register bits X9-15 set to 1 by default, which gives big numbers
        self.assertTrue((self.pv_mbbi_X.read("STRING")=="Xinput_4_65284" or self.pv_mbbi_X.read("STRING")=="4"), 'Read value for X does not match.')
       
    def test_TC_11(self):
        print ('TC-11: X, invalid enum, mbbi')
        print ('TC-11 tests mbbi input for X for Invalid Value (not recognized by enum)')
        print ('ONLY tests INPUT!')
        print ('It uses ladder program to set outputs on Y which have loopback to X.')
                
        # Writes to E00257 to set condotion in ladder program to 1 and
        # then writes to register E00265 whose value is then copied by ladder program to Y
        self.pv_bo_E.write(1, self.SCAN_TIME)
        new_value = 40
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME)
        
        #self.assertEqual(self.pv_mbbi_X.read(), 65535, 'Read value for X was not read properly (should be Invalid Value).')
        # Input modules with 8 inputs have register bits X9-15 set to 1 by default, which gives big numbers
        self.assertTrue((self.pv_mbbi_X.read()==65535 or self.pv_mbbi_X.read()==0), 'Read value for X was not read properly (should be Invalid Value).')
    
    def test_TC_12(self):
        print ('TC-12: Y, bi')
        print ('TC-12 tests bi input for Y')
        print ('ONLY tests INPUT!')
        print ('It uses ladder program to set outputs on Y.')
                
        # Writes to E00257 to set condotion in ladder program to 1 and
        # then writes to register E00265 whose value is then copied by ladder program to Y
        self.pv_bo_E.write(1, self.SCAN_TIME)
        new_value = 5
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_bi_Y.read(), 1, 'Read value for X bi is not 1.')
        
        new_value = 4
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_bi_Y.read(), 0, 'Read value for X bi is not 0.')
        
    def test_TC_13(self):
        print ('TC-13: X, bi')
        print ('TC-13 tests bi input for X')
        print ('ONLY tests INPUT!')
        print ('It uses ladder program to set outputs on Y which have loopback to X.')
                
        # Writes to E00257 to set condotion in ladder program to 1 and
        # then writes to register E00265 whose value is then copied by ladder program to Y
        self.pv_bo_E.write(1, self.SCAN_TIME)
        new_value = 5
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_bi_X.read(), 1, 'Read value for X bi is not 1.')
        
        new_value = 4
        self.pv_mbbo_E.write(new_value, self.SCAN_TIME)
        
        self.assertEqual(self.pv_bi_X.read(), 0, 'Read value for X bi is not 0.')
        
    def test_TC_14(self):
        print ('TC-14: R, longout BCD, longin BCD, longin')
        print ('TC-14 uses longout with BCD option to write to register R')
        print ('and longin record to read that value from')
        print ('the internal register as BCD and decimal, respectively.')
        
        # Put 145 as BCD to longout
        self.pv_lo_R_optB.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_R_optB.read(), 100, 'Read BCD value from R not correct.')
        self.assertEqual(self.pv_li_R.read(), 256, 'Read value from R does not match - not properly converted to/from BCD.')
    
    def test_TC_15(self):
        print ('TC-15: R, longout, longin, longin BCD')
        print ('TC-15 uses longout with no options to write to register R')
        print ('and longin to read that value as BCD and decimal, respectively.')
        
        # Put 145 to longout
        self.pv_lo_R.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_R.read(), 100, 'Value for R not written or read properly.')
        self.assertEqual(self.pv_li_R_optB.read(), 64, 'BCD value for R not written or read properly.')
        
    def test_TC_16(self):
        print ('TC-16: \'old interface\', longout BCD, longin BCD, longin')
        print ('TC-16 uses longout with BCD option to write to register')
        print ('using \'old interface\' and longin record to read that value from')
        print ('the register as BCD and decimal, respectively.')
        
        # Put 145 as BCD to longout
        self.pv_lo_r_optB.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_r_optB.read(), 100, 'Read BCD value from R not correct.')
        self.assertEqual(self.pv_li_r.read(), 256, 'Read value from R does not match - not properly converted to/from BCD.')
    
    def test_TC_17(self):
        print ('TC-17: \'old interface\', longout, longin, longin BCD')
        print ('TC-17 uses longout with no options to write to register')
        print ('using \'old interface\' and longin to read that value')
        print ('as BCD and decimal, respectively.')
        
        # Put 145 to longout
        self.pv_lo_r.write(100, self.SCAN_TIME)
        
        self.assertEqual(self.pv_li_r.read(), 100, 'Value for R not written or read properly.')
        self.assertEqual(self.pv_li_r_optB.read(), 64, 'BCD value for R not written or read properly.')

        
