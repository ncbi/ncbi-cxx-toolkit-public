#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.20.0 and up
"""

import time
import socket
from netschedule_tests_pack import TestBase
from netschedule_tests_pack_4_10 import execAny
from netschedule_tests_pack_4_10 import getClientInfo
# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs


class Scenario1500( TestBase ):
    " scenario 1500 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit a job without affinity, " \
               "submit a job with an affinity, " \
               "get and commit the job with affinity, read the job with affinity"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()
        jobID1 = self.ns.submitJob( 'TEST', 'bla' )
        jobID1 = jobID1     # pylint is happy
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'aff0' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'aff0' )[ 0 ]
        if jobID2 != receivedJobID:
            raise Exception( "Unexpected job for execution" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1500' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "PUT " + jobID2 + " 0 nooutput" )
        output = execAny( ns_client, "READ2 reader_aff=0 any_aff=0 aff=aff0" )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobID2:
            raise Exception( "Unexpected job for reading" )
        return True


class Scenario1501( TestBase ):
    " scenario 1501 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "submit a job with an affinity, " \
               "get and commit the job with affinity, read the job with another affinity"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()
        jobID = self.ns.submitJob( 'TEST', 'bla', 'aff0' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'aff0' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1501' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )
        output = execAny( ns_client, "READ2 reader_aff=0 any_aff=0 aff=other_affinity" )
        values = parse_qs( output, True, True )
        if 'job_key' in values:
            raise Exception( "Expected no job for reading, received one" )
        return True


class Scenario1502( TestBase ):
    " Scenario 1502 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as anon"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1502' )

        try:
            execAny( ns_client, "CHRAFF add=a1" )
        except Exception, excp:
            if "cannot use CHRAFF command" in str( excp ):
                return True
            raise
        return False


class Scenario1503( TestBase ):
    " Scenario 119 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as identified, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1503' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, "CHRAFF add=a1,a2" )

        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'number_of_reader_preferred_affinities' ] != 2:
            raise Exception( 'Unexpected length of reader_preferred_affinities' )
        if client[ 'type' ] not in [ 'unknown', 'reader', 'reader | admin' ]:
            raise Exception( 'Unexpected client type: ' + client[ 'type' ] )

        return True


class Scenario1504( TestBase ):
    " Scenario 1504 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = ""

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as identified (rm), STAT CLIENTS"

    def report_warning( self, msg, server ):
        self.warning = msg

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1504' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning
        execAny( ns_client, "CHRAFF del=a1,a2" )

        getClientInfo( ns_client, 'node', 1, 1 )
        if "unknown affinity to delete" in self.warning:
            return True
        raise Exception( "The expected warning has not received" )


class Scenario1505( TestBase ):
    " Scenario 1505 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = ""

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as identified (add, rm), STAT CLIENTS"

    def report_warning( self, msg, server ):
        self.warning = msg

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1505' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        execAny( ns_client, "CHRAFF add=a1,a2,a3" )
        execAny( ns_client, "CHRAFF add=a2,a4 del=a1" )
        client = getClientInfo( ns_client, 'node' )
        if "already registered affinity to add" not in self.warning:
            raise Exception( "The expected warning has not received" )

        if len( client[ 'reader_preferred_affinities' ] ) != 3 or \
           'a2' not in client[ 'reader_preferred_affinities' ] or \
           'a3' not in client[ 'reader_preferred_affinities' ] or \
           'a4' not in client[ 'reader_preferred_affinities' ]:
            raise Exception( "Unexpected reader preferred affinities" )
        return True

class Scenario1506( TestBase ):
    " Scenario 1506 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as identified (add), CLRN, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1506' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a1,a2" )

        execAny( ns_client, 'CLRN' )
        client = getClientInfo( ns_client, 'node' )
        if client.has_key( 'number_of_reader_preferred_affinities' ):
            if client[ 'number_of_reader_preferred_affinities' ] != 0:
                raise Exception( "Expected no reader preferred affinities, got some." )
        return True

class Scenario1507( TestBase ):
    " Scenario 1507 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as identified (add), connect with another session, " \
               "STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1507' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a1,a2" )

        self.ns.submitJob( 'TEST', 'bla', '', '', 'node', 'other_session' )

        client = getClientInfo( ns_client, 'node' )
        if client.has_key( 'number_of_reader_preferred_affinities' ):
            if client[ 'number_of_reader_preferred_affinities' ] != 0:
                raise Exception( "Expected no reader preferred affinities, got some." )
        return True

class Scenario1508( TestBase ):
    " Scenario 1508 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 as anon"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            ns_client = self.getNetScheduleService( 'TEST', 'scenario1508' )
            execAny( ns_client, 'READ2 reader_aff=1 any_aff=1' )
        except Exception, exc:
            if "Anonymous client" in str( exc ):
                return True
            raise
        return False

class Scenario1509( TestBase ):
    " Scenario 1509 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, CHRAFF as identified (add a0, a1, a2), " \
               "READ2 reader_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1509' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a0,a1,a2" )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )

        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )
        return True

class Scenario1510( TestBase ):
    " Scenario 1510 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, CHRAFF as identified (add a0, a2), " \
               "READ2 reader_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1510' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a0,a2" )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )

        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0' )
        if output != "no_more_jobs=false":
            raise Exception( "Expect no jobs, but received one." )
        return True


class Scenario1511( TestBase ):
    " Scenario 1511 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as identified (add a0, a1, a2), SUBMIT with a1, " \
               "READ2 reader_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1511' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a0,a1,a2" )

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )
        return True

class Scenario1512( TestBase ):
    " Scenario 1512 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF as identified (add a0, a1, a2), SUBMIT with a5, " \
               "READ2 reader_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1512' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a0,a1,a2" )

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a5' )
        receivedJobID = self.ns.getJob( 'TEST', -1, 'a5' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client, 'READ2 reader_aff=1 any_aff=0' )

        if output != "no_more_jobs=false":
            raise Exception( "Expect no jobs, but received one." )
        return True

class Scenario1513( TestBase ):
    " Scenario 1513 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, CHRAFF add=a2, READ2 reader_aff = 1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1513' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a2" )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )
        return True

class Scenario1514( TestBase ):
    " Scenario 1514 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a0, SUBMIT with a1, SUBMIT with no aff, " \
               "CHRAFF add=a2, READ2 aff=a1 reader_aff = 1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a0' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1514' )
        ns_client.set_client_identification( 'node', 'session' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a0' )[ 0 ]
        if jobID1 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID1 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID2 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID2 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID3 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID3 + " 0 nooutput" )


        execAny( ns_client, "CHRAFF add=a2" )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=1 aff=a1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID2 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID2 + " Received: " + receivedJobID )
        return True

class Scenario1515( TestBase ):
    " Scenario 1515 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHRAFF add=a2, READ2 aff=a5 reader_aff = 1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1515' )
        ns_client.set_client_identification( 'node', 'session' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID1 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID1 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST', -1, 'a2' )[ 0 ]
        if jobID2 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID2 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID3 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID3 + " 0 nooutput" )

        execAny( ns_client, "CHRAFF add=a2" )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=1 aff=a5' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID2 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID2 + " Received: " + receivedJobID )
        return True

class Scenario1516( TestBase ):
    " Scenario 1516 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHRAFF add=a2, READ2 aff=a5 reader_aff=0 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1516' )
        ns_client.set_client_identification( 'node', 'session' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID1 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID1 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST', -1, 'a2' )[ 0 ]
        if jobID2 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID2 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID3 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID3 + " 0 nooutput" )

        execAny( ns_client, "CHRAFF add=a2" )
        output = execAny( ns_client,
                          'READ2 reader_aff=0 any_aff=1 aff=a5' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID1 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID1 + " Received: " + receivedJobID )
        return True

class Scenario1517( TestBase ):
    " Scenario 1517 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHRAFF add=a7, READ2 aff=a5 reader_aff=1 any_aff=0"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1517' )
        ns_client.set_client_identification( 'node', 'session' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID1 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID1 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST', -1, 'a2' )[ 0 ]
        if jobID2 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID2 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID3 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID3 + " 0 nooutput" )

        execAny( ns_client, "CHRAFF add=a7" )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 aff=a5' )
        if output != "no_more_jobs=false":
            raise Exception( "Expect no jobs, but received one." )
        return True

class Scenario1518( TestBase ):
    " Scenario 11518 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHRAFF add=a7, READ2 aff=a5 reader_aff=1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario136' )
        ns_client.set_client_identification( 'node', 'session' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID1 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID1 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST', -1, 'a2' )[ 0 ]
        if jobID2 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID2 + " 0 nooutput" )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID3 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID3 + " 0 nooutput" )

        execAny( ns_client, "CHRAFF add=a7" )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=1 aff=a5' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID1 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID1 + " Received: " + receivedJobID )
        return True




