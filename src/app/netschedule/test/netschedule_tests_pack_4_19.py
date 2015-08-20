#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.23.0 and up
"""

from netschedule_tests_pack import TestBase
from netschedule_tests_pack_4_10 import execAny

from cgi import parse_qs
import socket
import time


class Scenario1700( TestBase ):
    " Scenario 1700 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT 1 job with group=222, " \
               "GET2 group = 111,333 -> GET2 group=111,222"

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        jobID1 = self.ns.submitJob(  'TEST', 'bla', '', '222', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1700' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1 group=111,333' )
        if output != "":
            raise Exception( "Expected no job, received some: " + output )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1 group=111,222' )
        if output == "":
            raise Exception( "Expected a job, got nothing" )

        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        if receivedJobID != jobID1:
            raise Exception( "Expected: " + jobID1 + ", got: " + output )
        return True

class Scenario1701( TestBase ):
    " Scenario 1701 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT 1 job with group=222, PUT the results, " \
               "READ2 group = 111,333 -> READ2 group=111,222"

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        jobID1 = self.ns.submitJob(  'TEST', 'bla', '', '222', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1701' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        j1 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', j1[ 0 ], j1[ 1 ], 0 )

        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1 group=111,333' )
        if output != "no_more_jobs=true":
            raise Exception( "Expected no job, received some: " + output )

        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1 group=111,222' )
        if output == "":
            raise Exception( "Expected a job, got nothing" )

        values = parse_qs( output, True, True )
        receivedJobID = values[ 'job_key' ][ 0 ]
        if receivedJobID != jobID1:
            raise Exception( "Expected: " + jobID1 + ", got: " + output )
        return True


class Scenario1702( TestBase ):
    " Scenario 1702 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 notifications with group list"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1702' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'GET2 wnode_aff=0 any_aff=1 port=' +
                 str( notifPort ) + ' timeout=10 group=111,333' )
        # Submit a job
        self.ns.submitJob(  'TEST', 'bla', '', '222', '', '' )
        time.sleep( 1 )
        result = self.getNotif( notifSocket )
        if result != 0:
            raise Exception( "Expected no notifications, received some" )

        self.ns.submitJob(  'TEST', 'bla', '', '333', '', '' )
        time.sleep( 1 )
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


class Scenario1703( TestBase ):
    " Scenario 1703 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "READ2 notifications with group list"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1703' )
        ns_client.set_client_identification( 'node', 'session' )

        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 0 ) )
        notifPort = notifSocket.getsockname()[ 1 ]

        execAny( ns_client,
                 'READ2 reader_aff=0 any_aff=1 port=' +
                 str( notifPort ) + ' timeout=10 group=111,333' )
        # Submit a job
        self.ns.submitJob(  'TEST', 'bla', '', '222', '', '' )
        j1 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', j1[ 0 ], j1[ 1 ], 0 )
        time.sleep( 1 )
        result = self.getNotif( notifSocket )
        if result != 0:
            raise Exception( "Expected no notifications, received some" )

        self.ns.submitJob(  'TEST', 'bla', '', '333', '', '' )
        j2 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', j2[ 0 ], j2[ 1 ], 0 )
        time.sleep( 1 )
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
