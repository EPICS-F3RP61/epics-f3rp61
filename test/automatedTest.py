################################################################
# Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
#
# f3rp61-test 1.3.0
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
################################################################


import unittest

#import xmlrunner
from modules import HTMLTestRunner

from testSeq import SeqTest
from testSysCtl import SysCtlTest
from testIO import IOTest
#from RunIOC import runIOC

# f3rp61 IP address

################################################################
#RUN SPECIFIC TEST CASE 
################################################################
#runner = unittest.TextTestRunner()
#runner.run(SysCtlTest("test_TC_02"))


################################################################
#RUN ALL TESTS
################################################################
#if __name__ == '__main__':
    #unittest.main() 
    #unittest.main(testRunner=xmlrunner.XMLTestRunner(output='test-reports'))


################################################################        
# RUN A GROUP OF CHOSEN TESTS
################################################################
def SysCtlsuite():
    suite = unittest.TestSuite()
    suite.addTest(SysCtlTest("test_TC_01"))
    suite.addTest(SysCtlTest("test_TC_02"))
    suite.addTest(SysCtlTest("test_TC_03"))
    suite.addTest(SysCtlTest("test_TC_04"))
    suite.addTest(SysCtlTest("test_TC_05"))
    return suite

def Seqsuite():
    suite = unittest.TestSuite()
    suite.addTest(SeqTest("test_TC_01"))
    suite.addTest(SeqTest("test_TC_02"))
    suite.addTest(SeqTest("test_TC_03"))
    suite.addTest(SeqTest("test_TC_04"))
    suite.addTest(SeqTest("test_TC_05"))
    suite.addTest(SeqTest("test_TC_06"))
    suite.addTest(SeqTest("test_TC_07"))
    suite.addTest(SeqTest("test_TC_08"))
    return suite

def IOsuite():
    suite = unittest.TestSuite()
    suite.addTest(IOTest("test_TC_01"))
    suite.addTest(IOTest("test_TC_02"))
    suite.addTest(IOTest("test_TC_03"))
    suite.addTest(IOTest("test_TC_04"))
    suite.addTest(IOTest("test_TC_05"))
    suite.addTest(IOTest("test_TC_06"))
    suite.addTest(IOTest("test_TC_07"))
    suite.addTest(IOTest("test_TC_08"))
    #suite.addTest(IOTest("test_TC_09"))
    #suite.addTest(IOTest("test_TC_10"))
    #suite.addTest(IOTest("test_TC_11"))
    #suite.addTest(IOTest("test_TC_12"))
    #suite.addTest(IOTest("test_TC_13"))
    suite.addTest(IOTest("test_TC_14"))
    suite.addTest(IOTest("test_TC_15"))
    suite.addTest(IOTest("test_TC_16"))
    suite.addTest(IOTest("test_TC_17"))
    suite.addTest(IOTest("test_TC_18"))
    suite.addTest(IOTest("test_TC_19"))
    return suite

def runTest():
        
    automatedTests = unittest.TestSuite((SysCtlsuite(), Seqsuite(), IOsuite()))
    
    # ONLY RUN THE TEST - NO REPORT LOG
    #runner = unittest.TextTestRunner(verbosity=4)
    #runner.run(automatedTests)
    
    # RUN THE TEST AND GENERATE TEXT REPORT
    #logFile = open("./reports/automatedTest_Report.txt", "w")
    #runner = unittest.TextTestRunner(stream=logFile, verbosity=2)
    #runner.run(automatedTests)
    #logFile.close()
    
    # USE XMLRunner FOR GENERATING XML REPORT
    #runner = xmlrunner.XMLTestRunner(output='test-reports')
    #runner.run(automatedTests)
    
    # USE HTMLTestRunner FOR FANCY HTML REPORT
    logFile = open("./generated-reports/automatedTest_Report.html", "w")
    runner = HTMLTestRunner.HTMLTestRunner(stream=logFile, title="F3RP61 EPICS Device Support Automated Test Report", verbosity=2)
    runner.run(automatedTests)
    logFile.close()

#runTest()
if __name__ == "__main__":
    runTest()
