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


class Scenario1704( TestBase ):
    " Scenario 1704 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Case insensitive queue name submit"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # Submit a couple of jobs
        self.ns.submitJob( 'TEST', 'bla', '', '', '', '' )
        self.ns.submitJob( 'test', 'bla', '', '', '', '' )
        j1 = self.ns.getJob( 'TeSt' )
        self.ns.putJob( 'tEsT', j1[ 0 ], j1[ 1 ], 0 )
        j2 = self.ns.getJob( 'TeSt' )
        self.ns.putJob( 'tEsT', j2[ 0 ], j2[ 1 ], 0 )
        return True



class Scenario1800( TestBase ):
    " Scenario 1800 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit without a scope; GET2 with a scope; GET2 without a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', '', '', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1800' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        if output != "":
            raise Exception( "Expected no job, received some: " + output )

        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        if output == "":
            raise Exception( "Expected a job, got nothing" )
        return True


class Scenario1801( TestBase ):
    " Scenario 1801 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit with a scope; GET2 without a scope; GET2 with a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1801' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        execAny( ns_client, 'SUBMIT blah' )
        execAny( ns_client, 'SETSCOPE scope=' )

        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        if output != "":
            raise Exception( "Expected no job, received some: " + output )

        execAny( ns_client, 'SETSCOPE scope=MyWrongScope' )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        if output != "":
            raise Exception( "Expected no job, received some: " + output )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        if output == "":
            raise Exception( "Expected a job, got nothing" )
        return True


class Scenario1802( TestBase ):
    " Scenario 1802 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit without a scope; READ2 with a scope; READ2 without a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', '', '', '', '' )
        j = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', j[ 0 ], j[ 1 ], 0 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1802' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        if output != "no_more_jobs=true":
            raise Exception( "Expected no job, received some: " + output )

        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        if output == "":
            raise Exception( "Expected a job, got nothing" )
        return True


class Scenario1803( TestBase ):
    " Scenario 1803 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit with a scope; READ2 without a scope; READ2 with a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1800' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        execAny( ns_client, 'SUBMIT blah' )
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1' )
        if output == "":
            raise Exception( "Expected a job, got nothing" )
        values = parse_qs( output, True, True )
        jobID = values[ 'job_key' ][ 0 ]
        execAny( ns_client, 'PUT ' + jobID + ' 0 0' )

        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        if output != "no_more_jobs=true":
            raise Exception( "Expected no job, received some: " + output )

        execAny( ns_client, 'SETSCOPE scope=MyWrongScope' )
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        if output != "no_more_jobs=true":
            raise Exception( "Expected no job, received some: " + output )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1' )
        if output == "" or "no_more_jobs" in output:
            raise Exception( "Expected a job, got nothing" )
        return True


class Scenario1804( TestBase ):
    " Scenario 1804 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = None

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit without a scope; CANCEL with a scope; CANCEL without a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        j = self.ns.submitJob( 'TEST', 'bla', '', '', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1804' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'CANCEL ' + j )
        if output != "0" or "eJobNotFound" not in self.warning:
            raise Exception( "Expected no job to cancel, canceled some: " + str( output ) )

        self.warning = None
        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'CANCEL ' + j )
        if output != "1" or self.warning is not None:
            raise Exception( "Expected to cancel a job, could not do it" )
        return True


class Scenario1805( TestBase ):
    " Scenario 1805 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = None

    def report_warning( self, msg, server ):
        " Callback to report a warning "
        self.warning = msg

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit with a scope; CANCEL without a scope; CANCEL with a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1805' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        j = execAny( ns_client, 'SUBMIT blah' )
        execAny( ns_client, 'SETSCOPE scope=' )

        output = execAny( ns_client, 'CANCEL ' + j )
        if output != "0" or "eJobNotFound" not in self.warning:
            raise Exception( "Expected no job to cancel, canceled some: " + str( output ) )
        self.warning = None

        execAny( ns_client, 'SETSCOPE scope=MyWrongScope' )
        output = execAny( ns_client, 'CANCEL ' + j )
        if output != "0" or "eJobNotFound" not in self.warning:
            raise Exception( "Expected no job to cancel, canceled some: " + str( output ) )
        self.warning = None

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'CANCEL ' + j )
        if output != "1" or self.warning is not None:
            raise Exception( "Expected to cancel a job, could not do it" )
        return True




class Scenario1806( TestBase ):
    " Scenario 1806 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit without a scope + aff; STAT AFFINITIES with a scope; STAT AFFINITIES without a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', 'a1', '', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1806' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'STAT AFFINITIES verbose=1', isMultiline =True )
        if output != ['']:
            raise Exception( "Expected no affinities, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'STAT AFFINITIES verbose=1', isMultiline =True )
        if "AFFINITY: 'a1'" not in "\n".join( output ):
            raise Exception( "Expected an affinity, got nothing: " + str(output) )
        return True



class Scenario1807( TestBase ):
    " Scenario 1807 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit with a scope + aff; STAT AFFINITIES without a scope; STAT AFFINITIES with a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1807' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        execAny( ns_client, 'SUBMIT blah aff=a1' )
        execAny( ns_client, 'SETSCOPE scope=no-scope-only' )

        output = execAny( ns_client, 'STAT AFFINITIES verbose=1', isMultiline =True )
        if output != ['']:
            raise Exception( "Expected no affinities, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyWrongScope' )
        output = execAny( ns_client, 'STAT AFFINITIES verbose=1', isMultiline =True )
        if output != ['']:
            raise Exception( "Expected no affinities, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'STAT AFFINITIES verbose=1', isMultiline =True )
        if "AFFINITY: 'a1'" not in "\n".join( output ):
            raise Exception( "Expected an affinity, got nothing: " + str(output) )
        return True


class Scenario1808( TestBase ):
    " Scenario 1808 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit without a scope + group; STAT GROUPS with a scope; STAT GROUPS without a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', '', 'g1', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1808' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'STAT GROUPS verbose=1', isMultiline =True )
        if output != []:
            raise Exception( "Expected no groups, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'STAT GROUPS verbose=1', isMultiline =True )
        if "GROUP: 'g1'" not in "\n".join( output ):
            raise Exception( "Expected a group, got nothing: " + str(output) )
        return True



class Scenario1809( TestBase ):
    " Scenario 1809 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit with a scope + group; STAT GROUPS without a scope; STAT GROUPS with a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1809' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        execAny( ns_client, 'SUBMIT blah group=g1' )
        execAny( ns_client, 'SETSCOPE scope=no-scope-only' )

        output = execAny( ns_client, 'STAT GROUPS verbose=1', isMultiline =True )
        if output != []:
            raise Exception( "Expected no groups, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyWrongScope' )
        output = execAny( ns_client, 'STAT GROUPS verbose=1', isMultiline =True )
        if output != []:
            raise Exception( "Expected no groups, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'STAT GROUPS verbose=1', isMultiline =True )
        if "GROUP: 'g1'" not in "\n".join( output ):
            raise Exception( "Expected a group, got nothing: " + str(output) )
        return True


class Scenario1810( TestBase ):
    " Scenario 1810 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit without a scope; STAT JOBS with a scope; STAT JOBS without a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', '', '', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1810' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'STAT JOBS', isMultiline =True )
        if 'Pending: 0' not in "\n".join( output ):
            raise Exception( "Expected no jobs, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'STAT JOBS', isMultiline =True )
        if 'Pending: 1' not in "\n".join( output ):
            raise Exception( "Expected a job, got nothing: " + str(output) )
        return True


class Scenario1811( TestBase ):
    " Scenario 1811 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit with a scope; STAT JOBS without a scope; STAT JOBS with a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1811' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        execAny( ns_client, 'SUBMIT blah' )
        execAny( ns_client, 'SETSCOPE scope=no-scope-only' )

        output = execAny( ns_client, 'STAT JOBS', isMultiline =True )
        if 'Pending: 0' not in "\n".join( output ):
            raise Exception( "Expected no jobs, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyWrongScope' )
        output = execAny( ns_client, 'STAT JOBS', isMultiline =True )
        if 'Pending: 0' not in "\n".join( output ):
            raise Exception( "Expected no jobs, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'STAT JOBS', isMultiline =True )
        if 'Pending: 1' not in "\n".join( output ):
            raise Exception( "Expected a job, got nothing: " + str(output) )
        return True



class Scenario1812( TestBase ):
    " Scenario 1812 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit without a scope; DUMP with a scope; DUMP without a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', '', '', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1812' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'DUMP', isMultiline =True )
        if output != []:
            raise Exception( "Expected no jobs, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=' )
        output = execAny( ns_client, 'DUMP', isMultiline =True )
        if 'status: Pending' not in "\n".join( output ):
            raise Exception( "Expected a job, got nothing: " + str(output) )
        return True


class Scenario1813( TestBase ):
    " Scenario 1813 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Submit with a scope; DUMP without a scope; DUMP with a scope"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1813' )
        ns_client.set_client_identification( 'node', 'session' )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        execAny( ns_client, 'SUBMIT blah' )
        execAny( ns_client, 'SETSCOPE scope=no-scope-only' )

        output = execAny( ns_client, 'DUMP', isMultiline =True )
        if output != []:
            raise Exception( "Expected no jobs, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyWrongScope' )
        output = execAny( ns_client, 'DUMP', isMultiline =True )
        if output != []:
            raise Exception( "Expected no jobs, received some: " + str(output) )

        execAny( ns_client, 'SETSCOPE scope=MyScope' )
        output = execAny( ns_client, 'DUMP', isMultiline =True )
        if 'status: Pending' not in "\n".join( output ):
            raise Exception( "Expected a job, got nothing: " + str(output) )
        return True



class Scenario1900( TestBase ):
    " Scenario 1900 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a2, SUBMIT with a1 " \
               "GET2 a=a1,a2 wnode_aff=1 prioritized_aff=1 => ERR, " \
               "GET2 a=a3,a4 any_aff=1 prioritized_aff=1 => OK, " \
               "GET2 a=a1,a2 exclusive_new_aff=1 prioritized_aff=1 => ERR"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', affinity='a2' )
        self.ns.submitJob( 'TEST', 'bla', affinity='a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1603' )
        ns_client.set_client_identification( 'node', 'session' )

        ex = False
        try:
            execAny( ns_client, 'GET2 wnode_aff=1 any_aff=0 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none  (case 1)" )

        # 4.27.0 and up should provide a job
        output = execAny( ns_client, 'GET2 wnode_aff=0 any_aff=1 '
                                     'aff=a3,a4 prioritized_aff=1' )
        if 'job_key=' not in output:
            raise Exception( "Expected a job with affinity a2, received no job: '" + output + "'" )

        ex = False
        try:
            execAny( ns_client, 'GET2 wnode_aff=0 any_aff=0 exclusive_new_aff=1 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none  (case 3)" )

        return True


class Scenario1901( TestBase ):
    " Scenario 1901 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a2, SUBMIT with a1 " \
               "GET + PUT" \
               "READ2 a=a1,a2 reader_aff=1 prioritized_aff=1 => ERR, " \
               "READ2 a=a3,a4 any_aff=1 prioritized_aff=1 => OK, " \
               "READ2 a=a1,a2 exclusive_new_aff=1 prioritized_aff=1 => ERR"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', affinity='a2' )
        self.ns.submitJob( 'TEST', 'bla', affinity='a1' )

        j = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', j[ 0 ], j[ 1 ], 0 )
        j = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', j[ 0 ], j[ 1 ], 0 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario1606' )
        ns_client.set_client_identification( 'node', 'session' )

        ex = False
        try:
            execAny( ns_client, 'READ2 reader_aff=1 any_aff=0 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none (case 1)" )

        # 4.27.0 and up should provide a job
        output = execAny( ns_client, 'READ2 reader_aff=0 any_aff=1 '
                                     'aff=a3,a4 prioritized_aff=1' )
        if 'job_key=' not in output:
            raise Exception( "Expected a job with affinity a2, received no job: '" + output + "'" )

        ex = False
        try:
            execAny( ns_client, 'READ2 reader_aff=0 any_aff=0 exclusive_new_aff=1 '
                                'aff=a1,a2 prioritized_aff=1' )
        #except Exception, exc:
        except Exception:
            ex = True
            #print str( exc )

        if ex == False:
            raise Exception( "Expected exception, got none (case 3)" )

        return True
