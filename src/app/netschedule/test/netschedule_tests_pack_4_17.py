#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.17.0
"""

import time
import socket
from netschedule_tests_pack import TestBase
from netschedule_tests_pack_4_10 import execAny

# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs


class Scenario1300( TestBase ):
    " Scenario 1300 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = ""
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a group and affinity, CANCEL with affinity"

    def report_warning( self, msg, server ):
        " Just ignore it "
        self.warning = msg
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1300' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        jobID = execAny( ns_client, "SUBMIT blah aff=a1 group=g2" )

        self.warning = ""
        output = execAny( ns_client, "CANCEL group=nonexistent" )
        if 'not found' not in self.warning:
            raise Exception( "Error cancelling a job by a wrong group. "
                             "Expected warning, got nothing" )

        self.warning = ""
        output = execAny( ns_client, "CANCEL aff=nonexistent" )
        if 'not found' not in self.warning:
            raise Exception( "Error cancelling a job by a wrong affinity. "
                             "Expected warning, got nothing" )

        output = execAny( ns_client, 'SST ' + jobID )
        if output != "0":
            raise Exception( "Unexpected job state: " + output )

        output = execAny( ns_client, "CANCEL aff=a1" )
        output = execAny( ns_client, 'SST ' + jobID )
        if output != "3":   # cancelled
            raise Exception( "Unexpected job state: " + output )

        return True


class Scenario1301( TestBase ):
    " Scenario 1301 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Getting a job for reading from the Canceled state"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.ns.cancelJob( 'TEST', jobID, 'mynode', 'mysession' )

        key, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                           'mynode',
                                                           'mysession' )

        if not key.startswith( "JSID_" ) or state != "Canceled":
            raise Exception( "Could not get a job for reading "
                             "from the Canceled state" )

        return True


class Scenario1302( TestBase ):
    " Scenario 1302 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Getting a job for reading from the Failed state"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1302' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )

        output = execAny( ns_client, "READ" )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Failed":
            raise Exception( "Could not get a job for reading "
                             "from the Failed state" )

        return True


class Scenario1303( TestBase ):
    " Scenario 1303 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Once read job is not provided the second time"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1303' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.putJob( 'TEST', jobID, authToken, 0,
                        'blah-out', 'node', 'session' )

        output = execAny( ns_client, "READ" )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Done":
            raise Exception( "Could not get a job for reading "
                             "from the Done state" )

        execAny( ns_client, "CFRM " + jobKey + " " + authToken )

        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario1303' )
        ns_client1.set_client_identification( 'readnode1', 'readsession1' )
        output = execAny( ns_client1, "READ" )
        values = parse_qs( output, True, True )
        if 'job_key' in values:
            raise Exception( "Not expected Confirmed job but received it" )

        execAny( ns_client, "CANCELQ" )
        output = execAny( ns_client1, "READ" )
        values = parse_qs( output, True, True )
        if 'job_key' in values:
            raise Exception( "Not expected Confirmed and Canceled job but received it" )

        return True


class Scenario1304( TestBase ):
    " Scenario 1304 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Group limit for the READ jobs"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1304' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "SUBMIT blah group=g1" )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.putJob( 'TEST', jobID, authToken, 0,
                         'blah-out', 'node', 'session' )

        output = execAny( ns_client, "READ group=g2" )
        values = parse_qs( output, True, True )
        if 'job_key' in values:
            raise Exception( "Not expected a job from group g2 but received it" )

        output = execAny( ns_client, "READ group=g1" )
        values = parse_qs( output, True, True )
        if 'job_key' not in values:
            raise Exception( "Expected a job from group g1 but not received it" )

        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        # state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ):
            raise Exception( "Could not get a job for reading "
                             "from the group g1" )

        return True


class Scenario1305( TestBase ):
    " Scenario 1305 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "RDRB for Canceled job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1305' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "SUBMIT blah group=g1" )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "CANCELQ" )

        output = execAny( ns_client, "READ group=g1" )
        values = parse_qs( output, True, True )
        if 'job_key' not in values:
            raise Exception( "Expected a job from group g1 but not received it" )

        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Canceled":
            raise Exception( "Could not get a job for reading "
                             "from the canceled state" )

        execAny( ns_client, "RDRB " + jobKey + " " + authToken )
        status = self.ns.getJobStatus( 'TEST', jobKey )
        if status != "Canceled":
            raise Exception( "RDRB does not return the job to the Canceled state" )

        return True

class Scenario1306( TestBase ):
    " Scenario 1306 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "RDRB for Failed job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1306' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "SUBMIT blah group=g1" )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )

        output = execAny( ns_client, "READ group=g1" )
        values = parse_qs( output, True, True )
        if 'job_key' not in values:
            raise Exception( "Expected a job from group g1 but not received it" )

        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Failed":
            raise Exception( "Could not get a job for reading "
                             "from the failed state" )

        execAny( ns_client, "RDRB " + jobKey + " " + authToken )
        status = self.ns.getJobStatus( 'TEST', jobKey )
        if status != "Failed":
            raise Exception( "RDRB does not return the job to the Failed state" )

        return True


class Scenario1307( TestBase ):
    " Scenario 1307 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "read timeout for Canceled job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 10 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1307' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "SUBMIT blah group=g1" )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "CANCELQ" )

        output = execAny( ns_client, "READ group=g1" )
        values = parse_qs( output, True, True )
        if 'job_key' not in values:
            raise Exception( "Expected a job from group g1 but not received it" )

        jobKey = values[ 'job_key' ][ 0 ]
        # authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Canceled":
            raise Exception( "Could not get a job for reading "
                             "from the canceled state" )

        # read timeout is set to 2 in the config file
        time.sleep( 4 )
        status = self.ns.getJobStatus( 'TEST', jobKey )
        if status != "Canceled":
            raise Exception( "read timeout does not return the job to "
                             "the Canceled state. State: " + status )

        return True


class Scenario1308( TestBase ):
    " Scenario 1308 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "read timeout for Failed job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 10 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1308' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "SUBMIT blah group=g1" )

        # Need to fail the job twice because the retry is set to 1
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node1", "session1" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node1', 'session1' )

        output = execAny( ns_client, "READ group=g1" )
        values = parse_qs( output, True, True )
        if 'job_key' not in values:
            raise Exception( "Expected a job from group g1 but not received it" )

        jobKey = values[ 'job_key' ][ 0 ]
        # authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Failed":
            raise Exception( "Could not get a job for reading "
                             "from the Failed state" )

        # read timeout is set to 2 in the config file
        time.sleep( 4 )
        status = self.ns.getJobStatus( 'TEST', jobKey )
        if status != "Failed":
            raise Exception( "read timeout does not return the job to "
                             "the Failed state. State: " + status )

        return True


class Scenario1309( TestBase ):
    " Scenario 1309 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Session cleared for Canceled and read job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 10 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1309' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "SUBMIT blah group=g1" )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "CANCELQ" )

        output = execAny( ns_client, "READ group=g1" )
        values = parse_qs( output, True, True )
        if 'job_key' not in values:
            raise Exception( "Expected a job from group g1 but not received it" )

        jobKey = values[ 'job_key' ][ 0 ]
        # authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Canceled":
            raise Exception( "Could not get a job for reading "
                             "from the canceled state" )

        # Session is explicitly cleared
        execAny( ns_client, "CLRN" )
        status = self.ns.getJobStatus( 'TEST', jobKey )
        if status != "Canceled":
            raise Exception( "read timeout does not return the job to "
                             "the Canceled state. State: " + status )

        return True


class Scenario1310( TestBase ):
    " Scenario 1310 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CLRN for Failed and read job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 10 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1310' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        execAny( ns_client, "SUBMIT blah group=g1" )

        # Need to fail the job twice because the retry is set to 1
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node1", "session1" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node1', 'session1' )

        output = execAny( ns_client, "READ group=g1" )
        values = parse_qs( output, True, True )
        if 'job_key' not in values:
            raise Exception( "Expected a job from group g1 but not received it" )

        jobKey = values[ 'job_key' ][ 0 ]
        # authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Failed":
            raise Exception( "Could not get a job for reading "
                             "from the Failed state" )

        # Session is explicitly cleared
        execAny( ns_client, "CLRN" )
        status = self.ns.getJobStatus( 'TEST', jobKey )
        if status != "Failed":
            raise Exception( "read timeout does not return the job to "
                             "the Failed state. State: " + status )

        return True


class Scenario1311( TestBase ):
    " Scenario 1311 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Once read job is not provided the second time"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1311' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.putJob( 'TEST', jobID, authToken, 0,
                         'blah-out', 'node', 'session' )

        output = execAny( ns_client, "READ" )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        state = values[ 'status' ][ 0 ]

        if not jobKey.startswith( "JSID_" ) or state != "Done":
            raise Exception( "Could not get a job for reading "
                             "from the Done state" )

        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario1311' )
        ns_client1.set_client_identification( 'readnode1', 'readsession1' )
        output = execAny( ns_client1, "READ" )
        values = parse_qs( output, True, True )
        if 'job_key' in values:
            raise Exception( "Not expected reading job but received it" )
        if 'no_more_jobs' not in values:
            raise Exception( "Expected no_more_jobs flag but not found it" )
        if values[ 'no_more_jobs' ][ 0 ] != 'false':
            raise Exception( "Expected no_more_jobs == false, received: " +
                             values[ 'no_more_jobs' ][ 0 ] )

        execAny( ns_client, "CFRM " + jobKey + " " + authToken )

        output = execAny( ns_client1, "READ" )
        values = parse_qs( output, True, True )
        if 'job_key' in values:
            raise Exception( "Not expected reading job but received it" )
        if 'no_more_jobs' not in values:
            raise Exception( "Expected no_more_jobs flag but not found it" )
        if values[ 'no_more_jobs' ][ 0 ] != 'true':
            raise Exception( "Expected no_more_jobs == true, received: " +
                             values[ 'no_more_jobs' ][ 0 ] )

        return True


class Scenario1312( TestBase ):
    " Scenario 1312 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ notification when a job is Done"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1312' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15' )

        # Submit a job and move it to Done state
        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.putJob( 'TEST', jobID, authToken, 0,
                         'blah-out', 'node', 'session' )

        time.sleep( 10 )
        if not self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected notification, received nothing" )
        notifSocket.close()
        return True


class Scenario1313( TestBase ):
    " Scenario 1313 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ notification when a job is Failed"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1313' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15' )

        # Submit a job and move it to Done state
        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )

        time.sleep( 10 )
        if not self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected notification, received nothing" )
        notifSocket.close()
        return True


class Scenario1314( TestBase ):
    " Scenario 1314 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ notification when a job is Canceled"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1314' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15' )

        # Submit a job and move it to Done state
        self.ns.submitJob( 'TEST', 'blah' )
        execAny( ns_client, "CANCELQ" )

        time.sleep( 10 )
        if not self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected notification, received nothing" )
        notifSocket.close()
        return True


class Scenario1315( TestBase ):
    " Scenario 1315 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ notification when a job is Done (with groups)"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1315' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15 group=group1' )

        # Submit a job and move it to Done state
        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID1, authToken1, attrs1, jobInput1 = self.ns.getJob( "TEST", -1, '', '',
                                                                "node", "session" )

        self.ns.submitJob( 'TEST', 'blah', '', 'group2' )
        jobID2, authToken2, attrs2, jobInput2 = self.ns.getJob( "TEST", -1, '', '',
                                                                "node", "session" )

        # Put the non expected group2 job
        self.ns.putJob( 'TEST', jobID2, authToken2, 0,
                         'blah-out', 'node', 'session' )
        time.sleep( 3 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )

        time.sleep( 1 )
        self.ns.putJob( 'TEST', jobID1, authToken1, 0,
                         'blah-out', 'node', 'session' )
        time.sleep( 3 )
        if not self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected notification, received nothing" )
        notifSocket.close()
        return True

class Scenario1316( TestBase ):
    " Scenario 1316 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ notification when a job is Failed (with groups)"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1316' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15 group=group1' )

        # Submit a job and move it to Done state
        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID1, authToken1, attrs1, jobInput1 = self.ns.getJob( "TEST", -1, '', '',
                                                                "node", "session" )

        self.ns.submitJob( 'TEST', 'blah', '', 'group2' )
        jobID2, authToken2, attrs2, jobInput2 = self.ns.getJob( "TEST", -1, '', '',
                                                                "node", "session" )

        # run_timeout = 7 secs
        # Fail the non expected group2 job
        self.ns.failJob( 'TEST', jobID2, authToken2, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )
        time.sleep( 2 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )

        self.ns.failJob( 'TEST', jobID1, authToken1, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )
        time.sleep( 2 )
        if not self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected notification, received nothing" )
        notifSocket.close()
        return True


class Scenario1317( TestBase ):
    " Scenario 1317 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ notification when a job is Canceled (with groups)"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1317' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15 group=group1' )

        # Submit a job and move it to Done state
        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID1, authToken1, attrs1, jobInput1 = self.ns.getJob( "TEST", -1, '', '',
                                                                "node", "session" )

        self.ns.submitJob( 'TEST', 'blah', '', 'group2' )
        jobID2, authToken2, attrs2, jobInput2 = self.ns.getJob( "TEST", -1, '', '',
                                                                "node", "session" )

        # Run timeout is set to 7 sec in the config file #3.
        # So the test should complete before 7 seconds are over because otherwise
        # the job is taken away from a WNode due to a timeout
        # Fail the non expected group2 job
        execAny( ns_client, "CANCEL group=group2" )
        time.sleep( 3 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )

        execAny( ns_client, "CANCEL group=group1" )
        time.sleep( 3 )
        if not self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected notification, received nothing" )
        notifSocket.close()
        return True



class Scenario1318( TestBase ):
    " Scenario 1318 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ, CWREAD, Done job appeared: no notifications"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1318' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15 group=group1' )
        time.sleep( 1 )
        execAny( ns_client, "CWREAD" )

        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.putJob( 'TEST', jobID, authToken, 0,
                         'blah-out', 'node', 'session' )

        time.sleep( 7 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )
        notifSocket.close()
        return True


class Scenario1319( TestBase ):
    " Scenario 1319 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ, CWREAD, Failed job appeared: no notifications"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1319' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15 group=group1' )
        time.sleep( 1 )
        execAny( ns_client, "CWREAD" )

        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )

        time.sleep( 7 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )
        notifSocket.close()
        return True


class Scenario1320( TestBase ):
    " Scenario 1320 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ, CWREAD, Canceled job appeared: no notifications"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1320' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=15 group=group1' )
        time.sleep( 1 )
        execAny( ns_client, "CWREAD" )

        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "CANCELQ" )

        time.sleep( 7 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )
        notifSocket.close()
        return True


class Scenario1321( TestBase ):
    " Scenario 1321 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ, timeout, Done job appeared: no notifications"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1321' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=10 group=group1' )
        time.sleep( 11 )

        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.putJob( 'TEST', jobID, authToken, 0,
                         'blah-out', 'node', 'session' )

        time.sleep( 7 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )
        notifSocket.close()
        return True

class Scenario1322( TestBase ):
    " Scenario 1322 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ, timeout, Failed job appeared: no notifications"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1322' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=10 group=group1' )
        time.sleep( 11 )

        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node', 'session' )

        time.sleep( 7 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )
        notifSocket.close()
        return True


class Scenario1323( TestBase ):
    " Scenario 1323 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ, timeout, Canceled job appeared: no notifications"

    @staticmethod
    def hasNotification( s ):
        try:
            data = s.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data or "reason=read" not in data:
                raise Exception( "Unexpected notification" )
            return True
        except Exception, ex:
            if "Unexpected notification" in str( ex ):
                raise
            pass
        return False

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1323' )
        ns_client.set_client_identification( 'readnode', 'readsession' )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client, 'READ port=' + str( notifPort ) + ' timeout=10 group=group1' )
        time.sleep( 11 )

        self.ns.submitJob( 'TEST', 'blah', '', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "CANCELQ" )

        time.sleep( 7 )
        if self.hasNotification( notifSocket ):
            notifSocket.close()
            raise Exception( "Expected no notifications, received some" )
        notifSocket.close()
        return True


class Scenario1324( TestBase ):
    " Scenario 1324 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET, RESCHEDULE with new affinity and new group"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1324' )
        ns_client.set_client_identification( 'node', 'session' )

        self.ns.submitJob( 'TEST', 'blah', 'aff1', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "RESCHEDULE " + jobID + " " + authToken + " aff2 group2" )
        output = execAny( ns_client, "GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=0 aff=aff1" )
        if output != "":
            raise Exception( "Not expected job with old affinity" )
        output = execAny( ns_client, "DUMP " + jobID, isMultiline = True )
        if "affinity: 2 ('aff2')" not in output:
            raise Exception( "Unexpected affinity of the job" )
        if "group: 2 ('group2')" not in output:
            raise Exception( "Unexpected group of the job" )

        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, 'aff2', '',
                                                            "node", "session" )
        if jobID == "":
            raise Exception( "Expected job with new affinity" )
        return True


class Scenario1325( TestBase ):
    " Scenario 1325 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET, RESCHEDULE without affinity and group"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1325' )
        ns_client.set_client_identification( 'node', 'session' )

        self.ns.submitJob( 'TEST', 'blah', 'aff1', 'group1' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "RESCHEDULE " + jobID + " " + authToken )
        output = execAny( ns_client, "GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=0 aff=aff1" )
        if output != "":
            raise Exception( "Not expected job with old affinity" )
        output = execAny( ns_client, "DUMP " + jobID, isMultiline = True )
        if "affinity: n/a" not in output:
            raise Exception( "Unexpected affinity of the job" )
        if "group: n/a" not in output:
            raise Exception( "Unexpected group of the job" )

        output = execAny( ns_client, "GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=0 group=group1" )
        if output != "":
            raise Exception( "Not expected job with old group" )
        return True


class Scenario1326( TestBase ):
    " Scenario 1326 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with no aff, no group, GET, RESCHEDULE with new aff and group"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1326' )
        ns_client.set_client_identification( 'node', 'session' )

        self.ns.submitJob( 'TEST', 'blah', '', '' )
        output = execAny( ns_client, "GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=0 aff=aff1" )
        if output != "":
            raise Exception( "Not expected job with aff1" )

        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        execAny( ns_client, "RESCHEDULE " + jobID + " " + authToken + " aff1 group1" )
        output = execAny( ns_client, "GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=0 aff=aff1" )
        if output == "":
            raise Exception( "Expected job with aff1" )
        output = execAny( ns_client, "DUMP " + jobID, isMultiline = True )
        if "affinity: 1 ('aff1')" not in output:
            raise Exception( "Unexpected affinity of the job" )
        if "group: 1 ('group1')" not in output:
            raise Exception( "Unexpected group of the job" )
        return True


class Scenario1400( TestBase ):
    " Scenario 1400 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "DUMP of jobs with certain affinity"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1400' )
        ns_client.set_client_identification( 'node', 'session' )

        # No such affinity, there must be nothing
        output = execAny( ns_client, "DUMP aff=888", isMultiline = True )
        if len( output ) != 0:
            raise Exception( "Expected no dumped jobs by non existing "
                             "affinity, received some" )

        output = execAny( ns_client, "DUMP group=888", isMultiline = True )
        if len( output ) != 0:
            raise Exception( "Expected no dumped jobs by non existing "
                             "group, received some" )

        execAny( ns_client, "SUBMIT blah aff=777 group=111" )
        execAny( ns_client, "SUBMIT blah aff=777 group=222" )
        execAny( ns_client, "SUBMIT blah aff=999 group=111" )

        # DUMP by affinity
        output = execAny( ns_client, "DUMP aff=777", isMultiline = True )
        jobCount = "\n".join( output ).count( "affinity: 1 ('777')" )
        if jobCount != 2:
            raise Exception( "Expected two jobs dumped by affinity, "
                             "received " + str( jobCount ) )

        output = execAny( ns_client, "DUMP aff=999", isMultiline = True )
        jobCount = "\n".join( output ).count( "affinity: 2 ('999')" )
        if jobCount != 1:
            raise Exception( "Expected one job dumped by affinity, "
                             "received " + str( jobCount ) )

        # DUMP by group
        output = execAny( ns_client, "DUMP group=111", isMultiline = True )
        jobCount = "\n".join( output ).count( "group: 1 ('111')" )
        if jobCount != 2:
            raise Exception( "Expected two jobs dumped by group, "
                             "received " + str( jobCount ) )

        output = execAny( ns_client, "DUMP group=222", isMultiline = True )
        jobCount = "\n".join( output ).count( "group: 2 ('222')" )
        if jobCount != 1:
            raise Exception( "Expected one job dumped by group, "
                             "received " + str( jobCount ) )

        # DUMP by affinity and group
        output = execAny( ns_client, "DUMP group=222 aff=777", isMultiline = True )
        jobCount = "\n".join( output ).count( "group: 2 ('222')" )
        if jobCount != 1:
            raise Exception( "Expected one jobs dumped by group and affinity, "
                             "received " + str( jobCount ) )

        output = execAny( ns_client, "DUMP group=222 aff=999", isMultiline = True )
        if len( output ) != 0:
            raise Exception( "Expected no jobs dumped by group and affinity, "
                             "received " + str( jobCount ) )

        return True


class Scenario1401( TestBase ):
    " Scenario 1401 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "DUMP of jobs with certain affinity"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1401' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, "SUBMIT blah" )
        output = execAny( ns_client, "DUMP status=PENDING", isMultiline = True )
        jobCount = "\n".join( output ).count( 'status: Pending' )
        if jobCount != 1:
            raise Exception( "Expected one job in the output, received " +
                             str( jobCount ) )

        output = execAny( ns_client, "DUMP status=Done", isMultiline = True )
        if len( output ) != 0:
            raise Exception( "Expected no jobs dumped by status, "
                             "received " + str( jobCount ) )

        output = execAny( ns_client, "DUMP status=invented", isMultiline = True )
        if len( output ) != 0:
            raise Exception( "Expected no jobs dumped by invented status, "
                             "received " + str( jobCount ) )

        # Submit a second job
        execAny( ns_client, "SUBMIT blah" )
        output = execAny( ns_client, "GET2 wnode_aff=0 any_aff=1 exclusive_new_aff=0" )

        output = execAny( ns_client, "DUMP status=running", isMultiline = True )
        jobCount = "\n".join( output ).count( 'status: Running' )
        if jobCount != 1:
            raise Exception( "Expected one running job in the output, received " +
                             str( jobCount ) )

        output = execAny( ns_client, "DUMP status=running,invented,pending", isMultiline = True )
        jobCount = "\n".join( output ).count( 'status: ' )
        if jobCount != 2:
            raise Exception( "Expected two (running and pending) jobs in the output, received " +
                             str( jobCount ) )
        return True

class Scenario1402( TestBase ):
    " Scenario 1402 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CANCEL of jobs in certain state"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1402' )
        ns_client.set_client_identification( 'node', 'session' )

        jobID = self.ns.submitJob( 'TEST', 'blah' )
        execAny( ns_client, "CANCEL status=done" )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Pending']:
            raise Exception( "Expected 'Pending' status, received '" +
                             values[ 'job_status' ] + "'" )

        execAny( ns_client, "CANCEL status=PENDING" )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Canceled']:
            raise Exception( "Expected 'Canceled' status, received '" +
                             values[ 'job_status' ] + "'" )
        return True

class Scenario1403( TestBase ):
    " Scenario 1403 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = None
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        self.warning = msg
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CANCEL of jobs in certain state"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1403' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        jobID1 = self.ns.submitJob( 'TEST', 'blah' )
        jobID2 = self.ns.submitJob( 'TEST', 'blah' )

        execAny( ns_client, "GET" )
        execAny( ns_client, "CANCEL status=Running,splitting" )

        output = execAny( ns_client, 'SST2 ' + jobID1 )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Canceled']:
            raise Exception( "Expected 'Canceled' status, received '" +
                             values[ 'job_status' ] + "'" )

        output = execAny( ns_client, 'SST2 ' + jobID2 )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Pending']:
            raise Exception( "Expected 'Pending' status, received '" +
                             values[ 'job_status' ] + "'" )
        return True


class Scenario1404( TestBase ):
    " scenario 1404 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = None
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        self.warning = msg
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Forced job fail"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1404' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        output = execAny( ns_client, 'FPUT2 job_key=' + jobID +
                                     ' auth_token=' + authToken +
                                     ' err_msg=nomessage output=none job_return_code=1 no_retries=1' )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Failed']:
            raise Exception( "Expected 'Failed' status, received '" +
                             values[ 'job_status' ] + "'" )
        return True

class Scenario1405( TestBase ):
    " scenario 1405 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = None
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        self.warning = msg
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Forced read job fail"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1404' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )
        output = execAny( ns_client, 'FPUT2 job_key=' + jobID +
                                     ' auth_token=' + authToken +
                                     ' err_msg=nomessage output=none job_return_code=1 no_retries=1' )
        key, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                           'node1',
                                                           'session1' )

        output = execAny( ns_client, 'FRED job_key=' + key +
                                     ' auth_token=' + passport +
                                     ' err_msg=nomessage no_retries=1' )

        output = execAny( ns_client, 'SST2 ' + key )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['ReadFailed']:
            raise Exception( "Expected 'ReadFailed' status, received '" +
                             values[ 'job_status' ] + "'" )
        return True


class Scenario1406( TestBase ):
    " scenario 1406 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = None
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        self.warning = msg
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Forced read job fail"

    def execute( self ):
        " Returns True if successfull "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1406' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        output = execAny( ns_client, 'CANCELQ' )
        if output != "0":
            raise Exception( "Expected 0 canceled jobs, received: " + str( output ) )

        self.ns.submitJob( 'TEST', 'blah' )
        output = execAny( ns_client, 'CANCELQ' )
        if output != "1":
            raise Exception( "Expected 1 canceled jobs, received: " + str( output ) )
        output = execAny( ns_client, 'CANCELQ' )
        if output != "0":
            raise Exception( "Expected 0 canceled jobs, received: " + str( output ) )

        self.ns.submitJob( 'TEST', 'blah' )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                            "node", "session" )

        output = execAny( ns_client, 'CANCEL status=pending' )
        if output != "0":
            raise Exception( "Expected 0 canceled jobs, received: " + str( output ) )

        output = execAny( ns_client, 'CANCEL status=running' )
        if output != "1":
            raise Exception( "Expected 1 canceled jobs, received: " + str( output ) )

        return True
