################################################################
# Copyright (c) 2013 High Energy Accelerator Research Organization (KEK)
#
# f3rp61-test 1.3.0
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
################################################################

# EPICS F3RP61 Device Support test
# For the communication with the database we use the Cachannel module, testing is done with pyUnit framework


from CaChannel import CaChannel
from CaChannel import ca
import time

#------------------------------------------------------------------------------
# Wrapper for put and get functions that provides some logging information
# using print statements when PV values are changed. 

class pv():
    # Wrapper for associating CaChannel class to specific PV using its name 
    def __init__(self, pvname):
        try:
            self.pv = CaChannel()
            self.pv.searchw(pvname)
            self.alarm_status = "n/a"	# used to store severity provided by callback
            self.alarm_severity = "n/a"	# used to store severity provided by callback
        except :
            raise Exception("****** PV not found - "+pvname+" ******")
        #return pv
  
    # Wrapper for putw() function - writing a value to PV and waiting for input records to scan (update their value)
    def write(self, *args):
        try:
            if len(args) == 2:  # arguments: value, scan_time
                self.pv.putw(args[0])
                time.sleep(args[1])
            if len(args) == 3:  # arguments: value, scan_time, type
                if args[2] == "STRING":
                    self.pv.putw(args[0],ca.DBR_STRING)
                else:
                    raise Exception("Type "+ str(args[3]) +" not recognized")
                time.sleep(args[1])
            print("***W: Set  PV "+self.pv.name()+" to    "+str(args[0]))
        except:
            print("Could not set PV "+self.pv.name()+" to "+str(args[0]))
    
    # Similar to write() except it does not print output when successfully sets the value
    # Useful for setUp and tearDown methods
    def write_silent(self, *args):
        try:
            if len(args) == 2:  # arguments: value, scan_time
                self.pv.putw(args[0])
                time.sleep(args[1])
            if len(args) == 3:  # arguments: value, scan_time, type
                if args[2] == "STRING":
                    self.pv.putw(args[0],ca.DBR_STRING)
                else:
                    raise Exception("Type "+ str(args[3]) +" not recognized")
                time.sleep(args[1])
        except:
            print("Could not set PV "+self.pv.name()+" to "+str(args[0]))
    
    # Wrapper for getw()
    def read(self, *args):
        if len(args) == 1:
            if args[0] == "STRING":
                val = self.pv.getw(ca.DBR_STRING)
            else:
                raise Exception("Type "+ str(args[3]) +" not recognized")
            print("***R: Read PV "+self.pv.name()+" value "+str(val))
            return val
        else:
            val = self.pv.getw()
            print("***R: Read PV "+self.pv.name()+" value "+str(val))
            return val
            
    # Wrapper for getw() with CALLBACK for alarm status/severity
    def read_alarm(self):
        self.pv.array_get_callback(ca.dbf_type_to_DBR_STS(self.pv.field_type()), None, Callback, self)
        self.pv.flush_io()
        for i in range(20):
            self.pv.pend_event()
    
    # Wrapper for name()
    def name(self):
        return self.pv.name()
        
#------------------------------------------------------------------------------
# CALLBACKS - needed when we want to read alarm status and severity of the record

def Callback(epics_args, user_args):
    user_args[0].alarm_severity = ca.alarmSeverityString(epics_args['pv_severity'])
    user_args[0].alarm_status = ca.alarmStatusString(epics_args['pv_status'])
    print ('***R: Read PV '+user_args[0].name()+' alarm severity ' + ca.alarmSeverityString(epics_args['pv_severity']))
    print ('***R: Read PV '+user_args[0].name()+' alarm status ' + ca.alarmStatusString(epics_args['pv_status']))

#------------------------------------------------------------------------------

# USED IN MANUAL TEST FOR ECHOING STDOUT TO FILE

# manual print method - prints to report file and echoes to stdout
class log(object):
    def __init__(self, filename):
        self.passed = 0
        self.failed = 0
        self.fd = open("./generated-reports/"+filename, "w")
    def close(self):
        self.fd.close()
    def __call__(self, string):
        print string
        self.fd.write(string+"\n")
    def assert_(self, a, b):
        if a == b:
            self.passed += 1
            self.__call__("+++++ PASS\n")
        else:
            self.failed += 1
            self.__call__("----- FAIL\n")
    def report(self):
        self.__call__("----------------------------")
        self.__call__("TOTAL: Passed: " + str(self.passed) + "  Failed: "+ str(self.failed))
        self.__call__("----------------------------")


            
            
