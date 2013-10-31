#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.11.0
"""

import time
import socket
from netschedule_tests_pack import TestBase
from netschedule_tests_pack_4_10 import ( getClientInfo, getAffinityInfo,
                                          getNotificationInfo, changeAffinity,
                                          execAny )

# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs



class Scenario300( TestBase ):
    " Scenario 300 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Check the extended WST2 output and the job expiration time"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob(  'TEST', 'bla', '', '', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario300' )

        output = execAny( ns_client, 'WST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Pending'] or \
           values.has_key( 'job_exptime' ) == False:
            raise Exception( "Unexpected WST2 output" )

        time1 = values[ 'job_exptime' ][ 0 ]
        time.sleep( 3 )

        output = execAny( ns_client, 'WST2 ' + jobID )
        values = parse_qs( output, True, True )
        time2 = values[ 'job_exptime' ][ 0 ]

        if time1 != time2:
            raise Exception( "WST2 changes the job "
                             "expiration while it must not" )

        return True

class Scenario301( TestBase ):
    " Scenario 301 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Check the extended SST2 output and the job expiration time"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario301' )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        if values[ 'job_status' ] != ['Pending'] or \
           values.has_key( 'job_exptime' ) == False:
            raise Exception( "Unexpected SST2 output" )

        time1 = int( values[ 'job_exptime' ][ 0 ] )
        time.sleep( 5 )

        output = execAny( ns_client, 'SST2 ' + jobID )
        values = parse_qs( output, True, True )
        time2 = int( values[ 'job_exptime' ][ 0 ] )

        if time2 - time1 < 3:
            raise Exception( "SST2 does not change the job "
                             "expiration while it must" )

        return True

class Scenario303( TestBase ):
    " Scenario 303 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 any_aff = 1 exclusive_new_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario303' )
        ns_client.set_client_identification( 'node', 'session' )

        try:
            execAny( ns_client,
                     'GET2 wnode_aff=1 any_aff=1 exclusive_new_aff=1' )
        except Exception, exc:
            if "forbidden" in str( exc ):
                return True
            raise
        raise Exception( "Expected exception, got nothing" )

class Scenario304( TestBase ):
    " Scenario 304 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT without aff, CHAFF as identified (add a0, a1, a2), " \
               "GET2 wnode_aff = 1 exclusive_new_aff=1"

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario304' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [ 'a3', 'a4', 'a5' ] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (it's in the blacklist), "
                             "but received one: " + output )
        return True

class Scenario305( TestBase ):
    " Scenario 305 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a7, CHAFF as identified (add a0, a1, a2), " \
               "GET2 wnode_aff = 1 exclusive_new_aff=1"

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a7' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario305' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [ 'a3', 'a4', 'a5' ] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (it's in the blacklist), "
                             "but received one: " + output )
        return True

class Scenario306( TestBase ):
    " Scenario 306 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a7, CHAFF as identified (add a0, a1, a2), " \
               "GET2 wnode_aff = 1 exclusive_new_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a7' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario306' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [] )

        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario306' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a7' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (non-unique affinity), "
                             "but received one: " + output )

        changeAffinity( ns_client2, [], [ 'a7' ] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (it's in the blacklist), "
                             "but received one: " + output )
        return True

class Scenario307( TestBase ):
    " Scenario 307 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications and blacklists"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario307' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " +
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        # Here: pending job and it is in the client black list

        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario307' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=1 exclusive_new_aff=0' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        # The first client waits for a job
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [ 'a0' ], False, False,
                                         'node', 'session' )

        # Sometimes it takes so long to spawn grid_cli that the next
        # command is sent before GET2 is sent. So, we have a sleep here
        time.sleep( 2 )

        # Return the job
        execAny( ns_client2, 'RETURN2 ' + jobID + ' ' + passport )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()   # analysis:ignore

        if "NCBI_JSQ_TEST" in processStdout:
            raise Exception( "Expect no notifications but received one" )
        return True

class Scenario308( TestBase ):
    " Scenario 308 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications and exclusive affinities"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )

        # First client holds a0 affinity
        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario308' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client1, [ 'a0' ], [] )

        # Second client holds a100 affinity
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario308' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a100' ], [] )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 9007 ) )

        # Second client tries to get the pending job - should get nothing
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=9007 timeout=3' )
        if output != "":
            raise Exception( "Expect no jobs, received: " + output )

        time.sleep( 4 )
        try:
            # Exception is expected
            data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
            raise Exception( "Expected no notifications, received one: " +
                             data )
        except Exception, exc:
            if "Resource temporarily unavailable" not in str( exc ):
                raise

        # Second client tries to get another pending job
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=9007 timeout=3' )

        # Should get notifications after this submit
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a5' )    # analysis:ignore

        time.sleep( 4 )
        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )

        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " +
                             data )

        return True


class Scenario309( TestBase ):
    " Scenario 309 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications and exclusive affinities"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )

        # First client holds a0 affinity
        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario309' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client1, [ 'a0' ], [] )

        output = execAny( ns_client1,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]      # analysis:ignore

        if jobID != receivedJobID:
            raise Exception( "Unexpected received job ID" )


        # Second client holds a100 affinity
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario309' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a100' ], [] )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 9007 ) )

        # Second client tries to get the pending job - should get nothing
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=9007 timeout=3' )
        if output != "":
            raise Exception( "Expect no jobs, received: " + output )

        time.sleep( 4 )
        try:
            # Exception is expected
            data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
            raise Exception( "Expected no notifications, received one: " +
                             data )
        except Exception, exc:
            if "Resource temporarily unavailable" not in str( exc ):
                raise

        # Second client tries to get another pending job
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 '
                          'port=9007 timeout=3' )

        # Should get notifications after this clear because
        # the a0 affinity becomes available
        execAny( ns_client1, "CLRN" )

        time.sleep( 4 )
        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )

        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " +
                             data )

        return True


class Scenario310( TestBase ):
    " Scenario 310 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

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
        changeAffinity( ns_client, [ 'a0' ], [] )

        # Worker node timeout is 5 sec
        time.sleep( 7 )

        ns_admin = self.getNetScheduleService( 'TEST', 'scenario310' )
        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'preferred_affinities_reset' ] != True:
            raise Exception( "Expected to have preferred affinities reset, "
                             "received: " +
                             str( info[ 'preferred_affinities_reset' ] ) )
        return True

class Scenario311( TestBase ):
    " Scenario 311 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Reset client affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # First client holds a0 affinity
        ns_client = self.getNetScheduleService( 'TEST', 'scenario311' )
        ns_client.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client, [ 'a0' ], [] )

        ns_admin = self.getNetScheduleService( 'TEST', 'scenario310' )

        affInfo = getAffinityInfo( ns_admin )
        if affInfo[ 'affinity_token' ] != 'a0' or \
           affInfo[ 'clients__preferred' ] != [ 'node1' ]:
            raise Exception( "Unexpected affinity registry content "
                             "after adding 1 preferred affinity" )
        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'preferred_affinities_reset' ] != False:
            raise Exception( "Expected to have preferred affinities non reset, "
                             "received: " +
                             str( info[ 'preferred_affinities_reset' ] ) )

        # Worker node timeout is 5 sec
        time.sleep( 7 )

        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'preferred_affinities_reset' ] != True:
            raise Exception( "Expected to have preferred affinities reset, "
                             "received: " +
                             str( info[ 'preferred_affinities_reset' ] ) )

        affInfo = getAffinityInfo( ns_admin )
        if affInfo[ 'affinity_token' ] != 'a0' or \
           affInfo[ 'clients__preferred' ] != None:
            raise Exception( "Unexpected affinity registry content "
                             "after worker node is expired" )

        try:
            output = execAny( ns_client,
                              'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        except Exception, excpt:
            if "ePrefAffExpired" in str( excpt ):
                return True
            raise

        raise Exception( "Expected exception in GET2 and did not get it: " +
                         output )


class Scenario312( TestBase ):
    " Scenario 312 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Reset client affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # First client holds a0 affinity
        ns_client = self.getNetScheduleService( 'TEST', 'scenario312' )
        ns_client.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client, [ 'a0' ], [] )

        ns_admin = self.getNetScheduleService( 'TEST', 'scenario312' )

        execAny( ns_client,
                 'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=0 '
                 'port=9007 timeout=3000' )

        info = getNotificationInfo( ns_admin )
        if info[ 'client_node' ] != 'node1':
            raise Exception( "Unexpected client in the notifications list: " +
                             info[ 'client_node' ] )

        # Worker node timeout is 5 sec
        time.sleep( 7 )

        info = getClientInfo( ns_admin, 'node1' )
        if info[ 'preferred_affinities_reset' ] != True:
            raise Exception( "Expected to have preferred affinities reset, "
                             "received: " +
                             str( info[ 'preferred_affinities_reset' ] ) )

        affInfo = getAffinityInfo( ns_admin )
        if affInfo[ 'affinity_token' ] != 'a0' or \
           affInfo[ 'clients__preferred' ] != None:
            raise Exception( "Unexpected affinity registry content "
                             "after worker node is expired" )

        if getNotificationInfo( ns_admin, True, 0 ) != None:
            raise Exception( "Expected no notification, got some" )

        return True

class Scenario313( TestBase ):
    " Scenario 313 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Transit client data"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.connect( 10 )
        self.ns.directLogin( 'TEST',
                             'netschedule_admin client_node=n1 '
                             'client_session=s1 control_port=732 '
                             'client_host=myhost' )
        self.ns.directSendCmd( 'AFLS' )
        reply = self.ns.directReadSingleReply()
        self.ns.disconnect()

        ns_admin = self.getNetScheduleService( 'TEST', 'scenario313' )
        info = getClientInfo( ns_admin, 'n1' )
        if info[ 'worker_node_control_port' ] != 732 or \
           info[ 'client_host' ] != 'myhost':
            raise Exception( "Unexpected client control "
                             "port and/or client host" )

        # Second connect to remove control port and host
        self.ns.connect( 10 )
        self.ns.directLogin( 'TEST',
                             'netschedule_admin client_node=n1 '
                             'client_session=s1' )
        self.ns.directSendCmd( 'AFLS' )
        reply = self.ns.directReadSingleReply()     # analysis:ignore
        self.ns.disconnect()
        info = getClientInfo( ns_admin, 'n1' )
        if info[ 'worker_node_control_port' ] != 'n/a' or \
           info[ 'client_host' ] != 'n/a':
            raise Exception( "Unexpected cleared client control "
                             "port and/or client host" )
        return True
