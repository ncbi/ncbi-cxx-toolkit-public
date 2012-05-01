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
from netschedule_tests_pack_4_10 import getClientInfo, NON_EXISTED_JOB, \
                                        getAffinityInfo, getNotificationInfo, \
                                        getGroupInfo, changeAffinity, \
                                        execAny
import grid_v01
import ncbi.grid.ns as grid

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

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario300' )

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
            raise Exception( "WST2 changes the job expiration while it must not" )

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

        jobID = self.ns.submitJob(  'TEST', 'bla', '', '', '', '' )

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario301' )

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
            raise Exception( "SST2 does not change the job expiration while it must" )

        return True

class Scenario302( TestBase ):
    " Scenario 302 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 wnode_aff = 0 exclusive_new_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario302' )
        ns_client.set_client_identification( 'node', 'session' )

        try:
            output = execAny( ns_client,
                              'GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=1' )
        except Exception, exc:
            if "forbidden" in str( exc ):
                return True
            raise
        raise Exception( "Expected exception, got nothing" )

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

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario303' )
        ns_client.set_client_identification( 'node', 'session' )

        try:
            output = execAny( ns_client,
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

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario304' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [ 'a3', 'a4', 'a5' ] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (it's in the blacklist), " \
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

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a7' )

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario305' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [ 'a3', 'a4', 'a5' ] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (it's in the blacklist), " \
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

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario306' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [] )

        ns_client2 = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                              str( self.ns.getPort() ),
                                              'TEST', 'scenario306' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a7' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (non-unique affinity), " \
                             "but received one: " + output )

        changeAffinity( ns_client2, [], [ 'a7' ] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != "":
            raise Exception( "Expect no job (it's in the blacklist), " \
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

        ns_client = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                             str( self.ns.getPort() ),
                                             'TEST', 'scenario307' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )
        # Here: pending job and it is in the client black list

        ns_client2 = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                              str( self.ns.getPort() ),
                                              'TEST', 'scenario307' )
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

        # Return the job
        execAny( ns_client2, 'RETURN2 ' + jobID + ' ' + passport )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

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
        ns_client1 = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                              str( self.ns.getPort() ),
                                              'TEST', 'scenario308' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client1, [ 'a0' ], [] )

        # Second client holds a100 affinity
        ns_client2 = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                              str( self.ns.getPort() ),
                                              'TEST', 'scenario308' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a100' ], [] )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 9007 ) )

        # Second client tries to get the pending job - should get nothing
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 port=9007 timeout=3' )
        if output != "":
            raise Exception( "Expect no jobs, received: " + output )

        time.sleep( 4 )
        try:
            # Exception is expected
            data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
            raise Exception( "Expected no notifications, received one: " + data )
        except Exception, exc:
            if "Resource temporarily unavailable" not in str( exc ):
                raise

        # Second client tries to get another pending job
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 port=9007 timeout=3' )

        # Should get notifications after this submit
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a5' )

        time.sleep( 4 )
        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )

        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " + data )

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
        ns_client1 = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                              str( self.ns.getPort() ),
                                              'TEST', 'scenario309' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client1, [ 'a0' ], [] )

        output = execAny( ns_client1,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        passport = values[ 'auth_token' ][ 0 ]

        if jobID != receivedJobID:
            raise Exception( "Unexpected received job ID" )


        # Second client holds a100 affinity
        ns_client2 = grid.NetScheduleService( self.ns.getHost() + ":" + \
                                              str( self.ns.getPort() ),
                                              'TEST', 'scenario309' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a100' ], [] )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 9007 ) )

        # Second client tries to get the pending job - should get nothing
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 port=9007 timeout=3' )
        if output != "":
            raise Exception( "Expect no jobs, received: " + output )

        time.sleep( 4 )
        try:
            # Exception is expected
            data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
            raise Exception( "Expected no notifications, received one: " + data )
        except Exception, exc:
            if "Resource temporarily unavailable" not in str( exc ):
                raise

        # Second client tries to get another pending job
        output = execAny( ns_client2,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 port=9007 timeout=3' )

        # Should get notifications after this clear because
        # the a0 affinity becomes available
        execAny( ns_client1, "CLRN" )

        time.sleep( 4 )
        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )

        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " + data )

        return True
