#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.16.0
"""

import time
import socket
from netschedule_tests_pack import TestBase
from netschedule_tests_pack_4_10 import getClientInfo, changeAffinity, execAny

# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs


class Scenario600( TestBase ):
    " Scenario 600 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, FAIL -> check blacklist, timeout, GET -> check blacklist"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 5 )
        origJobID = self.ns.submitJob( 'TEST', 'blah' )

        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node1', 'session1' )

        # Check that the job is in the blacklist for this client
        ns_client = self.getNetScheduleService( 'TEST', 'scenario109' )
        client = getClientInfo( ns_client, 'node1' )

        if 'blacklisted_jobs' not in client:
            raise Exception( "No blacklisted jobs found" )
        if len( client[ 'blacklisted_jobs' ] ) != 1:
            raise Exception( "Unexpected number of blacklisted jobs" )
        if client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Unexpected job is in the blacklist" )

        # wait till the job is gone from the blacklist
        time.sleep( 6 )

        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        if jobID != origJobID:
            raise Exception( "Unexpected job given for execution" )

        # Check the blacklist again
        client = getClientInfo( ns_client, 'node1' )
        if 'number_of_blacklisted_jobs' not in client:
            raise Exception( "Expected no jobs in the blacklist, got something" )
        if client[ 'number_of_blacklisted_jobs' ] != 0:
            raise Exception( "Unexpected # of the blacklisted jobs. Expected 0." )

        return True


class Scenario601( TestBase ):
    " Scenario 601 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, FAIL -> check blacklist, timeout, check blacklist"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 5 )
        origJobID = self.ns.submitJob( 'TEST', 'bla' )

        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        self.ns.failJob( 'TEST', jobID, authToken, 4,
                         'blah-out', 'blah-err',
                         'node1', 'session1' )

        # Check that the job is in the blacklist for this client
        ns_client = self.getNetScheduleService( 'TEST', 'scenario109' )
        client = getClientInfo( ns_client, 'node1' )

        if 'blacklisted_jobs' not in client:
            raise Exception( "No blacklisted jobs found" )
        if len( client[ 'blacklisted_jobs' ] ) != 1:
            raise Exception( "Unexpected number of blacklisted jobs" )
        if client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Unexpected job is in the blacklist" )

        # wait till the job is gone from the blacklist
        time.sleep( 6 )

        # There is no more GET, but the job must not be in the blacklist

        # Check the blacklist again
        client = getClientInfo( ns_client, 'node1' )
        if 'number_of_blacklisted_jobs' not in client:
            raise Exception( "Expected no jobs in the blacklist, got something" )
        if client[ 'number_of_blacklisted_jobs' ] != 0:
            raise Exception( "Unexpected # of the blacklisted jobs. Expected 0." )

        return True


class Scenario602( TestBase ):
    " Scenario 602 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications to 2 WNs with a handicap timeout"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 6 )

        # Client1 for a job
        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario602' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        # Socket to receive notifications
        notifSocket1 = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket1.bind( ( "", 9007 ) )

        # Client2 for a job
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario602' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        # Socket to receive notifications
        notifSocket2 = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket2.bind( ( "", 9008 ) )

        execAny( ns_client1,
                 'GET2 wnode_aff=0 any_aff=1 exclusive_new_aff=0 port=9007 timeout=15' )
        execAny( ns_client2,
                 'GET2 wnode_aff=0 any_aff=1 exclusive_new_aff=0 port=9008 timeout=15' )

        # Submit a job and wait for notifications
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )        # analysis:ignore

        # First notification must come immediately and the second later on
        time.sleep( 0.1 )
        result1 = self.getNotif( notifSocket1, notifSocket2 )
        if result1 == 0:
            raise Exception( "Expected one notification, received nothing" )

        time.sleep( 10 )
        result2 = self.getNotif( notifSocket1, notifSocket2 )
        if result2 == 0:
            raise Exception( "Expected another notification, received nothing" )

        if result1 + result2 != 3:
            raise Exception( "Expected notifications to both worker nodes, " \
                             "received in the same" )

        return True

    @staticmethod
    def getNotif( s1, s2 ):
        try:
            data = s1.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket 1" )
            return 1
        except Exception, ex:
            if "Unexpected notification in socket 1" in str( ex ):
                raise
            pass

        try:
            data = s2.recv( 8192, socket.MSG_DONTWAIT )
            if "queue=TEST" not in data:
                raise Exception( "Unexpected notification in socket 2" )
            return 2
        except Exception, ex:
            if "Unexpected notification in socket 2" in str( ex ):
                raise
            pass
        return 0



class Scenario603( TestBase ):
    " Scenario 603 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Change affinity, wait till WN expired, set affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 7 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario603' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        changeAffinity( ns_client, [ 'a1', 'a2' ], [] )

        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'preferred_affinities_reset' ] != False:
            raise Exception( "Expected non-resetted preferred affinities" )

        # wait till the worker node is expired
        time.sleep( 12 )

        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'preferred_affinities_reset' ] != True:
            raise Exception( "Expected resetted preferred affinities" )
        if client[ 'number_of_preferred_affinities' ] != 0:
            raise Exception( 'Unexpected length of preferred_affinities' )

        execAny( ns_client, 'SETAFF' )
        client = getClientInfo( ns_client, 'mynode', verbose = False )

        if client[ 'preferred_affinities_reset' ] != False:
            raise Exception( "Expected non-resetted preferred affinities" \
                             " after SETAFF" )
        if client[ 'number_of_preferred_affinities' ] != 0:
            raise Exception( 'Unexpected length of preferred_affinities' )

        execAny( ns_client, 'SETAFF a4,a7' )
        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'preferred_affinities_reset' ] != False:
            raise Exception( "Expected non-resetted preferred affinities" \
                             " after SETAFF" )
        if client[ 'number_of_preferred_affinities' ] != 2:
            raise Exception( 'Unexpected length of preferred_affinities' )

        return True



# NS 4.16.5 tests

class Scenario700( TestBase ):
    " Scenario 700 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, check that the queue name is in the key"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'blah', 'a0' )
        if 'TEST' not in jobID:
            raise Exception( "Job key does not have queue in it" )

        return True

class Scenario701( TestBase ):
    " Scenario 701 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, check that the queue name is in the key"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'blah' )
        receivedJobID, authToken, \
        attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                          "node1", "session1" )

        if 'TEST' not in receivedJobID:
            raise Exception( "Job key does not have queue in it" )

        return True

class Scenario702( TestBase ):
    " Scenario 702 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, reconnect without queue, SST "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'blah' )

        ns_client = self.getNetScheduleService( '', 'scenario702' )
        ns_client.set_client_identification( 'node1', 'session1' )

        output = execAny( ns_client, 'SST ' + jobID )
        if output != "0":
            raise Exception( "Unexpected job state" )

        # Try to submit a job without the queue
        try:
            output = execAny( ns_client, 'SUBMIT blah')
        except Exception, exc:
            if 'Job queue is required' in str( exc ):
                return True
            raise

        raise Exception( "Exception is expected, received nothing" )



class Scenario703( TestBase ):
    " Scenario 703 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return """SUBMIT into first queue, reconnect to second queue,
                  SST for the first job, SUBMIT to second -> check the key """

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 2 )

        jobID = self.ns.submitJob( 'TEST2', 'blah' )
        if 'TEST2' not in jobID:
            raise Exception( "Job key does not have first queue in it" )

        ns_client = self.getNetScheduleService( 'TEST3', 'scenario703' )
        ns_client.set_client_identification( 'node1', 'session1' )

        output = execAny( ns_client, 'SST ' + jobID )
        if output != "0":
            raise Exception( "Unexpected job state" )

        # Try to submit a job to another queue
        jobID2 = execAny( ns_client, 'SUBMIT blah')
        if 'TEST3' not in jobID2:
            raise Exception( "Job key does not have second queue in it" )

        return True


# NS 4.16.9 tests

class Scenario800( TestBase ):
    " Scenario800 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Make sure GETP2 provides the info from a config file"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 8 )

        ns_client = self.getNetScheduleService( 'TEST1', 'scenario800' )
        output = execAny( ns_client, 'GETP2' )
        if "&nc::one=value+1" not in output and "nc.one=value+1" not in output:
            raise Exception( "First value is not found in the output" )
        if "&nc::two=value+2" not in output and "&nc.two=value+2" not in output:
            raise Exception( "Second value is not found in the output" )

        output = execAny( ns_client, 'SETQUEUE TEST2' )
        output = execAny( ns_client, 'GETP2' )
        if "&nc::three=value+3" not in output and "&nc.three=value+3" not in output:
            raise Exception( "Third value is not found in the output" )
        if "&nc::four=value+4" not in output and "&nc.four=value+4" not in output:
            raise Exception( "Fourth value is not found in the output" )

        return True


class Scenario801( TestBase ):
    " Scenario801 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "RECO after changes in the netcache_api values/sections"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 8 )

        self.ns.connect( 15 )
        try:
            self.ns.directLogin( '', 'netschedule_admin' )

            self.ns.setConfig( "9" )
            self.ns.directSendCmd( 'RECO' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'RECO failed: ' + reply[ 1 ] )
            if reply[ 1 ] != '"netcache_api_changed" ' \
                             '["api2" ' \
                             '{"deleted" ["four", "three"], ' \
                              '"added" ["added1", "added2"]}]':
                raise Exception( 'Unexpected output for RECO: ' + reply[ 1 ] )

            ns_client = self.getNetScheduleService( 'TEST2', 'scenario801' )
            output = execAny( ns_client, 'GETP2' )
            if "&nc::added1=addval1" not in output:
                raise Exception( "First changed value is not found in the output" )
            if "&nc::added2=addval2" not in output:
                raise Exception( "Second changed value is not found in the output" )
        except Exception, exc:
            self.ns.disconnect()
            raise

        self.ns.disconnect()
        return True


class Scenario900( TestBase ):
    " Scenario900 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "HEALTH command"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( '', 'scenario900' )
        ns_client.set_client_identification( 'node1', 'session1' )

        output = execAny( ns_client, 'HEALTH' )
        values = parse_qs( output, True, True )
        if int( values[ 'proc_thread_count' ][ 0 ] ) <= 0:
            raise Exception( "Unexpected number of threads" )
        if int( values[ 'proc_fd_used' ][ 0 ] ) <= 0:
            raise Exception( "Unexpected number of fd" )
        return True


# NS 4.16.10 tests

class Scenario1000( TestBase ):
    " Scenario1000 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start, RECO, GETCONF effective=0, GETCONF effective=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( '', 'netschedule_admin' )
        ns_client.set_client_identification( 'netschedule_admin', 'session1' )

        self.ns.setConfig( "1.1000" )
        output = execAny( ns_client, 'RECO' )   # analysis:ignore
        output1 = execAny( ns_client, 'GETCONF effective=0', isMultiline = True )
        output2 = execAny( ns_client, 'GETCONF effective=1', isMultiline = True )

        nonEffectiveMaxThreads = None
        nonEffectiveNetwrokTimeout = None
        nonEffectiveAdminClient = None
        for line in output1:
            if line.startswith( 'max_threads' ):
                value = line.split( "=" )[ 1 ].strip()[ 1 : -1 ]
                nonEffectiveMaxThreads = int( value )
                continue
            if line.startswith( 'network_timeout' ):
                value = line.split( "=" )[ 1 ].strip()[ 1 : -1 ]
                nonEffectiveNetwrokTimeout = int( value )
                continue
            if line.startswith( 'admin_client_name' ):
                value = line.split( "=" )[ 1 ].strip()[ 1 : -1 ]
                nonEffectiveAdminClient = value

        effectiveMaxThreads = None
        effectiveNetwrokTimeout = None
        effectiveAdminClient = None
        for line in output2:
            if line.startswith( 'max_threads' ):
                value = line.split( "=" )[ 1 ].strip()[ 1 : -1 ]
                effectiveMaxThreads = int( value )
                continue
            if line.startswith( 'network_timeout' ):
                value = line.split( "=" )[ 1 ].strip()[ 1 : -1 ]
                effectiveNetwrokTimeout = int( value )
                continue
            if line.startswith( 'admin_client_name' ):
                value = line.split( "=" )[ 1 ].strip()[ 1 : -1 ]
                effectiveAdminClient = value

        # Parameters cannot be changed since startup
        if nonEffectiveMaxThreads != 10:
            raise Exception( "Unexpected !eff max threads: " +
                             str( nonEffectiveMaxThreads ) + ". Expected 10" )
        if effectiveMaxThreads != 5:
            raise Exception( "Unexpected eff max threads: " +
                             str( effectiveMaxThreads ) + ". Expected 5" )
        if nonEffectiveNetwrokTimeout != 360:
            raise Exception( "Unexpected !eff network timeout: " +
                             str( nonEffectiveNetwrokTimeout ) + ". Expected 360" )
        if effectiveNetwrokTimeout != 180:
            raise Exception( "Unexpected eff network timeout: " +
                             str( effectiveNetwrokTimeout ) + ". Expected 180" )

        # Parameters has to be changed
        if nonEffectiveAdminClient != "netschedule_admin":
            raise Exception( "Unexpected !eff admin clients: " +
                             nonEffectiveAdminClient + ". Expected 'netschedule_admin'" )
        if effectiveAdminClient != "netschedule_admin":
            raise Exception( "Unexpected eff admin clients: " +
                             effectiveAdminClient + ". Expected 'netschedule_admin'" )

        return True

class Scenario1100( TestBase ):
    " Scenario1100 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start with scrambled keys, SUBMIT"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        origJobID = self.ns.submitJob( 'TEST', 'bla' )
        if "JSID" in origJobID:
            raise Exception( "Unexpected job key format: " + origJobID )
        return True

class Scenario1101( TestBase ):
    " Scenario1101 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start with scrambled keys, SUBMIT, SST2"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        jobID = self.ns.submitJob( 'TEST', 'blah' )
        if "JSID" in jobID:
            raise Exception( "Unexpected job key format: " + jobID )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1101' )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Pending']:
            raise Exception( "Unexpected SST2 output" )
        return True


class Scenario1102( TestBase ):
    " Scenario1102 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start with scrambled keys, SUBMIT, GET2"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        jobID = self.ns.submitJob( 'TEST', 'blah' )
        if "JSID" in jobID:
            raise Exception( "Unexpected job key format: " + jobID )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1102' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ]
        if jobKey[ 0 ] != jobID:
            raise Exception( "Unexpected GET2 output" )
        return True


class Scenario1103( TestBase ):
    " Scenario1103 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start with scrambled keys, SUBMIT, PUT2"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        jobID = self.ns.submitJob( 'TEST', 'blah' )
        if "JSID" in jobID:
            raise Exception( "Unexpected job key format: " + jobID )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1103' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ]
        if jobKey[ 0 ] != jobID:
            raise Exception( "Unexpected GET2 output" )

        authToken = values[ 'auth_token' ][ 0 ]
        execAny( ns_client, "PUT2 " + jobID + " " + authToken + " 0 output" )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Done']:
            raise Exception( "Unexpected SST2 output" )
        return True

class Scenario1104( TestBase ):
    " Scenario1104 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start with scrambled keys, SUBMIT, FPUT2"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        jobID = self.ns.submitJob( 'TEST', 'blah' )
        if "JSID" in jobID:
            raise Exception( "Unexpected job key format: " + jobID )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1104' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ]
        if jobKey[ 0 ] != jobID:
            raise Exception( "Unexpected GET2 output" )

        authToken = values[ 'auth_token' ][ 0 ]
        execAny( ns_client, "FPUT2 " + jobID + " " + authToken + " ErrMsg Output 2" )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Failed']:
            raise Exception( "Unexpected SST2 output" )
        return True


class Scenario1105( TestBase ):
    " Scenario1105 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start with scrambled keys, SUBMIT, RETURN2"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        jobID = self.ns.submitJob( 'TEST', 'blah' )
        if "JSID" in jobID:
            raise Exception( "Unexpected job key format: " + jobID )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1105' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ]
        if jobKey[ 0 ] != jobID:
            raise Exception( "Unexpected GET2 output" )

        authToken = values[ 'auth_token' ][ 0 ]
        execAny( ns_client, "RETURN2 " + jobID + " " + authToken )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Pending']:
            raise Exception( "Unexpected SST2 output" )
        return True

class Scenario1106( TestBase ):
    " Scenario1106 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start with scrambled keys, SUBMIT, CANCEL"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        jobID = self.ns.submitJob( 'TEST', 'blah' )
        if "JSID" in jobID:
            raise Exception( "Unexpected job key format: " + jobID )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1106' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ]
        if jobKey[ 0 ] != jobID:
            raise Exception( "Unexpected GET2 output" )

        authToken = values[ 'auth_token' ][ 0 ]         # analysis:ignore
        execAny( ns_client, "CANCEL " + jobID )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Canceled']:
            raise Exception( "Unexpected SST2 output" )
        return True

class Scenario1107( TestBase ):
    " Scenario1107 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Start without scrambled keys, SUBMIT, RECO with scrambled, SUBMIT"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1100 )
        jobID = self.ns.submitJob( 'TEST', 'blah' )
        if "JSID" in jobID:
            raise Exception( "Unexpected job key format: " + jobID )

        self.ns.connect( 15 )
        try:
            self.ns.directLogin( '', 'netschedule_admin' )

            self.ns.setConfig( "1" )
            self.ns.directSendCmd( 'RECO' )
            reply = self.ns.directReadSingleReply()
            if reply[ 0 ] != True:
                raise Exception( 'RECO failed: ' + reply[ 1 ] )
            if reply[ 1 ] != '"queue_changes"' \
                             ' {"TEST" {"failed_retries" [0, 3], ' \
                             '"scramble_job_keys" [true, false]}}':
                raise Exception( 'Unexpected output for RECO: ' + reply[ 1 ] )

            jobID = self.ns.submitJob( 'TEST', 'blah' )
            if "JSID" not in jobID:
                raise Exception( "Unexpected job key format: " + jobID )
        except Exception, exc:  # analysis:ignore
            self.ns.disconnect()
            raise

        self.ns.disconnect()
        return True


class Scenario1108( TestBase ):
    " Scenario1108 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Check that the service_to_queue section is processed"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 1200 )

        ns_client = self.getNetScheduleService( '', 'scenario1108' )
        ns_client.set_client_identification( 'node1', 'session1' )

        output = execAny( ns_client, 'STAT SERVICES' )
        values = parse_qs( output, True, True )
        if values[ 'service1' ][ 0 ] != 'TEST1':
            raise Exception( "Unexpected service1" )
        if values[ 'service11' ][ 0 ] != 'TEST1':
            raise Exception( "Unexpected service11" )
        if values[ 'service2' ][ 0 ] != 'TEST2':
            raise Exception( "Unexpected service2" )

        output = execAny( ns_client, "QINF2 service=service1" )
        values = parse_qs( output, True, True )
        if values[ 'queue_name' ][ 0 ] != 'TEST1':
            raise Exception( "Unexpected QINF2 for service1" )

        return True


class Scenario1109( TestBase ):
    " Scenario1109 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks that the linked sections are provides in the QINF2 command"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 8 )

        ns_client = self.getNetScheduleService( '', 'scenario1109' )
        ns_client.set_client_identification( 'node1', 'session1' )

        output = execAny( ns_client, 'QINF2 TEST1' )
        values = parse_qs( output, True, True )
        if values[ 'nc.one' ][ 0 ] != 'value 1':
            raise Exception( "Unexpected nc.one" )
        if values[ 'nc.two' ][ 0 ] != 'value 2':
            raise Exception( "Unexpected nc.two" )

        return True


class Scenario1110( TestBase ):
    " Scenario1110 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks that HEALTH informs about a reinit after crash alert"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.kill( "SIGKILL" )
        time.sleep( 1 )
        self.ns.start()
        time.sleep( 1 )

        ns_client = self.getNetScheduleService( '', 'scenario1110' )
        ns_client.set_client_identification( 'node1', 'session1' )

        output = execAny( ns_client, 'HEALTH' )
        values = parse_qs( output, True, True )
        if values[ 'alert_startaftercrash' ][ 0 ] != '1':
            raise Exception( "Unexpected alert_startaftercrash alert value" )

        return True

class Scenario1111( TestBase ):
    " Scenario1111 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks STAT ALERTS output"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( '', 'scenario1111' )
        ns_client.set_client_identification( 'node1', 'session1' )

        output = execAny( ns_client, 'STAT ALERTS', isMultiline = True )
        if len( output ) != 0:
            raise Exception( "Expected no alerts, received some:\n" + output )

        self.ns.kill( "SIGKILL" )
        time.sleep( 1 )
        self.ns.start()
        time.sleep( 1 )

        ns_client1 = self.getNetScheduleService( '', 'scenario1111' )
        ns_client1.set_client_identification( 'node1', 'session1' )

        output = "\n".join( execAny( ns_client1, 'STAT ALERTS', isMultiline = True ) )
        if '[alert startaftercrash]' not in output:
            raise Exception( "Alert startaftercrash is not found" )
        if 'acknowledged_time: n/a' not in output:
            raise Exception( "Acknowledge time is not found" )
        if 'on: true' not in output:
            raise Exception( "On status is not found" )
        if 'count: 1' not in output:
            raise Exception( "Count is not found" )

        return True


class Scenario1112( TestBase ):
    " Scenario1112 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks QPAUSE/QRESUME commands"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1112' )
        ns_client.set_client_identification( 'node1', 'session1' )
        ns_client.on_warning = self.report_warning

        output = execAny( ns_client, 'QINF2 TEST' )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "nopause":
            raise Exception( "Unexpected pause value. Expected: no pause" )

        output = execAny( ns_client, "QPAUSE" )

        output = execAny( ns_client, 'QINF2 TEST' )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "nopullback":
            raise Exception( "Unexpected pause value. Expected: nopullback" )

        output = execAny( ns_client, "QPAUSE pullback=1" )

        output = execAny( ns_client, 'QINF2 TEST' )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "pullback":
            raise Exception( "Unexpected pause value. Expected: pullback" )

        output = execAny( ns_client, "QRESUME" )

        output = execAny( ns_client, 'QINF2 TEST' )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "nopause":
            raise Exception( "Unexpected pause value. Expected: no pause" )

        return True


class Scenario1113( TestBase ):
    " Scenario1113 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks GET2 for paused queue"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1113' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        jobID = self.ns.submitJob( 'TEST', 'blah' )
        output = execAny( ns_client, "QPAUSE pullback=0" )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "nopullback":
            raise Exception( "Unexpected GET2 output" )
        if "job_key" in values:
            raise Exception( "Expected no jobs, got one" )

        output = execAny( ns_client, "QPAUSE pullback=1" )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "pullback":
            raise Exception( "Unexpected GET2 output" )
        if "job_key" in values:
            raise Exception( "Expected no jobs, got one" )

        output = execAny( ns_client, "QRESUME" )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ]
        if jobKey[ 0 ] != jobID:
            raise Exception( "Unexpected job key after QRESUME" )
        return True


class Scenario1114( TestBase ):
    " Scenario1114 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks SST2, WST2, STATUS2 for paused queue"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1114' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        jobID = self.ns.submitJob( 'TEST', 'blah' )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ]
        if jobKey[ 0 ] != jobID:
            raise Exception( "Unexpected job key" )

        output = execAny( ns_client, "QPAUSE pullback=0" )
        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "nopullback":
            raise Exception( "Unexpected SST2 output, expected nopullback" )
        output = execAny( ns_client, 'WST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "nopullback":
            raise Exception( "Unexpected WST2 output, expected nopullback" )
        output = execAny( ns_client, 'STATUS2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "nopullback":
            raise Exception( "Unexpected STATUS2 output, expected nopullback" )

        output = execAny( ns_client, "QPAUSE pullback=1" )
        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "pullback":
            raise Exception( "Unexpected SST2 output, expected pullback" )
        output = execAny( ns_client, 'WST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "pullback":
            raise Exception( "Unexpected WST2 output, expected pullback" )
        output = execAny( ns_client, 'STATUS2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ "pause" ][ 0 ] != "pullback":
            raise Exception( "Unexpected STATUS2 output, expected pullback" )

        output = execAny( ns_client, "QRESUME" )
        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if 'pause' in values:
            raise Exception( "Unexpected SST2 after QRESUME" )
        output = execAny( ns_client, 'WST2 ' + jobID )
        values = parse_qs( output, True, True )
        if 'pause' in values:
            raise Exception( "Unexpected WST2 after QRESUME" )
        output = execAny( ns_client, 'STATUS2 ' + jobID )
        values = parse_qs( output, True, True )
        if 'pause' in values:
            raise Exception( "Unexpected STATUS2 after QRESUME" )

        return True


class Scenario1115( TestBase ):
    " Scenario1115 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    def report_warning( self, msg, server ):
        " Just ignore it "
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Checks RETURN2 blacklist=0|1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 5 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1115' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        jobID1 = self.ns.submitJob( 'TEST', 'blah' )
        jobID2, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                             "node", "session" )
        if jobID1 != jobID2:
            raise Exception( "Unexpected jobID" )

        execAny( ns_client, 'RETURN2 job_key=' + jobID1 + " auth_token=" +
                            authToken + " blacklist=0" )

        jobID3, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                             "node", "session" )
        if jobID1 != jobID3:
            raise Exception( "Unexpected jobID" )

        execAny( ns_client, 'RETURN2 job_key=' + jobID1 + " auth_token=" +
                            authToken + " blacklist=1" )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        if output.strip() != '':
            raise Exception( "No job expected, received one" )
        return True


class Scenario1116( TestBase ):
    " Scenario1116 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SETCLIENTDATA and check the version"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1116' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client, 'SETCLIENTDATA data="abc cde"' )
        if "version=1" not in output:
            raise Exception( "Unexpected version of the client data. Expected: "
                             "'version=1', received: " + output )

        output = execAny( ns_client, 'SETCLIENTDATA data="123 456"' )
        if "version=2" not in output:
            raise Exception( "Unexpected version of the client data. Expected: "
                             "'version=2', received: " + output )

        output = "\n".join( execAny( ns_client, 'STAT CLIENTS', isMultiline = True ) )
        if "DATA: '123 456'" not in output:
            raise Exception( "Cannot find expected client data" )
        if 'DATA VERSION: 2' not in output:
            raise Exception( "Cannot find expected client data version" )
        return True


class Scenario1117( TestBase ):
    " Scenario1117 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SETCLIENTDATA and the version mismatch"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1117' )
        ns_client.set_client_identification( 'node', 'session' )

        output = execAny( ns_client, 'SETCLIENTDATA data="abc cde"' )
        if "version=1" not in output:
            raise Exception( "Unexpected version of the client data. Expected: "
                             "'version=1', received: " + output )

        output = execAny( ns_client, 'SETCLIENTDATA data="123 456" version=1' )
        if "version=2" not in output:
            raise Exception( "Unexpected version of the client data. Expected: "
                             "'version=2', received: " + output )

        try:
            output = execAny( ns_client, 'SETCLIENTDATA data="123 456" version=1' )
        except Exception, exc:
            if 'client data version does not match' in str( exc ):
                return True
            raise

        raise Exception( "Expected data mismatch exception, received output: " +
                         output )
