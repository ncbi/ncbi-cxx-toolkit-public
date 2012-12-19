#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.16.0
"""

import time, os
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
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a0' )

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

