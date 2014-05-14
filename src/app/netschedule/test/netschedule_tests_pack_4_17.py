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
from netschedule_tests_pack_4_10 import getClientInfo, changeAffinity, execAny

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
