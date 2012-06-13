#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.13.0
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



class Scenario400( TestBase ):
    " Scenario 400 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 for outdated job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 4 )

        # Client #2 plays a passive role of holding an affinity (a2)
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario400' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a2' ], [] )
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a2' )


        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario400' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client1, [ 'a1' ], [] )

        output = execAny( ns_client1,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if output != '':
            raise Exception( "Expected no job, received: '" + output + "'" )

        # 10 seconds till the job becomes obsolete
        time.sleep( 12 )

        output = execAny( ns_client1,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1' )
        if "job_key=" + jobID not in output:
            raise Exception( "Expected a job, received: '" + output + "'" )

        return True

class Scenario401( TestBase ):
    " Scenario 401 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Notifications for outdated job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 4 )

        # Client #2 plays a passive role of holding an affinity (a2)
        ns_client2 = self.getNetScheduleService( 'TEST', 'scenario401' )
        ns_client2.set_client_identification( 'node2', 'session2' )
        changeAffinity( ns_client2, [ 'a2' ], [] )
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a2' )


        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario400' )
        ns_client1.set_client_identification( 'node1', 'session1' )
        changeAffinity( ns_client1, [ 'a1' ], [] )

        # Socket to receive notifications
        notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
        notifSocket.bind( ( "", 9007 ) )

        # Second client tries to get the pending job - should get nothing
        output = execAny( ns_client1,
                          'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 port=9007 timeout=15' )
        if output != "":
            raise Exception( "Expect no jobs, received: " + output )

        # 10 seconds till the job becomes outdated
        time.sleep( 12 )

        data = notifSocket.recv( 8192, socket.MSG_DONTWAIT )
        if "queue=TEST" not in data:
            raise Exception( "Expected notification, received garbage: " + data )
        return True
