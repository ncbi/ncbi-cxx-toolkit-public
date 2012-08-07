#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.14.0
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


class Scenario500( TestBase ):
    " Scenario 500 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Main job status changes loop and notifications"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario500' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 20")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        self.ns.returnJob( 'TEST', jobID, authToken )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node2", "session2" )
        self.ns.putJob( 'TEST', jobID, authToken, 0, out = "my output" )
        jobID, state, passport = self.ns.getJobsForReading2( 'TEST', -1, "",
                                                           'node3', 'session3' )
        self.ns.rollbackRead2( 'TEST', jobID, passport, 'node4', 'session4' )
        jobID, state, passport = self.ns.getJobsForReading2( 'TEST', -1, "",
                                                           'node5', 'session5' )
        self.ns.confirmRead2( 'TEST', jobID, passport, 'node6', 'session6' )
        self.ns.cancelJob( 'TEST', jobID, 'node7', 'session7' )

        # Receive from the socket
        time.sleep( 5 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )
        if "job_status=Running&last_event_index=3" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=3" )
        if "job_status=Done&last_event_index=4" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=4" )
        if "job_status=Reading&last_event_index=5" not in notifications:
            raise Exception( "Did not receive job_status=Reading&last_event_index=5" )
        if "job_status=Done&last_event_index=6" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=6" )
        if "job_status=Reading&last_event_index=7" not in notifications:
            raise Exception( "Did not receive job_status=Reading&last_event_index=7" )
        if "job_status=Confirmed&last_event_index=8" not in notifications:
            raise Exception( "Did not receive job_status=Confirmed&last_event_index=8" )
        if "job_status=Canceled&last_event_index=9" not in notifications:
            raise Exception( "Did not receive job_status=Canceled&last_event_index=9" )

        return True


class Scenario501( TestBase ):
    " Scenario 501 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the timed out job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario501' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        time.sleep( 15 )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )

        return True


class Scenario502( TestBase ):
    " Scenario 502 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the read-timed out job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario502' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        self.ns.returnJob( 'TEST', jobID, authToken )
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node2", "session2" )
        self.ns.putJob( 'TEST', jobID, authToken, 0, out = "my output" )
        jobID, state, passport = self.ns.getJobsForReading2( 'TEST', -1, "",
                                                           'node3', 'session3' )
        time.sleep( 15 )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )
        if "job_status=Running&last_event_index=3" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=3" )
        if "job_status=Done&last_event_index=4" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=4" )
        if "job_status=Reading&last_event_index=5" not in notifications:
            raise Exception( "Did not receive job_status=Reading&last_event_index=5" )
        if "job_status=Done&last_event_index=6" not in notifications:
            raise Exception( "Did not receive job_status=Done&last_event_index=6" )

        return True


class Scenario503( TestBase ):
    " Scenario 503 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the deleted job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario503' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        # Wait till the job is deleted by GC
        time.sleep( 35 )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Deleted&last_event_index=0" not in notifications:
            raise Exception( "Did not receive job_status=Deleted&last_event_index=0" )

        return True


class Scenario504( TestBase ):
    " Scenario 504 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for the job which was moved due to session reset"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )

        # Start listening
        port = self.ns.getPort() + 2 
        sock = socket.socket( socket.AF_INET, # Internet
                              socket.SOCK_DGRAM ) # UDP
        sock.bind( ( "" , port ) )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario504' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'LISTEN ' + jobID + " " + str( port ) + " 30")

        # Main job loop
        jobID, authToken, attrs, jobInput = self.ns.getJob( "TEST", -1, '', '',
                                                           "node1", "session1" )
        # Imitate a new session of the worker node
        self.ns.getJobStatus( 'TEST', jobID, 'node1', 'new_session' )

        # Receive from the socket
        time.sleep( 3 )
        notifications = ""
        while True:
            try:
                data = sock.recv( 16384, socket.MSG_DONTWAIT )
                if not data:
                    break
                if len( notifications ) != 0:
                    notifications += "\n"
                notifications += data
            except:
                break

        if "job_status=Running&last_event_index=1" not in notifications:
            raise Exception( "Did not receive job_status=Running&last_event_index=1" )
        if "job_status=Pending&last_event_index=2" not in notifications:
            raise Exception( "Did not receive job_status=Pending&last_event_index=2" )

        return True
