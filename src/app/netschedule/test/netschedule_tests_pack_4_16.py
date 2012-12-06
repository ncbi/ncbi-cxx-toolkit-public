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
        if client[ 'blacklisted_jobs' ][ 0 ] != jobID:
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
        if client[ 'blacklisted_jobs' ][ 0 ] != jobID:
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


