#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.10.0
"""

from netschedule_tests_pack import TestBase



class Scenario100( TestBase ):
    " Scenario 100 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Get the NS version in a new format "
        return "Get the NS version in a new format"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        result = self.ns.getVersion()
        if "server_version=" not in result:
            raise Exception( "No 'server_version' in VERSION output" )
        if "storage_version=" not in result:
            raise Exception( "No 'storage_version' in VERSION output" )
        if "protocol_version=" not in result:
            raise Exception( "No 'protocol_version' in VERSION output" )
        if "ns_node=" not in result:
            raise Exception( "No 'ns_node=' in VERSION output" )
        if "ns_session=" not in result:
            raise Exception( "No ns_session=' in VERSION output" )
        return True


class Scenario101( TestBase ):
    " Scenario 101 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Check regenerated ns_session after NS restart "
        return "Get the NS version, restart NS, get NS version; " \
               "Make sure that the session is regenerated"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        result = self.ns.getVersion()

        # Extract ns_session
        firstSession = "1"

        self.fromScratch()
        result = self.ns.getVersion()

        # Extract ns_session
        secondSession = "2"

        if firstSession == secondSession:
            raise Exception( "NS did not regenerate its session after restart" )
        return True



