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
from netschedule_tests_pack_4_10 import getAffinityInfo
from netschedule_tests_pack_4_10 import getNotificationInfo
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
        if output != "no_more_jobs=true":
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

        if output != "no_more_jobs=true":
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
        if output != "no_more_jobs=true":
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

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1518' )
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
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID1 + " Received: " + receivedJobID )
        return True

class Scenario1519( TestBase ):
    " Scenario 1519 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, restart netschedule, CHRAFF add=a1, READ2 reader_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        self.ns.shutdown()
        self.ns.resetPID()
        time.sleep( 15 )
        if self.ns.isRunning():
            raise Exception( "Cannot shutdown netschedule" )

        self.ns.start()
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1519' )
        ns_client.set_client_identification( 'node', 'session' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID1 != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID1 + " 0 nooutput" )

        execAny( ns_client, "CHRAFF add=a1" )
        output = execAny( ns_client, 'READ2 reader_aff=1 any_aff=0' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]

        if jobID1 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID1 + " Received: " + receivedJobID )
        return True


class Scenario1520( TestBase ):
    " Scenario 1520 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 with timeout and port and affinity a1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1520' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=0 any_aff=0 exclusive_new_aff=0 aff=a1 port=' + str( notifPort ) + ' timeout=3' )

        # Submit a job
        jobID = self.ns.submitJob( 'TEST', 'input' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        if result != 0:
            notifSocket.close()
            raise Exception( "Expect no notifications but received some" )
        notifSocket.close()
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0

class Scenario1521( TestBase ):
    " Scenario 1521 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 with timeout and port, and explicit aff a1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1521' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=0 any_aff=0 exclusive_new_aff=0 aff=a1 port=' + str( notifPort ) + ' timeout=5' )

        # Submit a job
        jobID = self.ns.submitJob( 'TEST', 'input', 'a1' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result == 0:
            raise Exception( "Expected notification(s), received nothing" )
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0

class Scenario1522( TestBase ):
    " Scenario 1522 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 with timeout and port, and explicit aff a1, and any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1522' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=0 any_aff=1 exclusive_new_aff=0 aff=a1 port=' + str( notifPort ) + ' timeout=3' )

        # Submit a job
        jobID = self.ns.submitJob( 'TEST', 'input', 'a2' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result == 0:
            raise Exception( "Expected notification(s), received nothing" )
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0


class Scenario1523( TestBase ):
    " Scenario 1523 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF add=a3; READ2 with timeout and port, wnode_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1523' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a3" )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1 port=' + str( notifPort ) + ' timeout=3' )

        # Submit a job
        jobID = self.ns.submitJob( 'TEST', 'input', 'a3' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result == 0:
            raise Exception( "Expected one notification, received nothing" )
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0


class Scenario1524( TestBase ):
    " Scenario 1524 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF add=a3; READ2 with timeout and port, wnode_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1524' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a3" )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=0 port=' + str( notifPort ) + ' timeout=3' )

        # Submit a job
        jobID = self.ns.submitJob( 'TEST', 'input', 'a4' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result != 0:
            raise Exception( "Received notifications when not expected" )
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0

class Scenario1525( TestBase ):
    " Scenario 1525 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CWREAD as anonymous"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1525' )
        try:
            execAny( ns_client, 'CWREAD' )
        except Exception, exc:
            if "no client_node and client_session at handshake" in str( exc ):
                return True
            raise
        raise Exception( "Expected auth exception, got nothing" )

class Scenario1526( TestBase ):
    " Scenario 1526 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CWREAD as identified"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1526' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'CWREAD' )
        return True

class Scenario1527( TestBase ):
    " Scenario 1527 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2, CWREAD, SUBMIT -> no notifications"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1527' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=0 any_aff=1 exclusive_new_aff=0 port=' + str( notifPort ) + ' timeout=3' )
        execAny( ns_client, 'CWREAD' )

        # Submit a job
        jobID = self.ns.submitJob( 'TEST', 'input', 'a4' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result != 0:
            raise Exception( "Received notifications when not expected" )
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0

class Scenario1528( TestBase ):
    " Scenario 1528 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF add=a2, STAT AFFINITIES VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario241' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, "CHRAFF add=a2" )

        aff = getAffinityInfo( ns_client, True, 1, 0 )
        if aff[ 'affinity_token' ] != 'a2':
            raise Exception( "Expected aff token: a2, received: " +
                             aff[ 'affinity_token' ] )
        if aff[ 'clients__explicit_wget' ] is not None:
            raise Exception( "Expected 0 WGET clients, received: " +
                             str( len( aff[ 'clients__explicit_wget' ] ) ) )
        if aff[ 'clients__explicit_read' ] is not None:
            raise Exception( "Expected 0 wait read clients, received: " +
                             str( len( aff[ 'clients__explicit_read' ] ) ) )
        if aff[ 'wn_clients__preferred' ] is not None:
            raise Exception( "Expected 0 preferred clients, received: " +
                             str( len( aff[ 'wn_clients__preferred' ] ) ) )
        if len( aff[ 'reader_clients__preferred' ] ) != 1:
            raise Exception( "Expected 1 preferred read client, received: " +
                             str( len( aff[ 'reader_clients__preferred' ] ) ) )
        if aff[ 'reader_clients__preferred' ][ 0 ] != 'mynode':
            raise Exception( "Unexpected reader preferred client. "
                             "Expected 'mynode', received: '" +
                             aff[ 'reader_clients__preferred' ][ 0 ] + "'" )
        return True

class Scenario1529( TestBase ):
    " Scenario 1529 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF del=a3, STAT AFFINITIES VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario242' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        ns_client.on_warning = None

        execAny( ns_client, "CHRAFF del=a3" )

        getAffinityInfo( ns_client, True, 0, 0 )
        return True

class Scenario1530( TestBase ):
    " Scenario 1530 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHRAFF add=a4, CHRAFF del=a4, STAT AFFINITIES VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1530' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, "CHRAFF add=a4" )
        execAny( ns_client, "CHRAFF del=a4" )

        aff = getAffinityInfo( ns_client, True, 1, 0 )
        if aff[ 'jobs' ] is not None:
            raise Exception( "Expected no jobs, received: " +
                             str( len( aff[ 'jobs' ] ) ) )

        if aff[ 'affinity_token' ] != 'a4':
            raise Exception( "Expected aff token: a4, received: " +
                             aff[ 'affinity_token' ] )
        if aff[ 'clients__explicit_wget' ] is not None:
            raise Exception( "Expected 0 WGET clients, received: " +
                             str( len( aff[ 'clients__explicit_wget' ] ) ) )
        if aff[ 'clients__explicit_read' ] is not None:
            raise Exception( "Expected 0 wait read clients, received: " +
                             str( len( aff[ 'clients__explicit_read' ] ) ) )
        if aff[ 'wn_clients__preferred' ] is not None:
            raise Exception( "Expected 0 preferred clients, received: " +
                             str( len( aff[ 'wn_clients__preferred' ] ) ) )
        if aff[ 'reader_clients__preferred' ] is not None:
            raise Exception( "Expected 0 read preferred clients, received: " +
                             str( len( aff[ 'reader_clients__preferred' ] ) ) )
        return True

class Scenario1531( TestBase ):
    " Scenario 1531 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 as anonymous"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        ns_client = self.getNetScheduleService( 'TEST', 'scenario1531' )
        try:
            execAny( ns_client,
                     'READ2 reader_aff=0 any_aff=1' )
        except Exception, exc:
            if "Anonymous client" in str( exc ):
                return True
            raise
        return False

class Scenario1532( TestBase ):
    " Scenario 1532 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2, STAT NOTIFICATIONS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1532' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client,
                 'READ2 reader_aff=0 any_aff=1 exclusive_new_aff=0 port=9007 timeout=5' )
        time.sleep( 2 )
        # STAT NOTIFICATIONS
        ns_client = self.getNetScheduleService( 'TEST', 'scenario1532' )
        info = getNotificationInfo( ns_client, True, 1, 0 )

        if info[ "use_preferred_affinities" ] != False:
            raise Exception( "Unexpected use_preferred_affinities" )
        if info[ "any_job" ] != True:
            raise Exception( "Unexpected any_job" )
        if info[ "slow_rate_active" ] != False:
            raise Exception( "Unexpected slow_rate_active" )
        if info[ 'active' ] != False:
            raise Exception( "Unexpected active" )
        if info[ 'reason' ] != 'READ':
            raise Exception( "Unexpected reason" )

        return True

class Scenario1533( TestBase ):
    " Scenario 1533 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 any_aff = 1 exclusive_new_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1533' )
        ns_client.set_client_identification( 'node', 'session' )
        try:
            execAny( ns_client,
                     'READ2 reader_aff=1 any_aff=1 exclusive_new_aff=1' )
        except Exception, exc:
            if "forbidden" in str( exc ):
                return True
            raise
        raise Exception( "Expected exception, got nothing" )

class Scenario1534( TestBase ):
    " Scenario 1534 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT without aff, CHRAFF as identified (add a0, a1, a2), " \
               "READ2 reader_aff = 1 exclusive_new_aff=1"

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1534' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning
        execAny( ns_client, "CHRAFF add=a0,a1,a2 del=a3,a4,a5" )

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RDRB ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "no_more_jobs=false":
            raise Exception( "Expect no job (it's in the blacklist), "
                             "but received one: " + output )
        return True

class Scenario1535( TestBase ):
    " Scenario 1535 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a7, CHRAFF as identified (add a0, a1, a2), " \
               "READ2 reader_aff = 1 exclusive_new_aff=1"

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a7' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1535' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning
        execAny( ns_client, "CHRAFF add=a0,a1,a2 del=a3,a4,a5" )

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RDRB ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "no_more_jobs=false":
            raise Exception( "Expect no job (it's in the blacklist), "
                             "but received one: " + output )
        return True

class Scenario1536( TestBase ):
    " Scenario 1536 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a7, CHRAFF as identified (add a0, a1, a2), " \
               "READ2 reader_aff = 1 exclusive_new_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a7' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1536' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a0,a1,a2" )

        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario1536' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        execAny( ns_client2, "CHRAFF add=a7" )

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "no_more_jobs=true":
            raise Exception( "Expect no job (non-unique affinity), "
                             "but received one: " + output )

        execAny( ns_client2, "CHRAFF del=a7" )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RDRB ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "no_more_jobs=false":
            raise Exception( "Expect no job (it's in the blacklist), "
                             "but received one: " + output )
        return True

class Scenario1537( TestBase ):
    " Scenario 1537 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Read notifications and blacklists"

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1537' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a0" )

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RDRB ' + jobID + ' ' + passport )
        # Here: pending job and it is in the client black list

        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario1537' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        output = execAny( ns_client2,
                          'READ2 reader_aff=1 any_aff=1 exclusive_new_aff=0' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        # The first client waits for a job
        execAny( ns_client, "READ2 reader_aff=0 any_aff=0 exclusive_new_aff=0 "
                            "aff=a0 port=" + str( notifPort ) + " timeout=3" )

        # Sometimes it takes so long to spawn grid_cli that the next
        # command is sent before GET2 is sent. So, we have a sleep here
        time.sleep( 2 )

        # Return the job
        execAny( ns_client2, 'RDRB ' + jobID + ' ' + passport )

        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result != 0:
            raise Exception( "Received notifications when not expected" )
        return True

class Scenario1538( TestBase ):
    " Scenario 1538 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications and exclusive affinities"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )

        # First client holds a0 affinity
        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario1538' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        execAny( ns_client1, "CHRAFF add=a0" )

        # Second client holds a100 affinity
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario1538' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        execAny( ns_client2, "CHRAFF add=a100" )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client1, "PUT " + jobID + " 0 nooutput" )

        # Second client tries to get the - should get nothing
        output = execAny( ns_client2,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=' + str( notifPort ) + ' timeout=3' )
        if output != "no_more_jobs=true":
            notifSocket.close()
            raise Exception( "Expect no jobs, received: " + output )

        time.sleep( 4 )
        try:
            # Exception is expected
            data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
            notifSocket.close()
            raise Exception( "Expected no notifications, received one: " +
                             data )
        except Exception, exc:
            if "Resource temporarily unavailable" not in str( exc ):
                notifSocket.close()
                raise

        # Second client tries to get another pending job
        output = execAny( ns_client2,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=' + str( notifPort ) + ' timeout=3' )

        # Should get notifications after this submit
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a5' )    # analysis:ignore
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client1, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 4 )
        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
        notifSocket.close()

        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " +
                             data )
        return True

class Scenario1539( TestBase ):
    " Scenario 1539 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Read notifications and exclusive affinities"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )

        # First client holds a0 affinity
        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario1539' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        execAny( ns_client1, "CHRAFF add=a0" )

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client1, "PUT " + jobID + " 0 nooutput" )

        output = execAny( ns_client1,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]      # analysis:ignore

        if jobID != receivedJobID:
            raise Exception( "Unexpected received job ID" )


        # Second client holds a100 affinity
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario1539' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        execAny( ns_client2, "CHRAFF add=a100" )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        # Second client tries to get the pending job - should get nothing
        output = execAny( ns_client2,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=' + str( notifPort ) + ' timeout=3' )
        if output != "no_more_jobs=true":
            notifSocket.close()
            raise Exception( "Expect no jobs, received: " + output )

        time.sleep( 4 )
        try:
            # Exception is expected
            data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
            notifSocket.close()
            raise Exception( "Expected no notifications, received one: " +
                             data )
        except Exception, exc:
            if "Resource temporarily unavailable" not in str( exc ):
                notifSocket.close()
                raise

        # Second client tries to get another job
        output = execAny( ns_client2,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=' + str( notifPort ) + ' timeout=3' )

        # Should get notifications after this clear because
        # the a0 affinity becomes available
        execAny( ns_client1, "CLRN" )

        time.sleep( 4 )
        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
        notifSocket.close()

        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " +
                             data )
        return True

class Scenario1540( TestBase ):
    " Scenario 1540 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Reset client affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # First client holds a0 affinity
        ns_client = self.getNetScheduleService( 'TEST', 'scenario310' )
        ns_client.set_client_identification( 'node1', 'session1' )
        execAny( ns_client, "CHRAFF add=a0" )

        # Reader timeout is 5 sec
        time.sleep( 7 )

        ns_admin = self.getNetScheduleService( 'TEST', 'scenario310' )
        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'reader_preferred_affinities_reset' ] != True:
            raise Exception( "Expected to have preferred affinities reset, "
                             "received: " +
                             str( info[ 'reader_preferred_affinities_reset' ] ) )
        return True

class Scenario1541( TestBase ):
    " Scenario 1541 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Reset client affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # First client holds a0 affinity
        ns_client = self.getNetScheduleService( 'TEST', 'scenario1541' )
        ns_client.set_client_identification( 'node1', 'session1' )
        execAny( ns_client, "CHRAFF add=a0" )

        ns_admin = self.getNetScheduleService( 'TEST', 'scenario310' )

        affInfo = getAffinityInfo( ns_admin )
        if affInfo[ 'affinity_token' ] != 'a0':
            raise Exception( "Unexpected affinity registry content "
                             "after adding 1 reader preferred affinity (token)" )
        if affInfo[ 'reader_clients__preferred' ] != [ 'node1' ]:
            raise Exception( "Unexpected affinity registry content "
                            "after adding 1 reader preferred affinity (node)" )
        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'reader_preferred_affinities_reset' ] != False:
            raise Exception( "Expected to have reader preferred affinities non reset, "
                             "received: " +
                             str( info[ 'reader_preferred_affinities_reset' ] ) )

        # Reader timeout is 5 sec
        time.sleep( 7 )

        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'reader_preferred_affinities_reset' ] != True:
            raise Exception( "Expected to have reader preferred affinities reset, "
                             "received: " +
                             str( info[ 'reader_preferred_affinities_reset' ] ) )

        affInfo = getAffinityInfo( ns_admin )
        if affInfo[ 'affinity_token' ] != 'a0':
            raise Exception( "Unexpected affinity registry content "
                             "after reader is expired (token)" )
        if affInfo[ 'reader_clients__preferred' ] != None:
            raise Exception( "Unexpected affinity registry content "
                             "after reader is expired (node)" )

        try:
            output = execAny( ns_client,
                              'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        except Exception, excpt:
            if "ePrefAffExpired" in str( excpt ) or "expired" in str( excpt ):
                return True
            raise

        raise Exception( "Expected exception in READ2 and did not get it: " +
                         output )

class Scenario1542( TestBase ):
    " Scenario 1542 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Reset client affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # First client holds a0 affinity
        ns_client = self.getNetScheduleService( 'TEST', 'scenario1542' )
        ns_client.set_client_identification( 'node1', 'session1' )
        execAny( ns_client, "CHRAFF add=a0" )

        ns_admin = self.getNetScheduleService( 'TEST', 'scenario1542' )

        execAny( ns_client,
                 'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=0 '
                 'port=9007 timeout=4' )

        info = getNotificationInfo( ns_admin )
        if info[ 'client_node' ] != 'node1':
            raise Exception( "Unexpected client in the notifications list: " +
                             info[ 'client_node' ] )

        # Reader timeout is 5 sec
        time.sleep( 7 )

        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'reader_preferred_affinities_reset' ] != True:
            raise Exception( "Expected to have reader preferred affinities reset, "
                             "received: " +
                             str( info[ 'reader_preferred_affinities_reset' ] ) )

        affInfo = getAffinityInfo( ns_admin )
        if affInfo[ 'affinity_token' ] != 'a0':
            raise Exception( "Unexpected affinity registry content "
                             "after reader is expired (token)" )
        if 'reader_clients__preferred' in affInfo:
            if affInfo[ 'reader_clients__preferred' ] != None:
                raise Exception( "Unexpected affinity registry content "
                                 "after reader is expired (node)" )
        return True

class Scenario1543( TestBase ):
    " Scenario 1543 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Change affinity, wait till reader expired, set affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 7 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1543' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, "CHRAFF add=a1,a2" )

        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'reader_preferred_affinities_reset' ] != False:
            raise Exception( "Expected non-resetted reader preferred affinities" )

        # wait till the reader is expired
        time.sleep( 12 )

        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'reader_preferred_affinities_reset' ] != True:
            raise Exception( "Expected resetted reader preferred affinities" )
        if client[ 'number_of_reader_preferred_affinities' ] != 0:
            raise Exception( 'Unexpected length of reader_preferred_affinities' )

        execAny( ns_client, 'SETRAFF' )
        client = getClientInfo( ns_client, 'mynode', verbose = False )

        if client[ 'reader_preferred_affinities_reset' ] != False:
            raise Exception( "Expected non-resetted reader preferred affinities" \
                             " after SETRAFF" )
        if client[ 'number_of_reader_preferred_affinities' ] != 0:
            raise Exception( 'Unexpected length of reader_preferred_affinities' )

        execAny( ns_client, 'SETRAFF a4,a7' )
        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'reader_preferred_affinities_reset' ] != False:
            raise Exception( "Expected non-resetted reader preferred affinities" \
                             " after SETRAFF" )
        if client[ 'number_of_reader_preferred_affinities' ] != 2:
            raise Exception( 'Unexpected length of reader_preferred_affinities' )

        return True

class Scenario1544( TestBase ):
    " Scenario 1544 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 for outdated job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 4 )

        # Client #2 plays a passive role of holding an affinity (a2)
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario1544' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        execAny( ns_client2, "CHRAFF add=a2" )

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client2, "PUT " + jobID + " 0 nooutput" )

        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario1544' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        execAny( ns_client1, "CHRAFF add=a1" )

        output = execAny( ns_client1,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != 'no_more_jobs=true':
            raise Exception( "Expected no job, received: '" + output + "'" )

        # 10 seconds till the job becomes obsolete
        time.sleep( 12 )

        output = execAny( ns_client1,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobID:
            raise Exception( "Expected a job, received: '" + output + "'" )

        return True

class Scenario1545( TestBase ):
    " Scenario 1545 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for outdated READ job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 4 )

        # Client #2 plays a passive role of holding an affinity (a2)
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario1545' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        execAny( ns_client2, "CHRAFF add=a2" )

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )
        execAny( ns_client2, "PUT " + jobID + " 0 nooutput" )

        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario1545' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        execAny( ns_client1, "CHRAFF add=a1" )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        # Second client tries to get the pending job - should get nothing
        output = execAny( ns_client1,
                          'READ2 reader_aff=1 any_aff=0 exclusive_new_aff=1 port=' + str( notifPort ) + ' timeout=15' )
        if output != "no_more_jobs=true":
            notifSocket.close()
            raise Exception( "Expect no jobs, received: " + output )

        # 10 seconds till the job becomes outdated
        time.sleep( 12 )

        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
        notifSocket.close()
        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " + data )
        return True


class Scenario1600( TestBase ):
    " Scenario 1600 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, CHRAFF as identified (add a0, a2), " \
               "READ2 reader_aff = 1 affinity_may_change=1/0"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1600' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, "CHRAFF add=a0,a2" )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'a1' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )

        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 affinity_may_change=1' )
        if output != "no_more_jobs=false":
            raise Exception( "Expect no more jobs == false, but received: " + output )
        output = execAny( ns_client,
                          'READ2 reader_aff=1 any_aff=0 affinity_may_change=0' )
        if output != "no_more_jobs=true":
            raise Exception( "Expect no more jobs == true, but received: " + output )
        return True

class Scenario1601( TestBase ):
    " Scenario 1601 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with g1 " \
               "READ2 g='g2' group_may_change=1/0"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', group='g1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1601' )
        ns_client.set_client_identification( 'node', 'session' )

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != receivedJobID:
            raise Exception( "Unexpected job for execution" )

        output = execAny( ns_client,
                          'READ2 reader_aff=0 any_aff=1 group=g2 group_may_change=1' )
        if output != "no_more_jobs=false":
            raise Exception( "Expect no more jobs == false, but received: " + output )
        output = execAny( ns_client,
                          'READ2 reader_aff=0 any_aff=1 group=g2 group_may_change=0' )
        if output != "no_more_jobs=true":
            raise Exception( "Expect no more jobs == true, but received: " + output )
        return True


class Scenario1602( TestBase ):
    " Scenario 1602 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a2, SUBMIT with a1 " \
               "GET2 a=a1,a2 prioritized_aff=0, " \
               "GET2 a=a1,a2 prioritized_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobIDa2 = self.ns.submitJob( 'TEST', 'bla', affinity='a2' )
        jobIDa1 = self.ns.submitJob( 'TEST', 'bla', affinity='a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1602' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=0 '
                          'aff=a1,a2 prioritized_aff=0' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobIDa2:
            raise Exception( "Unexpected job for executing. "
                             "Expected with affinity a2" )

        execAny( ns_client, "RETURN2 job_key=" + jobKey +
                            " auth_token=" + values[ 'auth_token' ][ 0 ] +
                            " blacklist=0" )
        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=0 '
                          'aff=a1,a2 prioritized_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobIDa1:
            raise Exception( "Unexpected job for executing. "
                             "Expected with affinity a1" )
        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=0 '
                          'aff=a1,a2 prioritized_aff=0' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobIDa2:
            raise Exception( "Unexpected job for executing. "
                             "Expected with affinity a2 (second time)" )
        return True


class Scenario1603( TestBase ):
    " Scenario 1603 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a2, SUBMIT with a1 " \
               "GET2 a=a1,a2 wnode_aff=1 prioritized_aff=1 => ERR, " \
               "GET2 a=a1,a2 any_aff=1 prioritized_aff=1 => ERR, " \
               "GET2 a=a1,a2 exclusive_new_aff=1 prioritized_aff=1 => ERR"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', affinity='a2' )
        self.ns.submitJob( 'TEST', 'bla', affinity='a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1603' )
        ns_client.set_client_identification( 'node', 'session' )

        ex = False
        try:
            execAny( ns_client, 'GET2 wnode_aff=1 any_aff=0 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none  (case 1)" )

        ex = False
        try:
            execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none  (case 2)" )

        ex = False
        try:
            execAny( ns_client, 'GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=1 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none  (case 3)" )

        return True

class Scenario1604( TestBase ):
    " Scenario 1604 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 with prioritized_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1604' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=0 aff=a1,a2 port=' + str( notifPort ) + ' timeout=3 prioritized_aff=1' )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input', affinity='a2' )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result != 1:
            raise Exception( "Expect notifications but received none" )
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0


class Scenario1605( TestBase ):
    " Scenario 1605 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a2, SUBMIT with a1 " \
               "READ2 a=a1,a2 prioritized_aff=0, " \
               "READ2 a=a1,a2 prioritized_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobIDa2 = self.ns.submitJob( 'TEST', 'bla', affinity='a2' )
        jobIDa1 = self.ns.submitJob( 'TEST', 'bla', affinity='a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1602' )
        ns_client.set_client_identification( 'node', 'session' )

        jobID = self.ns.getJob( 'TEST' )[ 0 ]
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )
        jobID = self.ns.getJob( 'TEST' )[ 0 ]
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        # READ2 test
        output = execAny( ns_client,
                          'READ2 reader_aff=0 any_aff=0 '
                          'aff=a1,a2 prioritized_aff=0' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobIDa2:
            raise Exception( "Unexpected job for executing. "
                             "Expected with affinity a2" )

        execAny( ns_client, "RDRB job_key=" + jobKey +
                            " auth_token=" + values[ 'auth_token' ][ 0 ] )

        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario1602' )
        ns_client1.set_client_identification( 'node1', 'session' )

        output = execAny( ns_client1,
                          'READ2 reader_aff=0 any_aff=0 '
                          'aff=a1,a2 prioritized_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobIDa1:
            raise Exception( "Unexpected job for executing. "
                             "Expected with affinity a1" )
        output = execAny( ns_client1,
                          'READ2 reader_aff=0 any_aff=0 '
                          'aff=a1,a2 prioritized_aff=0' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobIDa2:
            raise Exception( "Unexpected job for executing. "
                             "Expected with affinity a2 (second time)" )
        return True


class Scenario1606( TestBase ):
    " Scenario 1606 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a2, SUBMIT with a1 " \
               "READ2 a=a1,a2 reader_aff=1 prioritized_aff=1 => ERR, " \
               "READ2 a=a1,a2 any_aff=1 prioritized_aff=1 => ERR, " \
               "READ2 a=a1,a2 exclusive_new_aff=1 prioritized_aff=1 => ERR"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', affinity='a2' )
        self.ns.submitJob( 'TEST', 'bla', affinity='a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1606' )
        ns_client.set_client_identification( 'node', 'session' )

        ex = False
        try:
            execAny( ns_client, 'READ2 reader_aff=1 any_aff=0 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none (case 1)" )

        ex = False
        try:
            execAny( ns_client, 'READ2 reader_aff=0 any_aff=1 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none (case 2)" )

        ex = False
        try:
            execAny( ns_client, 'READ2 reader_aff=0 any_aff=0 exclusive_new_aff=1 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none (case 3)" )

        return True


class Scenario1607( TestBase ):
    " Scenario 1607 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 with prioritized_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1607' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=0 any_aff=0 exclusive_new_aff=0 aff=a1,a2 port=' + str( notifPort ) + ' timeout=3 prioritized_aff=1' )

        # Submit a job
        jobID = self.ns.submitJob( 'TEST', 'input', affinity='a2' )
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )

        time.sleep( 3 )
        result = self.getNotif( notifSocket )
        notifSocket.close()
        if result != 1:
            raise Exception( "Expect notifications but received none" )
        return True

    def getNotif( self, s ):
        " Retrieves notifications "
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket" in str( ex ):
                raise
            pass
        return 0

class Scenario1608( TestBase ):
    " Scenario1608 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks RDRB blacklist=0|1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 5 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1608' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        jobID1 = self.ns.submitJob( 'TEST', 'bla' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla' )

        jobID = self.ns.getJob( 'TEST' )[ 0 ]
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )
        jobID = self.ns.getJob( 'TEST' )[ 0 ]
        execAny( ns_client, "PUT " + jobID + " 0 nooutput" )


        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobID1:
            raise Exception( "Unexpected job for executing. "
                             "Expected the first job" )

        execAny( ns_client, "RDRB job_key=" + jobKey +
                            " auth_token=" + values[ 'auth_token' ][ 0 ] +
                            " blacklist=0" )
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobID1:
            raise Exception( "Unexpected job for executing. "
                             "Expected the first job after RDRB blacklist=0" )

        execAny( ns_client, "RDRB job_key=" + jobKey +
                            " auth_token=" + values[ 'auth_token' ][ 0 ] +
                            " blacklist=1" )
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        if jobKey != jobID2:
            raise Exception( "Unexpected job for executing. "
                             "Expected the second job after RDRB blacklist=1" )

        return True

