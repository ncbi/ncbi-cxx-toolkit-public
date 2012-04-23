#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.11.0
"""

import time
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

