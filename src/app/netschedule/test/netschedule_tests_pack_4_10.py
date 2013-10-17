#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack for the features appeared in NS-4.10.0
"""

import time, socket
from netschedule_tests_pack import TestBase
from ncbi_grid_1_0.ncbi.grid import ns as grid

# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs


NON_EXISTED_JOB = "JSID_01_777_130.14.24.83_9101"
ANY_AUTH_TOKEN = '1166018352_2'


def getClientInfo( ns, clientNode = None,
                    minClients = 1, maxClients = None, verbose = True ):
    " Provides the client info "
    servers = ns.get_servers()
    if len( servers ) != 1:
        raise Exception( "Invalid number of servers returned." )
    serverClients = servers[ 0 ].get_client_info( verbose )
    if minClients is not None and len( serverClients ) < minClients:
        raise Exception( "Too few clients returned (" +
                str( len( serverClients ) ) + "); minimum: " + \
                            str( minClients ) )
    if maxClients is not None and len( serverClients ) > maxClients:
        raise Exception( "Too many clients returned (" +
                str( len( serverClients ) ) + "); maximum: " + \
                            str( maxClients ) )
    if len( serverClients ) == 0 or not clientNode:
        return None

    for clientInfo in serverClients:
        if clientInfo[ 'client_node' ] == clientNode:
            return clientInfo

    raise Exception( "Unable to find client '" + clientNode + "'" )

def getAffinityInfo( ns, verbose = True, expectedAffinities = 1, affIndex = 0 ):
    " Provides the affinity info "
    servers = ns.get_servers()
    if len( servers ) != 1:
        raise Exception( "Invalid number of servers returned." )

    serverAffinities = servers[ 0 ].get_affinity_info( verbose )
    if len( serverAffinities ) != expectedAffinities:
        raise Exception( "Expected " + str( expectedAffinities ) + \
                         " affinity(s). Received " + \
                         str( len( serverAffinities ) ) + " affinity(s)" )
    if expectedAffinities <= 0:
        return None
    return serverAffinities[ affIndex ]

def getNotificationInfo( ns, verbose = True, expectedNotifications = 1,
                         notifIndex = 0 ):
    " Provides the notification info "
    servers = ns.get_servers()
    if len( servers ) != 1:
        raise Exception( "Invalid number of servers returned." )

    serverNotifications = servers[ 0 ].get_notification_info( verbose )
    if len( serverNotifications ) != expectedNotifications:
        raise Exception( "Expected " + str( expectedNotifications ) + \
                         " notification(s). Received " + \
                         str( len( serverNotifications ) ) + \
                         " notification(s)" )
    if expectedNotifications <= 0:
        return None
    return serverNotifications[ notifIndex ]

def getGroupInfo( ns, verbose = True, expectedGroups = 1,
                  groupIndex = 0 ):
    " Provides the group info "
    servers = ns.get_servers()
    if len( servers ) != 1:
        raise Exception( "Invalid number of servers returned." )

    serverGroups = servers[ 0 ].get_job_group_info( verbose )
    if len( serverGroups ) != expectedGroups:
        raise Exception( "Expected " + str( expectedGroups ) + \
                         " group(s). Received " + \
                         str( len( serverGroups ) ) + \
                         " group(s)" )
    if expectedGroups <= 0:
        return None
    return serverGroups[ groupIndex ]

def changeAffinity( ns, toAdd, toDel, serverIndex = 0 ):
    " Changes preferred affinities on the given server "
    servers = ns.get_servers()
    if len( servers ) == 0:
        raise Exception( "Cannot get any servers" )
    server = servers[ serverIndex ]
    server.change_preferred_affinities( toAdd, toDel )
    return

def execAny( ns, cmd, serverIndex = 0, isMultiline = False ):
    " Execute any command on the given server "
    servers = ns.get_servers()
    if len( servers ) == 0:
        raise Exception( "Cannot get any servers" )
    server = servers[ serverIndex ]
    return server.execute( cmd, isMultiline )



class Scenario100( TestBase ):
    " Scenario 100 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Get the NS version in a new format"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        result = self.ns.getVersion()
        if "server_version" not in result:
            raise Exception( "No 'server_version' in VERSION output" )
        if "storage_version" not in result:
            raise Exception( "No 'storage_version' in VERSION output" )
        if "protocol_version" not in result:
            raise Exception( "No 'protocol_version' in VERSION output" )
        if "ns_node" not in result:
            raise Exception( "No 'ns_node' in VERSION output" )
        if "ns_session" not in result:
            raise Exception( "No ns_session' in VERSION output" )
        return True

class Scenario101( TestBase ):
    " Scenario 101 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Get the NS version, restart NS, get NS version; " \
               "Make sure that the session is regenerated"

    def getSession( self, result ):
        " provides the session ID from the output "
        for line in result.split( "\n" ):
            if "ns_session" not in line:
                continue
            parts = line.split( "ns_session" )
            if len( parts ) != 2:
                raise Exception( "Cannot find ns_session" )
            part = parts[ 1 ].strip()
            if part.startswith( '=' ):
                part = part[ 1: ]
            if len( part ) == 0:
                raise Exception( "Unexpected ns_session format" )
            return part
        raise Exception( "No ns_session in the VERSION output" )


    def getNode( self, result ):
        " provides the node ID from the output "
        for line in result.split( "\n" ):
            if "ns_node" not in line:
                continue
            parts = line.split( "ns_node" )
            if len( parts ) != 2:
                raise Exception( "Cannot find ns_node" )
            part = parts[ 1 ].strip()
            if part.startswith( '=' ):
                part = part[ 1: ]
            part = part.split( '&' )[ 0 ]
            if len( part ) == 0:
                raise Exception( "Unexpected ns_node format" )
            return part
        raise Exception( "No ns_node in the VERSION output" )

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        result = self.ns.getVersion()

        # Extract ns_session
        firstSession = self.getSession( result )
        firstNode = self.getNode( result )

        self.ns.kill( "SIGKILL" )
        time.sleep( 1 )
        self.ns.start()
        time.sleep( 1 )

        result = self.ns.getVersion()

        # Extract ns_session
        secondSession = self.getSession( result )
        secondNode = self.getNode( result )

        if firstSession == secondSession:
            raise Exception( "NS did not regenerate its session after restart" )
        if firstNode != secondNode:
            raise Exception( "NS changes node ID after restart. Was: " + \
                             firstNode + " Became: " + secondNode )
        return True

class Scenario102( TestBase ):
    " Scenario 102 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Login with client_node and without client_session"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.getVersion( node = "my_node", session = "" )
        except Exception, exc:
            if "client_node is provided but " \
               "client_session is not" in str( exc ):
                return True
            raise

        return False

class Scenario103( TestBase ):
    " Scenario 103 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Login without client_node and with client_session"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.getVersion( node = "", session = "my_session" )
        except Exception, exc:
            if "client_session is provided " \
               "but client_node is not" in str( exc ):
                return True
            raise

        return False

class Scenario104( TestBase ):
    " Scenario 104 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Get empty list of registered clients"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario104' )
        getClientInfo( ns_client, None, 0, 0 )
        return True

class Scenario105( TestBase ):
    " Scenario 105 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "STAT CLIENTS on behalf an identified client"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario105' )
        ns_client.set_client_identification( 'mynode', 'mysession' )

        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'session' ] == 'mysession' and \
           client[ 'type' ] == 'unknown':
            return True

        raise Exception( "Unexpected client info: " + str( client ) )

class Scenario106( TestBase ):
    " Scenario 106 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT as identified, STAT CLIENTS and check the client type"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario105' )

        self.ns.submitJob( 'TEST', 'bla', '', '', 'node', '000' )

        client = getClientInfo( ns_client, 'node' )
        if client[ 'session' ] == '000' and \
           client[ 'type' ] == 'submitter':
            return True

        raise Exception( "Unexpected client info: " + str( client ) )

class Scenario107( TestBase ):
    " Scenario 107 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Get a job as identified, check the clients list"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.ns.getJob( 'TEST', -1, '', "",
                        'scenario107', 'default' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario107' )
        client = getClientInfo( ns_client, 'scenario107' )

        if client[ 'session' ] == 'default' and \
           client[ 'type' ] == 'worker node' and \
           len( client[ 'running_jobs' ] ) == 1 and \
           client[ 'running_jobs' ][ 0 ] == jobID:
            return True

        raise Exception( "Unexpected client info: " + str( client ) )

class Scenario108( TestBase ):
    " Scenario 108 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT as anon. READ as identified, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )

        key, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                           'mynode',
                                                           'mysession' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario108' )
        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'session' ] == 'mysession' and \
           client[ 'type' ] == 'reader' and \
           len( client[ 'reading_jobs' ] ) == 1 and \
           client[ 'reading_jobs' ][ 0 ] == key:
            return True

        raise Exception( "Unexpected client info: " + str( client ) )

class Scenario109( TestBase ):
    " Scenario 109 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ as identified, STAT CLIENTS VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', '0' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', '', 'mynode', '0' )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "", 'mynode', '0' )

        key, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                           'mynode',
                                                           '0' )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario109' )
        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'session' ] == '0' and \
           client[ 'type' ] == 'submitter | worker node | reader' and \
           len( client[ 'reading_jobs' ] ) == 1 and \
           client[ 'reading_jobs' ][ 0 ] == key:
            return True

        raise Exception( "Unexpected client info: " + str( client ) )

class Scenario110( TestBase ):
    " Scenario 110 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ as identified, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', '0' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', '', 'mynode', '0' )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "", 'mynode', '0' )

        key, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                           'mynode',
                                                           '0' )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario110' )
        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'session' ] == '0' and \
           client[ 'number_of_submitted_jobs' ] == 1 and \
           client[ 'number_of_jobs_given_for_execution' ] == 1 and \
           client[ 'number_of_jobs_given_for_reading' ] == 1 and \
           client[ 'type' ] == 'submitter | worker node | reader' and \
           client[ 'number_of_reading_jobs' ] == 1:
            return True

        raise Exception( "Unexpected client info: " + str( client ) )

class Scenario111( TestBase ):
    " Scenario 111 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Connect with one session, reconnect with another"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario111' )

        # get original session
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', '0' )
        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'session' ] != '0':
            raise Exception( "Unexpected session. Expected '0'. " \
                             "Received: '" + client[ 'session' ] )

        # get modified session session
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', '1' )
        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'session' ] != '1':
            raise Exception( "Unexpected session. Expected '1'. " \
                             "Received: '" + client[ 'session' ] )

        return True

class Scenario112( TestBase ):
    " Scenario 112 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Connect with a session, check clients list, " \
               "reconnect with the same session, check clients list, " \
               "issue CLRN, check clients list"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario112' )
        ns_client.set_client_identification( 'mynode', 'session1' )

        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'session' ] != 'session1':
            raise Exception( "Unexpected session. Expected 'session1'. " \
                             "Received: '" + client[ 'session' ] )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario112' )
        ns_client.set_client_identification( 'mynode', 'session1' )
        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'session' ] != 'session1':
            raise Exception( "Unexpected session. Expected 'session1'. " \
                             "Received: '" + client[ 'session' ] )

        # CLRN
        execAny( ns_client, 'CLRN' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario112' )
        info = getClientInfo( ns_client, 'mynode' )
        if info[ 'session' ] != 'n/a':
            raise Exception( "Unexpected session. Expected 'n/a', " \
                             "received: " + info[ 'session' ] )

        return True

class Scenario113( TestBase ):
    " Scenario 113 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Get a job with one session, reconnect with another session"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario113' )

        # get original session
        jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.ns.getJob( 'TEST', -1, '', '', 'mynode', 'mysession' )

        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'running_jobs' ][ 0 ] != jobID:
            raise Exception( "Running job is not registered" )

        # To touch the clients registry with another session
        self.ns.submitJob( 'TEST', 'bla2', '', '', 'mynode', 'changedsession' )
        client = getClientInfo( ns_client, 'mynode' )
        if client.has_key( 'running_jobs' ):
            raise Exception( "Running job is still there" )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Pending" or \
           status2 != "Pending" or \
           status3 != "Pending":
            return False
        return True

class Scenario114( TestBase ):
    " Scenario 114 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Get a job with one session, issue CLRN"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario114' )
        ns_client.set_client_identification( 'mynode', 'mysession' )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.ns.getJob( 'TEST', -1, '', '', 'mynode', 'mysession' )

        client = getClientInfo( ns_client, 'mynode' )
        if client[ 'running_jobs' ][ 0 ] != jobID:
            raise Exception( "Running job is not registered" )

        execAny( ns_client, 'CLRN' )
        client = getClientInfo( ns_client, 'mynode' )
        if client.has_key( 'running_jobs' ):
            raise Exception( "Running job is still there" )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Pending" or \
           status2 != "Pending" or \
           status3 != "Pending":
            return False
        return True

class Scenario115( TestBase ):
    " Scenario 115 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT as anon. READ as identified, STAT CLIENTS. " \
               "Reconnect with another session, check the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario115' )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )

        key, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                           'mynode',
                                                           'session1' )

        client = getClientInfo( ns_client, 'mynode' )
        if client.has_key( 'running_jobs' ):
            raise Exception( "Running job is still there" )
        if client[ 'number_of_jobs_given_for_reading' ] != 1:
            raise Exception( "Unexpected number of running jobs" )

        # Another sesson
        self.ns.getFastJobStatus( 'TEST', jobID, 'mynode', 'session2' )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Done" or \
           status2 != "Done" or \
           status3 != "Done":
            return False
        return True

class Scenario116( TestBase ):
    " Scenario 116 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT as anon. READ as identified, STAT CLIENTS. " \
               "CLRN, check the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario116' )
        ns_client.set_client_identification( 'mynode', 'session1' )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )

        key, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                           'mynode',
                                                           'session1' )

        client = getClientInfo( ns_client, 'mynode' )
        if not client.has_key( 'reading_jobs' ):
            raise Exception( "Reading job is not found" )
        if client[ 'number_of_jobs_given_for_reading' ] != 1:
            raise Exception( "Unexpected number of running jobs" )

        execAny( ns_client, 'CLRN' )

        client = getClientInfo( ns_client, 'mynode' )
        if client.has_key( 'reading_jobs' ):
            raise Exception( "Reading job is still there" )
        if client[ 'number_of_jobs_given_for_reading' ] != 1:
            raise Exception( "Unexpected number of running jobs" )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Done" or \
           status2 != "Done" or \
           status3 != "Done":
            return False
        return True

class Scenario117( TestBase ):
    " Scenario 117 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "Two records in the clients registry"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.getJob( 'TEST', -1, '', "", 'scenario117-1', 'default' )
        self.ns.getJob( 'TEST', -1, '', "", 'scenario117-2', 'default' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario117' )
        getClientInfo( ns_client, minClients = 2, maxClients = 2 )
        return True

class Scenario118( TestBase ):
    " Scenario 118 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as anon"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            ns_client = self.getNetScheduleService( 'TEST', 'scenario118' )
            changeAffinity( ns_client, [ 'a1' ], [] )
        except Exception, excp:
            if "cannot use CHAFF command" in str( excp ):
                return True
            raise
        return False

class Scenario119( TestBase ):
    " Scenario 119 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario119' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        changeAffinity( ns_client, [ 'a1', 'a2' ], [] )

        client = getClientInfo( ns_client, 'mynode', verbose = False )
        if client[ 'number_of_preferred_affinities' ] != 2:
            raise Exception( 'Unexpected length of preferred_affinities' )
        if client[ 'type' ] not in [ 'unknown', 'worker node' ]:
            raise Exception( 'Unexpected client type: ' + client[ 'type' ] )

        return True

class Scenario120( TestBase ):
    " Scenario 120 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified, STAT CLIENTS VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario120' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        changeAffinity( ns_client, [ 'a1', 'a2' ], [] )

        client = getClientInfo( ns_client, 'mynode' )
        if len( client[ 'preferred_affinities' ] ) != 2:
            raise Exception( 'Unexpected length of preferred_affinities' )
        if client[ 'preferred_affinities' ][ 0 ] != 'a1':
            raise Exception( 'Unexpected preferred_affinities[ 0 ]' )
        if client[ 'preferred_affinities' ][ 1 ] != 'a2':
            raise Exception( 'Unexpected preferred_affinities[ 1 ]' )
        if client[ 'type' ] not in [ 'unknown', 'worker node' ]:
            raise Exception( 'Unexpected client type: ' + client[ 'type' ] )

        return True

class Scenario121( TestBase ):
    " Scenario 121 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = ""
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified (rm), STAT CLIENTS"

    def report_warning( self, msg, server ):
        self.warning = msg
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario121' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning
        changeAffinity( ns_client, [], [ 'a1', 'a2' ] )

        getClientInfo( ns_client, 'node', 1, 1 )
        if "unknown affinity to delete" in self.warning:
            return True
        raise Exception( "The expected warning has not received" )

class Scenario122( TestBase ):
    " Scenario 122 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.warning = ""
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified (add, rm), STAT CLIENTS"

    def report_warning( self, msg, server ):
        self.warning = msg
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario122' )
        ns_client.set_client_identification( 'node', 'session' )
        ns_client.on_warning = self.report_warning

        changeAffinity( ns_client, [ 'a1', 'a2', 'a3' ], [] )
        changeAffinity( ns_client, [ 'a2', 'a4' ], [ 'a1' ] )
        client = getClientInfo( ns_client, 'node' )
        if "already registered affinity to add" not in self.warning:
            raise Exception( "The expected warning has not received" )

        if len( client[ 'preferred_affinities' ] ) != 3 or \
           'a2' not in client[ 'preferred_affinities' ] or \
           'a3' not in client[ 'preferred_affinities' ] or \
           'a4' not in client[ 'preferred_affinities' ]:
            raise Exception( "Unexpected preferred affinities" )
        return True

class Scenario123( TestBase ):
    " Scenario 123 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified (add), CLRN, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario123' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a1', 'a2' ], [] )

        execAny( ns_client, 'CLRN' )
        client = getClientInfo( ns_client, 'node' )
        if client.has_key( 'number_of_preferred_affinities' ):
            if client[ 'number_of_preferred_affinities' ] != 0:
                raise Exception( "Expected no preferred affinities, got some." )
        return True

class Scenario124( TestBase ):
    " Scenario 124 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified (add), connect with another session, " \
               "STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario124' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a1', 'a2' ], [] )

        self.ns.submitJob( 'TEST', 'bla', '', '', 'node', 'other_session' )

        client = getClientInfo( ns_client, 'node' )
        if client.has_key( 'number_of_preferred_affinities' ):
            if client[ 'number_of_preferred_affinities' ] != 0:
                raise Exception( "Expected no preferred affinities, got some." )
        return True

class Scenario125( TestBase ):
    " Scenario 125 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 as anon"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            ns_client = self.getNetScheduleService( 'TEST', 'scenario125' )
            output = execAny( ns_client,
                              'GET2 wnode_aff=1 any_aff=1' )
        except Exception, exc:
            if "Anonymous client" in str( exc ):
                return True
            raise
        return False

class Scenario126( TestBase ):
    " Scenario 126 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, CHAFF as identified (add a0, a1, a2), " \
               "GET2 wnode_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario126' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )
        return True

class Scenario127( TestBase ):
    " Scenario 127 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, CHAFF as identified (add a0, a2), " \
               "GET2 wnode_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario126' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0', 'a2' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0' )
        if output != "":
            raise Exception( "Expect no jobs, but received one." )
        return True

class Scenario128( TestBase ):
    " Scenario 128 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified (add a0, a1, a2), SUBMIT with a1, " \
               "GET2 wnode_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario126' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [] )
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )
        return True

class Scenario129( TestBase ):
    " Scenario 129 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF as identified (add a0, a1, a2), SUBMIT with a5, " \
               "GET2 wnode_aff = 1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario126' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a0', 'a1', 'a2' ], [] )
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a5' )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0' )

        if output != "":
            raise Exception( "Expect no jobs, but received one." )
        return True

class Scenario131( TestBase ):
    " Scenario 131 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, CHAFF add=a2, GET2 wnode_aff = 1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario131' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a2' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=1' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID + " Received: " + receivedJobID )
        return True

class Scenario132( TestBase ):
    " Scenario 132 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a0, SUBMIT with a1, SUBMIT with no aff, " \
               "CHAFF add=a2, GET2 aff=a1 wnode_aff = 1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a0' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario132' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a2' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=1 aff=a1' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID2 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID2 + " Received: " + receivedJobID )
        return True

class Scenario133( TestBase ):
    " Scenario 133 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHAFF add=a2, GET2 aff=a5 wnode_aff = 1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario133' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a2' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=1 aff=a5' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID2 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID2 + " Received: " + receivedJobID )
        return True

class Scenario134( TestBase ):
    " Scenario 134 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHAFF add=a2, GET2 aff=a5 wnode_aff=0 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario134' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a2' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=1 aff=a5' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID1 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID1 + " Received: " + receivedJobID )
        return True

class Scenario135( TestBase ):
    " Scenario 135 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHAFF add=a7, GET2 aff=a5 wnode_aff=1 any_aff=0"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario135' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a7' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0 aff=a5' )
        if output != "":
            raise Exception( "Expect no jobs, but received one." )
        return True

class Scenario136( TestBase ):
    " Scenario 136 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, SUBMIT with a2, SUBMIT with no aff, " \
               "CHAFF add=a7, GET2 aff=a5 wnode_aff=1 any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'a2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario136' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a7' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=1 aff=a5' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID1 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID1 + " Received: " + receivedJobID )
        return True

class Scenario137( TestBase ):
    " Scenario 137 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with a1, restart netschedule, " \
               "CHAFF add=a1, GET2 wnode_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'a1' )

        self.ns.shutdown()
        self.ns.resetPID()
        time.sleep( 15 )
        if self.ns.isRunning():
            raise Exception( "Cannot shutdown netschedule" )

        self.ns.start()
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario136' )
        ns_client.set_client_identification( 'node', 'session' )

        changeAffinity( ns_client, [ 'a1' ], [] )

        output = execAny( ns_client,
                          'GET2 wnode_aff=1 any_aff=0' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID1 != receivedJobID:
            raise Exception( "Received job ID does not match. Expected: " + \
                             jobID1 + " Received: " + receivedJobID )
        return True

class Scenario138( TestBase ):
    " Scenario 138 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " Return unknown job "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            # Job is unknown, but the key format is just fine
            self.ns.returnJob( "TEST", NON_EXISTED_JOB, ANY_AUTH_TOKEN )
        except Exception, exc:
            if "Job not found" in str( exc ) or \
               "eJobNotFound" in str( exc ):
                return True
            raise

        return False


class Scenario139( TestBase ):
    " Scenario 139 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT a job, DROJ it. Check the job status. "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        # DROJ is obsolete, it is now == cancel
        self.ns.cancelJob( 'TEST', jobID )

        # Check the job status using 3 ways
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 == "Canceled" and \
           status2 == "Canceled" and \
           status3 == "Canceled":
            return True
        return False


class Scenario140( TestBase ):
    " Scenario 140 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " GETCONF as an administrator "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        config = self.ns.getServerConfiguration( "netschedule_admin" )
        return "node_id" in config


class Scenario141( TestBase ):
    " Scenario 141 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " GETCONF as nobody "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.getServerConfiguration()
        except Exception, exc:
            if "Access denied: admin privileges required" in str( exc ):
                return True
            raise

        return False


class Scenario142( TestBase ):
    " Scenario 142 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " CANCEL a job twice "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.ns.cancelJob( 'TEST', jobID )

        # Check the job status using 3 ways
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Canceled" or \
           status2 != "Canceled" or \
           status3 != "Canceled":
            return False

        # Cancel the job again
        self.ns.cancelJob( 'TEST', jobID )

        # Check the job status using 3 ways
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Canceled" or \
           status2 != "Canceled" or \
           status3 != "Canceled":
            return False

        return True


class Scenario143( TestBase ):
    " Scenario 143 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, wait till timeout, check status "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobIDReceived = self.ns.getJob( 'TEST' )[ 0 ]

        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        time.sleep( 15 )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Pending" or \
           status2 != "Pending" or \
           status3 != "Pending":
            return False
        return True

class Scenario144( TestBase ):
    " Scenario 144 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, PUT, READ, wait till timeout, check status "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )

        if jobID != jobInfo[ 0 ]:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        readJobID, state, passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                                                 'mynode',
                                                                 'session1' )
        if readJobID != jobID:
            raise Exception( "Unexpected job received for reading." )

        time.sleep( 15 )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Done" or \
           status2 != "Done" or \
           status3 != "Done":
            return False
        return True

class Scenario145( TestBase ):
    " Scenario 145 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, wait till timeout, disconnect, GET, check status "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobIDReceived = self.ns.getJob( 'TEST', node = 'client1' )[ 0 ]

        if jobID != jobIDReceived:
            raise Exception( "Inconsistency 1 detected" )

        time.sleep( 15 )

        jobInfo = self.ns.getJob( 'TEST', node = 'client2' )
        if not jobInfo or jobID != jobInfo[ 0 ]:
            raise Exception( "Inconsistency 2 detected" )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Running" or \
           status2 != "Running" or \
           status3 != "Running":
            return False
        return True


class Scenario146( TestBase ):
    " Scenario 146 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, [GET, FAIL] 3 times, check run_counter "

    def getAndFail( self, clientNode ):
        " get a job and then fail it "
        jobInfo = self.ns.getJob( 'TEST', node = clientNode )
        jobIDReceived = jobInfo[ 0 ]
        if self.jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.failJob( 'TEST', self.jobID, jobInfo[ 1 ], 3 )
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.getAndFail('client1')
        self.getAndFail('client2')
        self.getAndFail('client3')

        # Check the run_counter
        info = self.ns.getJobInfo( 'TEST', self.jobID )
        return info[ "run_counter" ] == "3"



class Scenario147( TestBase ):
    " Scenario 147 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, [GET, FAIL] 4 times, check status "

    def getAndFail( self, clientNode ):
        " get a job and then fail it "
        jobInfo = self.ns.getJob( 'TEST', node = clientNode )
        jobIDReceived = jobInfo[ 0 ]
        if self.jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.failJob( 'TEST', self.jobID, jobInfo[ 1 ], 3 )
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.getAndFail('client1')
        self.getAndFail('client2')
        self.getAndFail('client3')
        self.getAndFail('client4')

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', self.jobID )
        status2 = self.ns.getJobStatus( 'TEST', self.jobID )

        info = self.ns.getJobInfo( 'TEST', self.jobID )
        status3 = info[ "status" ]

        if status1 != "Failed" or \
           status2 != "Failed" or \
           status3 != "Failed":
            return False
        return True

class Scenario148( TestBase ):
    " Scenario 148 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.count = 0
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, [READ, FAIL READ] 4 times, check status "

    def readAndFail( self ):
        " get a job and then fail it "
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               str( self.count ),
                                               str( self.count ) )
        self.ns.failRead2( 'TEST', readJobID, passport, "",
                           str( self.count ), str( self.count ) )
        self.count += 1
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )

        self.readAndFail()
        self.readAndFail()
        self.readAndFail()
        self.readAndFail()

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "ReadFailed" or \
           status2 != "ReadFailed" or \
           status3 != "ReadFailed":
            return False
        return True

class Scenario149( TestBase ):
    " Scenario 149 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, [GET, wait till fail] 4 times, check status "

    def getAndWaitTillFail( self, clientNode ):
        " get a job and then fail it "
        jobIDReceived = self.ns.getJob( 'TEST', node = clientNode )[ 0 ]
        if self.jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        time.sleep( 15 )
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.jobID = self.ns.submitJob( 'TEST', 'bla' )
        self.getAndWaitTillFail('client1')
        self.getAndWaitTillFail('client2')
        self.getAndWaitTillFail('client3')
        self.getAndWaitTillFail('client4')

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', self.jobID )
        status2 = self.ns.getJobStatus( 'TEST', self.jobID )

        info = self.ns.getJobInfo( 'TEST', self.jobID )
        status3 = info[ "status" ]

        if status1 != "Failed" or \
           status2 != "Failed" or \
           status3 != "Failed":
            return False
        return True

class Scenario150( TestBase ):
    " Scenario 150 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.count = 0
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, PUT, [READ, wait till fail] 4 times, check status "

    def readAndWaitTillFail( self ):
        " read a job and then fail it "
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               str( self.count ),
                                               str( self.count ) )
        self.count += 1
        if self.jobID != readJobID:
            raise Exception( "Inconsistency detected" )
        time.sleep( 15 )
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        receivedJobID = jobInfo[ 0 ]
        if self.jobID != receivedJobID:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', self.jobID, jobInfo[ 1 ], 0, "" )

        self.readAndWaitTillFail()
        self.readAndWaitTillFail()
        self.readAndWaitTillFail()
        self.readAndWaitTillFail()

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', self.jobID )
        status2 = self.ns.getJobStatus( 'TEST', self.jobID )

        info = self.ns.getJobInfo( 'TEST', self.jobID )
        status3 = info[ "status" ]

        if status1 != "ReadFailed" or \
           status2 != "ReadFailed" or \
           status3 != "ReadFailed":
            return False
        return True

class Scenario151( TestBase ):
    " Scenario 151 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, DUMP -> check read counter, " \
               "wait till timeout, READ, DUMP -> check read counter, " \
               "RDRB, DUMP -> check read counter, " \
               "READ, DUMP -> check read counter, " \
               "CFRM, DUMP -> check read counter"

    def checkReadCounter( self, expectedCount ):
        " Checks a job read counter "
        info = self.ns.getJobInfo( 'TEST', self.jobID )
        counter = int( info[ "read_counter" ] )
        if counter != expectedCount:
            raise Exception( "Unexpected read counter. Expected: " + \
                             str( expectedCount ) + \
                             " Received: " + str( counter ) )
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        receivedJobID = jobInfo[ 0 ]
        if self.jobID != receivedJobID:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', self.jobID, jobInfo[ 1 ], 0, "" )
        jobID, s, p = self.ns.getJobsForReading2( 'TEST', -1, '', 'n1', 's' )
        self.checkReadCounter( 1 )

        time.sleep( 15 )
        jobID, s, p = self.ns.getJobsForReading2( 'TEST', -1, '', 'n2', 's' )
        self.checkReadCounter( 2 )

        self.ns.rollbackRead2( 'TEST', self.jobID, p, "n", "s" )
        self.checkReadCounter( 1 )

        jobID, s, p = self.ns.getJobsForReading2( 'TEST', -1, '', 'n3', 's' )
        self.checkReadCounter( 2 )

        self.ns.confirmRead2( 'TEST', self.jobID, p, 'n', 's' )
        self.checkReadCounter( 2 )
        return True

class Scenario153( TestBase ):
    " Scenario 153 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, check status, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobInfo = self.ns.getJob( 'TEST',
                node = 'mynode', session = 'mysession' )
        if jobID != jobInfo[ 0 ]:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "",
                        'mynode', 'mysession' )
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'mynode', 'mysession' )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Reading" or \
           status2 != "Reading" or \
           status3 != "Reading":
            return False

        ns_client = self.getNetScheduleService( 'TEST', 'scenario169' )
        client = getClientInfo( ns_client, 'mynode' )
        if len( client[ 'reading_jobs' ] ) != 1 or \
           client[ 'reading_jobs' ][ 0 ] != jobID:
            raise Exception( "Wrong job in the reading jobs list" )
        return True

class Scenario154( TestBase ):
    " Scenario 154 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, CFRM, check status and reading count"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "", 'mynode', 'mysession' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "",
                        'mynode', 'mysession' )
        jobIDReceived, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'mynode', 'mysession' )
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.confirmRead2( 'TEST', jobID, passport, 'mynode', 'mysession' )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Confirmed" or \
           status2 != "Confirmed" or \
           status3 != "Confirmed":
            raise Exception( "Unexpected job status" )
        if int( info[ "read_counter" ] ) != 1:
            raise Exception( "Unexpected reading count" )
        return True

class Scenario155( TestBase ):
    " Scenario 155 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, RDRB, check status and reading count"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "", 'mynode', 'mysession' )
        jobIDReceived = jobInfo [ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "",
                        'mynode', 'mysession' )
        jobIDReceived, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'mynode', 'mysession' )
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.rollbackRead2( 'TEST', jobID, passport, 'mynode', 'mysession' )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Done" or \
           status2 != "Done" or \
           status3 != "Done":
            raise Exception( "Unexpected job status" )
        if int( info[ "read_counter" ] ) != 0:
            raise Exception( "Unexpected reading count" )
        return True

class Scenario156( TestBase ):
    " Scenario 156 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, RDRB (wrong passport), check status and reading count"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "", 'mynode', 'mysession' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "",
                        'mynode', 'mysession' )
        jobIDReceived, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'mynode', 'mysession' )
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        try:
            parts = passport.split( '_' )
            if len( parts ) != 2:
                raise Exception( "Unexpected passport format" )
            badPassport = str( int( parts[ 0 ] ) + 1 ) + '_' + parts[ 1 ] + '7'
            self.ns.rollbackRead2( 'TEST', jobID, badPassport,
                                   'mynode', 'mysession' )
            raise Exception( "Wrong RDRB but no exception" )
        except Exception, excpt:
            if "Authorization token does not match" not in str( excpt ):
                raise

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Reading" or \
           status2 != "Reading" or \
           status3 != "Reading":
            raise Exception( "Unexpected job status" )
        if int( info[ "read_counter" ] ) != 1:
            raise Exception( "Unexpected reading count" )
        return True

class Scenario157( TestBase ):
    " Scenario 157 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, FRED, check status and reading count"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'mynode', 'mysession'  )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "",
                        'mynode', 'mysession' )
        jobIDReceived, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'mynode', 'mysession' )
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.failRead2( 'TEST', jobID, passport, "",
                          'mynode', 'mysession' )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Done" or \
           status2 != "Done" or \
           status3 != "Done":
            raise Exception( "Unexpected job status" )
        if int( info[ "read_counter" ] ) != 1:
            raise Exception( "Unexpected reading count" )
        return True

class Scenario158( TestBase ):
    " Scenario 158 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, FRED (wrong passport), check status and reading count"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "", 'mynode', 'mysession'  )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "",
                        'mynode', 'mysession' )
        jobIDReceived, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'mynode', 'mysession' )
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        try:
            self.ns.failRead2( 'TEST', jobID, "7" + passport, "",
                               'mynode', 'mysession' )
            raise Exception( "Wrong FRED but no exception" )
        except Exception, excpt:
            if "authorization" not in str( excpt ).lower():
                raise

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Reading" or \
           status2 != "Reading" or \
           status3 != "Reading":
            raise Exception( "Unexpected job status" )
        if int( info[ "read_counter" ] ) != 1:
            raise Exception( "Unexpected reading count" )
        return True

class Scenario159( TestBase ):
    " Scenario 159 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, CANCEL, CFRM, check status and reading count"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'mynode', 'mysession'  )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "",
                        'mynode', 'mysession' )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'mynode', 'mysession' )

        if jobID != readJobID:
            raise Exception( "Inconsistency detected" )

        self.ns.cancelJob( 'TEST', jobID, 'mynode', 'mysession')

        try:
            self.ns.confirmRead2( 'TEST', jobID, passport,
                                "mynode", "mysession" )
            raise Exception( "Expected error, got nothing" )
        except Exception, exc:
            if not "Canceled" in str( exc ):
                raise

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Canceled" or \
           status2 != "Canceled" or \
           status3 != "Canceled":
            raise Exception( "Unexpected job status"  )
        if int( info[ "read_counter" ] ) != 1:
            raise Exception( "Unexpected reading count" )
        return True

class Scenario160( TestBase ):
    " Scenario 160 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT as identified, check submit event "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        info = self.ns.getJobInfo( 'TEST', jobID )

        return info[ 'event' ] == 'Submit' and \
               info[ "node" ] == "mynode" and \
               info[ "session" ] == "mysession"

class Scenario161( TestBase ):
    " Scenario 161 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT as anonymous, check submit event "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        info = self.ns.getJobInfo( 'TEST', jobID )

        if info.has_key( "event" ):
            # grid 1.5
            return info[ 'event' ] == 'Submit' and \
                   info[ "node" ] == "" and \
                   info[ "session" ] == ""
        # grid 1.4
        return "event=Submit" in info[ "event1" ] and \
               "node=''" in info[ "event1" ] and \
               "session=''" in info[ "event1" ]

class Scenario162( TestBase ):
    " Scenario 162 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, wait till timeout as c1; GET, RETURN as c2; " \
               "GET, FPUT as c3; GET, PUT, READ, timeout as c4; " \
               "READ, RETURN as c5; READ, FRED as c6; READ, CFRM, CANCEL as c7; " \
               "DUMP and check the history"

    def checkEvent( self, client, event, status, node, session ):
        " Checks the last event fields "
        info = self.ns.getJobInfo( 'TEST', self.jobID )
        if info[ 'client' ] != client:
            raise Exception( "Unexpected client. Expected: " + client + \
                             " Received: " + info[ 'client' ] )
        if info[ 'event' ] != event:
            raise Exception( "Unexpected event. Expected: " + event + \
                             " Received: " + info[ 'event' ] )
        if info[ 'status' ] != status:
            raise Exception( "Unexpected status. Expected: " + status + \
                             " Received: " + info[ 'status' ] )
        if info[ 'node' ] != node:
            raise Exception( "Unexpected node. Expected: " + node + \
                             " Received: " + info[ 'node' ] )
        if info[ 'session' ] != session:
            raise Exception( "Unexpected node. Expected: " + session + \
                             " Received: " + info[ 'session' ] )
        return

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'c1', 'session1' )
        self.checkEvent( 'localhost', 'Submit', 'Pending', 'c1', 'session1' )

        self.ns.getJob( 'TEST', -1, '', "",
                                        'c2', 'session2' )
        self.checkEvent( 'localhost', 'Request', 'Running', 'c2', 'session2' )
        time.sleep( 15 )
        self.checkEvent( 'ns', 'Timeout', 'Pending', '', '' )

        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'c3', 'session3' )
        self.checkEvent( 'localhost', 'Request', 'Running', 'c3', 'session3' )
        self.ns.returnJob( 'TEST', self.jobID, jobInfo[ 1 ], 'c31', 'session31' )
        self.checkEvent( 'localhost', 'Return', 'Pending', 'c31', 'session31' )

        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'c4', 'session4' )
        self.checkEvent( 'localhost', 'Request', 'Running', 'c4', 'session4' )
        self.ns.failJob( 'TEST', self.jobID, jobInfo[ 1 ], 4, '', '', 'c5', 'session5' )
        self.checkEvent( 'localhost', 'Fail', 'Pending', 'c5', 'session5' )

        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'c6', 'session6' )
        self.checkEvent( 'localhost', 'Request', 'Running', 'c6', 'session6' )
        self.ns.putJob( 'TEST', jobInfo[ 0 ], jobInfo[ 1 ], 0, "", 'c7', 'session7' )
        self.checkEvent( 'localhost', 'Done', 'Done', 'c7', 'session7' )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'c8', 'session8' )
        self.checkEvent( 'localhost', 'Read', 'Reading', 'c8', 'session8' )
        time.sleep( 15 )
        self.checkEvent( 'ns', 'ReadTimeout', 'Done', '', '' )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'c9', 'session9' )
        self.checkEvent( 'localhost', 'Read', 'Reading', 'c9', 'session9' )
        self.ns.rollbackRead2( 'TEST', self.jobID, passport, 'c10', 'session10' )
        self.checkEvent( 'localhost', 'ReadRollback', 'Done', 'c10', 'session10' )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'c11', 'session11' )
        self.checkEvent( 'localhost', 'Read', 'Reading', 'c11', 'session11' )
        self.ns.failRead2( 'TEST', self.jobID, passport, "", 'c12', 'session12' )
        self.checkEvent( 'localhost', 'ReadFail', 'Done', 'c12', 'session12' )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'c13', 'session13' )
        self.checkEvent( 'localhost', 'Read', 'Reading', 'c13', 'session13' )
        self.ns.confirmRead2( 'TEST', self.jobID, passport, "c14", "session14" )
        self.checkEvent( 'localhost', 'ReadDone', 'Confirmed', 'c14', 'session14' )

        self.ns.cancelJob( 'TEST', self.jobID, 'c15', 'session15' )
        self.checkEvent( 'localhost', 'Cancel', 'Canceled', 'c15', 'session15' )
        return True

class Scenario163( TestBase ):
    " Scenario 163 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT as anonymous, GET, FAIL, check fail event "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.failJob( 'TEST', jobID, jobInfo[ 1 ], 4 )
        info = self.ns.getJobInfo( 'TEST', jobID )

        if info.has_key( "event" ):
            # grid 1.5
            return info[ 'event' ] == 'Fail' and \
                   info[ "status" ] == "Failed" and \
                   info[ "ret_code" ] == "4"
        # grid 1.4
        return "event=Fail" in info[ "event3" ] and \
               "status=Failed" in info[ "event3" ] and \
               "ret_code=4" in info[ "event3" ]


class Scenario164( TestBase ):
    " Scenario 164 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT as anonymous, GET, wait till fail, check fail event "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobIDReceived = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        time.sleep( 15 )

        info = self.ns.getJobInfo( 'TEST', jobID )

        if info.has_key( "event" ):
            # grid 1.5
            return info[ 'event' ] == 'Timeout' and \
                   info[ "status" ] == "Failed" and \
                   info[ "run_counter" ] == "1"
        # grid 1.4
        return "event=Timeout" in info[ "event3" ] and \
               "status=Failed" in info[ "event3" ] and \
               info[ "run_counter" ] == "1"

class Scenario165( TestBase ):
    " Scenario 165 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT as anonymous, GET, CLRN, check the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        jobIDReceived = self.ns.getJob( 'TEST', -1, '', "",
                                        'mynode', 'mysession' )[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario123' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'CLRN' )

        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Failed" or \
           status2 != "Failed" or \
           status3 != "Failed":
            return False
        return True

class Scenario166( TestBase ):
    " Scenario 166 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT as anonymous, GET, PUT, READ, FRED, check history"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        self.ns.failRead2( 'TEST', readJobID, passport, "", "node", "session" )
        info = self.ns.getJobInfo( 'TEST', jobID )

        if info[ 'event' ] != 'ReadFail':
            raise Exception( "Unexpected event: " + info[ 'event' ] )

        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "ReadFailed" or \
           status2 != "ReadFailed" or \
           status3 != "ReadFailed":
            return False
        return True

class Scenario167( TestBase ):
    " Scenario 167 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT as anonymous, GET, PUT, READ, timeout, check history"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        time.sleep( 15 )

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ 'event' ] != 'ReadTimeout':
            raise Exception( "Unexpected event: " + info[ 'event' ] )

        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "ReadFailed" or \
           status2 != "ReadFailed" or \
           status3 != "ReadFailed":
            return False
        return True

class Scenario168( TestBase ):
    " Scenario 168 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT as anonymous, GET, PUT, READ as identified, CLRN, check history"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario168' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, 'CLRN' )

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ 'event' ] != 'Clear':
            raise Exception( "Unexpected event: " + info[ 'event' ] )

        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "ReadFailed" or \
           status2 != "ReadFailed" or \
           status3 != "ReadFailed":
            return False
        return True

class Scenario169( TestBase ):
    " Scenario 169 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, RETURN, GET as the same identified client"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'scenario169', 'default' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.returnJob( 'TEST', jobID, jobInfo[ 1 ], 'scenario169', 'default' )
        if self.ns.getJob( 'TEST', -1, '', "", 'scenario169', 'default' ):
            raise Exception( "No jobs expected, however one was received" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario169' )
        client = getClientInfo( ns_client, 'scenario169' )
        if len( client[ 'blacklisted_jobs' ] ) != 1 or \
           client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Wrong job in the client blacklist" )
        return True

class Scenario170( TestBase ):
    " Scenario 170 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, FAIL, GET as the same identified client"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'scenario170', 'default' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.failJob( 'TEST', jobID, jobInfo[ 1 ], 4, '', '',
                'scenario170', 'default' )
        if self.ns.getJob( 'TEST', -1, '', "", 'scenario170', 'default' ):
            raise Exception( "No jobs expected, however one was received" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario170' )
        client = getClientInfo( ns_client, 'scenario170' )
        if len( client[ 'blacklisted_jobs' ] ) != 1 or \
           client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Wrong job in the client blacklist" )
        return True

class Scenario171( TestBase ):
    " Scenario 171 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT, GET, timeout till fail, " \
               "GET as the same identified client"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobIDReceived = self.ns.getJob( 'TEST', -1, '', "",
                                        'scenario171', 'default' )[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        time.sleep( 15 )
        if self.ns.getJob( 'TEST', -1, '', "", 'scenario171', 'default' ):
            raise Exception( "No jobs expected, however one was received" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario171' )
        client = getClientInfo( ns_client, 'scenario171' )
        if len( client[ 'blacklisted_jobs' ] ) != 1 or \
           client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Wrong job in the client blacklist" )
        return True

class Scenario172( TestBase ):
    " Scenario 172 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, RDRB as identified. " \
               "READ -> no jobs, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'node', 'session' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        if readJobID != jobID:
            raise Exception( "Inconsistency" )
        self.ns.rollbackRead2( 'TEST', readJobID, passport,
                               "node", "session" )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        if readJobID != "":
            raise Exception( "Expected no jobs but received one" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario172' )
        client = getClientInfo( ns_client, 'node' )
        if len( client[ 'blacklisted_jobs' ] ) != 1 or \
           client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Wrong job in the client blacklist" )
        return True

class Scenario173( TestBase ):
    " Scenario 173 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, FRED as identified. " \
               "READ -> no jobs, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'node', 'session' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        if readJobID != jobID:
            raise Exception( "Inconsistency" )
        self.ns.failRead2( 'TEST', readJobID, passport, "",
                           "node", "session" )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        if readJobID != "":
            raise Exception( "Expected no jobs but received one" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario173' )
        client = getClientInfo( ns_client, 'node' )
        if len( client[ 'blacklisted_jobs' ] ) != 1 or \
           client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Wrong job in the client blacklist" )
        return True

class Scenario174( TestBase ):
    " Scenario 174 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, timeout. " \
               "READ -> no jobs, STAT CLIENTS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'node', 'session' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        if readJobID != jobID:
            raise Exception( "Inconsistency" )
        time.sleep( 15 )

        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        if readJobID != "":
            raise Exception( "Expected no jobs but received one" )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario173' )
        client = getClientInfo( ns_client, 'node' )
        if len( client[ 'blacklisted_jobs' ] ) != 1 or \
           client[ 'blacklisted_jobs' ][ 0 ].split()[0] != jobID:
            raise Exception( "Wrong job in the client blacklist" )
        return True

class Scenario175( TestBase ):
    " Scenario 175 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET, PUT, READ, PUT -> error message"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'scenario175', 'default' )
        jobIDReceived = jobInfo[ 0 ]
        if jobID != jobIDReceived:
            raise Exception( "Inconsistency detected" )

        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        readJobID, \
        state, \
        passport = self.ns.getJobsForReading2( 'TEST', -1, '',
                                               'node', 'session' )
        if readJobID != jobID:
            raise Exception( "Inconsistency" )

        try:
            self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "" )
        except Exception, excpt:
            if 'Cannot accept job results' in str( excpt ):
                return True
            raise
        return False

class Scenario176( TestBase ):
    " Scenario 176 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " MGET for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.getJobProgressMessage( 'TEST', NON_EXISTED_JOB,
                                           'mynode', 'mysession' )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise

        return False


class Scenario177( TestBase ):
    " Scenario 177 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " MPUT for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.setJobProgressMessage( 'TEST', NON_EXISTED_JOB, 'msg',
                                           'mynode', 'mysession' )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise

        return False

class Scenario178( TestBase ):
    " Scenario 178 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " PUT for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.putJob( 'TEST', NON_EXISTED_JOB, ANY_AUTH_TOKEN, 0, "",
                            'mynode', 'mysession' )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise

        return False

class Scenario179( TestBase ):
    " Scenario 179 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " RETURN for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.returnJob( 'TEST', NON_EXISTED_JOB, ANY_AUTH_TOKEN,
                               'mynode', 'mysession' )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise

        return False

class Scenario180( TestBase ):
    " Scenario 180 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " DUMP for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.getJobInfo( 'TEST', NON_EXISTED_JOB,
                                'mynode', 'mysession' )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise

        return False

class Scenario181( TestBase ):
    " Scenario 181 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "RDRB for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.rollbackRead2( 'TEST', NON_EXISTED_JOB, 'passport',
                                   'node', 'session' )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise
        return False

class Scenario182( TestBase ):
    " Scenario 182 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "FRED for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.failRead2( 'TEST', NON_EXISTED_JOB, 'passport', "",
                               'node', 'session' )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise
        return False

class Scenario183( TestBase ):
    " Scenario 183 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CFRM for non-existed "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        try:
            self.ns.confirmRead2( 'TEST', NON_EXISTED_JOB, 'passport',
                                  "node", "session" )
        except Exception, exc:
            if "Job not found" in str( exc ):
                return True
            raise
        return False

class Scenario184( TestBase ):
    " Scenario 184 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with timeout and port, GET, PUT"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # Spawn SUBMIT
        process = self.ns.spawnSubmitWait( 'TEST', 3,
                                           "", 'node', 'session' )
        time.sleep( 1 )

        jobInfo = self.ns.getJob( 'TEST' )
        jobKey = jobInfo[ 0 ]
        self.ns.putJob( 'TEST', jobKey, jobInfo[ 1 ], 0, '' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning SUBMIT" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "job_key=" + jobKey in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected" )

class Scenario185( TestBase ):
    " Scenario 185 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with timeout and port, GET, FPUT"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        # Spawn SUBMIT
        process = self.ns.spawnSubmitWait( 'TEST', 3,
                                           "", 'node', 'session' )

        jobInfo = self.ns.getJob( 'TEST' )
        jobKey = jobInfo[ 0 ]
        self.ns.failJob( 'TEST', jobKey, jobInfo[ 1 ], 3 )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning SUBMIT" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "job_key=" + jobKey in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected" )

class Scenario186( TestBase ):
    " Scenario 186 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with timeout and port, GET, fail due to timeout"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        # Spawn SUBMIT
        process = self.ns.spawnSubmitWait( 'TEST', 20,
                                           "", 'node', 'session' )

        jobKey = self.ns.getJob( 'TEST' )[ 0 ]

        # The wait will have the pause till the job is timed out
        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning SUBMIT" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "job_key=" + jobKey in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected" )

class Scenario187( TestBase ):
    " Scenario 187 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with timeout and port, GET, CANCEL"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch( 3 )

        # Spawn SUBMIT
        process = self.ns.spawnSubmitWait( 'TEST', 3,
                                           "", 'node', 'session' )

        jobKey = self.ns.getJob( 'TEST' )[ 0 ]
        self.ns.cancelJob( 'TEST', jobKey )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning SUBMIT" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "job_key=" + jobKey in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected" )

class Scenario188( TestBase ):
    " Scenario 188 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 with timeout and port"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [], False, True,
                                         'node', 'session' )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "[valid" in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected" )

class Scenario189( TestBase ):
    " Scenario 189 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 with timeout and port and affinity a1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [ 'a1' ], False, False,
                                         'node', 'session' )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "[valid" in processStdout:
            raise Exception( "Expect no notifications but received one" )
        return True

class Scenario190( TestBase ):
    " Scenario 190 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 with timeout and port, and explicit aff a1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [ 'a1' ], False, False,
                                         'node', 'session' )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input', 'a1' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "[valid" in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected" )

class Scenario191( TestBase ):
    " Scenario 191 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 with timeout and port, and explicit aff a1, and any_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        safeMode = True
        if safeMode:
            ns_client = self.getNetScheduleService( 'TEST', 'scenario191' )
            ns_client.set_client_identification( 'node', 'session' )

            notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
            notifSocket.bind( ( "", 9007 ) )

            execAny( ns_client,
                     'GET2 wnode_aff=0 any_aff=1 exclusive_new_aff=0 aff=a1 port=9007 timeout=3' )

            # Submit a job
            self.ns.submitJob( 'TEST', 'input', 'a2' )

            time.sleep( 3 )
            result = self.getNotif( notifSocket )
            if result == 0:
                raise Exception( "Expected notification(s), received nothing" )
            return True


        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [ 'a1' ], False, True,
                                         'node', 'session' )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input', 'a2' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()       # analysis:ignore

        if "[valid" in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected" )

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



class Scenario192( TestBase ):
    " Scenario 192 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF add=a3; GET2 with timeout and port, wnode_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        safeMode = True
        if safeMode:
            ns_client = self.getNetScheduleService( 'TEST', 'scenario192' )
            ns_client.set_client_identification( 'node', 'session' )
            changeAffinity( ns_client, ['a3'], [] )

            notifSocket = socket.socket( socket.AF_INET, socket.SOCK_DGRAM )
            notifSocket.bind( ( "", 9007 ) )

            execAny( ns_client,
                     'GET2 wnode_aff=1 any_aff=0 exclusive_new_aff=1 port=9007 timeout=3' )

            # Submit a job
            self.ns.submitJob( 'TEST', 'input', 'a3' )

            time.sleep( 3 )
            result = self.getNotif( notifSocket )
            if result == 0:
                raise Exception( "Expected one notification, received nothing" )
            return True

        # Unsafe mode: grid_cli has a bug which breaks the test
        # It's been way too long to wait till it is fixed, so there is a workaround
        # above to avoid using buggy part of grid_cli
        ns_client = self.getNetScheduleService( 'TEST', 'scenario192' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, ['a3'], [] )

        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [], True, False,
                                         'node', 'session' )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input', 'a3' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()       # analysis:ignore

        if "[valid" in processStdout:
            return True

        raise Exception( "Did not receive notifications when expected. "
                         "Received:\n'" + processStdout + "'" )

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


class Scenario193( TestBase ):
    " Scenario 193 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF add=a3; GET2 with timeout and port, wnode_aff=1"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario193' )
        ns_client.set_client_identification( 'node', 'session' )
        changeAffinity( ns_client, ['a3'], [] )

        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [], True, False,
                                         'node', 'session' )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input', 'a4' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "[valid" in processStdout:
            raise Exception( "Received notifications when not expected" )
        return True

class Scenario194( TestBase ):
    " Scenario 194 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SST as anonymous, SST as identified, check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario194' )

        self.ns.getFastJobStatus( 'TEST', NON_EXISTED_JOB )
        getClientInfo( ns_client, None, 0, 0 )

        self.ns.getFastJobStatus( 'TEST', NON_EXISTED_JOB,
                                  'mynode', 'mysession')
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario195( TestBase ):
    " Scenario 195 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " WST as anonymous, WST as identified, check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario195' )

        self.ns.getJobStatus( 'TEST', NON_EXISTED_JOB )
        getClientInfo( ns_client, None, 0, 0 )

        self.ns.getJobStatus( 'TEST', NON_EXISTED_JOB,
                              'mynode', 'mysession')
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario196( TestBase ):
    " Scenario 196 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " SUBMIT as anonymous, SUBMIT as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario196' )

        self.ns.submitJob( 'TEST', 'input' )
        getClientInfo( ns_client, None, 0, 0 )

        self.ns.submitJob( 'TEST', 'input', '', '',
                           'mynode', 'mysession')
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario197( TestBase ):
    " Scenario 197 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " CANCEL as anonymous, CANCEL as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario197' )

        self.ns.cancelJob( 'TEST', NON_EXISTED_JOB )
        getClientInfo( ns_client, None, 0, 0 )

        self.ns.cancelJob( 'TEST', NON_EXISTED_JOB,
                           'mynode', 'mysession')
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario198( TestBase ):
    " Scenario 198 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " STATUS as anonymous, STATUS as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario198' )

        try:
            # This generates an exception for non existing job
            self.ns.getJobBriefStatus( 'TEST', NON_EXISTED_JOB )
        except:
            pass
        getClientInfo( ns_client, None, 0, 0 )

        try:
            # This generates an exception for non existing job
            self.ns.getJobBriefStatus( 'TEST', NON_EXISTED_JOB,
                                    'mynode', 'mysession' )
        except:
            pass
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario203( TestBase ):
    " Scenario 203 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " STAT as anonymous, STAT as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario203' )
        self.ns.getBriefStat( 'TEST' )
        client = getClientInfo( ns_client, None, 0, 0 )

        self.ns.getBriefStat( 'TEST', 'mynode', 'mysession' )
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario204( TestBase ):
    " Scenario 204 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " STAT as anonymous, STAT as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario204' )
        self.ns.getAffinityStatus( 'TEST' )
        getClientInfo( ns_client, None, 0, 0 )

        self.ns.getAffinityStatus( 'TEST',
                                   node = 'mynode', session = 'mysession' )
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario206( TestBase ):
    " Scenario 206 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " MPUT as anonymous, MPUT as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario206' )
        jobID = self.ns.submitJob( 'TEST', 'bla',
                                   node = 'prep', session = 'session' )
        self.ns.getJob( 'TEST', node = 'prep', session = 'session' )

        self.ns.setJobProgressMessage( 'TEST', jobID, 'msg' )

        getClientInfo( ns_client, None, 1, 1 )

        self.ns.setJobProgressMessage( 'TEST', jobID,
                                           'msg', 'mynode', 'mysession' )
        getClientInfo( ns_client, 'mynode', 2, 2 )
        return True

class Scenario207( TestBase ):
    " Scenario 207 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " MGET as anonymous, MGET as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario207' )
        jobID = self.ns.submitJob( 'TEST', 'bla',
                                   node = 'prep', session = 'session' )
        self.ns.getJob( 'TEST', node = 'prep', session = 'session' )

        self.ns.getJobProgressMessage( 'TEST', jobID )
        getClientInfo( ns_client, None, 1, 1 )

        self.ns.getJobProgressMessage( 'TEST', jobID, 'mynode', 'mysession' )
        getClientInfo( ns_client, 'mynode', 2, 2 )
        return True

class Scenario208( TestBase ):
    " Scenario 208 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " DUMP as anonymous, DUMP as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario208' )
        self.ns.getQueueDump( 'TEST' )
        getClientInfo( ns_client, None, 0, 0 )

        self.ns.getQueueDump( 'TEST', node = 'mynode', session = 'mysession' )
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario212( TestBase ):
    " Scenario 212 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " JDEX as anonymous, JDEX as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario212' )
        jobID = self.ns.submitJob( 'TEST', 'bla',
                                   node = 'prep', session = 'session' )
        self.ns.getJob( 'TEST', node = 'prep', session = 'session' )
        self.ns.extendJobExpiration( 'TEST', jobID, 5 )

        getClientInfo( ns_client, None, 1, 1 )

        self.ns.extendJobExpiration( 'TEST', jobID, 5, 'mynode', 'mysession' )
        getClientInfo( ns_client, 'mynode', 2, 2 )
        return True

class Scenario213( TestBase ):
    " Scenario 213 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " STSN as anonymous, STSN as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario213' )
        jobID = self.ns.submitJob( 'TEST', 'bla', affinity = 'myaff',
                                   node = 'prep', session = 'session' )
        self.ns.getAffinityStatus( 'TEST', 'myaff' )

        getClientInfo( ns_client, None, 1, 1 )

        self.ns.getAffinityStatus( 'TEST', 'myaff', 'mynode', 'mysession' )
        getClientInfo( ns_client, 'mynode', 2, 2 )
        return True

class Scenario214( TestBase ):
    " Scenario 214 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " GETP as anonymous, GETP as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario214' )
        output = execAny( ns_client, 'GETP' )
        getClientInfo( ns_client, None, 0, 0 )

        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario214' )
        ns_client1.set_client_identification( 'mynode', 'mysession' )
        output = execAny( ns_client1, 'GETP' )
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario215( TestBase ):
    " Scenario 215 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " GETC as anonymous, GETC as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario215' )
        output = execAny( ns_client, 'GETC' )
        getClientInfo( ns_client, None, 0, 0 )

        ns_client1 = self.getNetScheduleService( 'TEST', 'scenario215' )
        ns_client1.set_client_identification( 'mynode', 'mysession' )
        output = execAny( ns_client1, 'GETC' )
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario216( TestBase ):
    " Scenario 216 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " READ as anonymous, READ as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario216' )
        self.ns.getJobForReading( 'TEST' )
        client = getClientInfo( ns_client, False, 0 )
        if client is not None:
            raise Exception( "Expected no client. Received some." )

        self.ns.getJobForReading( 'TEST', 'mynode', 'mysession' )
        client = getClientInfo( ns_client, False, 1, 0 )
        if client is None:
            raise Exception( "Expected a client. Received none." )
        return True

class Scenario217( TestBase ):
    " Scenario 217 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CFRM as anonymous, CFRM as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario217' )
        try:
            self.ns.confirmReading( 'TEST', NON_EXISTED_JOB, 'passport' )
        except:
            pass
        client = getClientInfo( ns_client, False, 0 )
        if client is not None:
            raise Exception( "Expected no client. Received some." )

        try:
            self.ns.confirmReading( 'TEST', NON_EXISTED_JOB, 'passport',
                                    'mynode', 'mysession' )
        except:
            pass
        client = getClientInfo( ns_client, False, 1, 0 )
        if client is None:
            raise Exception( "Expected a client. Received none." )
        return True

class Scenario218( TestBase ):
    " Scenario 218 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "FRED as anonymous, FRED as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario218' )
        try:
            self.ns.failReadingJob( 'TEST', NON_EXISTED_JOB, 'passport' )
        except:
            pass
        client = getClientInfo( ns_client, False, 0 )
        if client is not None:
            raise Exception( "Expected no client. Received some." )

        try:
            self.ns.failReadingJob( 'TEST', NON_EXISTED_JOB, 'passport',
                                    'mynode', 'mysession' )
        except:
            pass
        client = getClientInfo( ns_client, False, 1, 0 )
        if client is None:
            raise Exception( "Expected a client. Received none." )
        return True

class Scenario219( TestBase ):
    " Scenario 219 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "RDRB as anonymous, RDRB as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario219' )
        try:
            self.ns.rollbackJob( 'TEST', NON_EXISTED_JOB, 'passport' )
        except:
            pass
        client = getClientInfo( ns_client, False, 0 )
        if client is not None:
            raise Exception( "Expected no client. Received some." )

        try:
            self.ns.rollbackJob( 'TEST', NON_EXISTED_JOB, 'passport',
                                 'mynode', 'mysession' )
        except:
            pass
        client = getClientInfo( ns_client, False, 1, 0 )
        if client is None:
            raise Exception( "Expected a client. Received none." )
        return True

class Scenario220( TestBase ):
    " Scenario 220 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " AFLS as anonymous, AFLS as identified, " \
               "check clients registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario220' )
        self.ns.getAffinityList( 'TEST' )
        getClientInfo( ns_client, None, 0, 0 )

        self.ns.getAffinityList( 'TEST', 'mynode', 'mysession' )
        getClientInfo( ns_client, 'mynode', 1, 1 )
        return True

class Scenario221( TestBase ):
    " Scenario 221 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CWGET as anonymous"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario221' )

        try:
            execAny( ns_client, 'CWGET' )
        except Exception, exc:
            if "no client_node and client_session at handshake" in str( exc ):
                return True
            raise
        raise Exception( "Expected auth exception, got nothing" )

class Scenario222( TestBase ):
    " Scenario 222 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CWGET as identified"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario221' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'CWGET' )
        return True

class Scenario223( TestBase ):
    " Scenario 223 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2, CWGET, SUBMIT -> no notifications"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 5,
                                         [], True, True,
                                         'node', 'session' )

        # Cancel waiting
        ns_client = self.getNetScheduleService( 'TEST', 'scenario223' )
        ns_client.set_client_identification( 'node', 'session' )
        execAny( ns_client, 'CWGET' )
        time.sleep( 1 )

        # Submit a job
        self.ns.submitJob( 'TEST', 'input' )

        process.wait()
        if process.returncode != 0:
            raise Exception( "Error spawning GET2" )
        processStdout = process.stdout.read()
        processStderr = process.stderr.read()

        if "[valid" in processStdout:
            raise Exception( "Receive notifications when not expected: " + \
                             processStdout)
        return True

class Scenario224( TestBase ):
    " Scenario 224 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " Get affinites list "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario224' )
        getNotificationInfo( ns_client, False, 0 )
        return True

class Scenario225( TestBase ):
    " Scenario 225 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " WGET as anonymous, check notifications registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario225' )
        execAny( ns_client, 'WGET 5000 15' )
        getNotificationInfo( ns_client, False, 1, 0 )
        return True

class Scenario226( TestBase ):
    " Scenario 226 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " WGET as identified, check notifications registry "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario226' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'WGET 5000 15' )
        getNotificationInfo( ns_client, False, 1, 0 )
        return True

class Scenario227( TestBase ):
    " Scenario 227 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "WGET as identified, check notifications registry, " \
               "wait till timeout exceeded, check notifications registry"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario227' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'WGET 5000 5' )
        getNotificationInfo( ns_client, False, 1, 0 )
        time.sleep( 10 )
        getNotificationInfo( ns_client, False, 0 )
        return True

class Scenario228( TestBase ):
    " Scenario 228 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "WGET as identified with wnode_aff=1, " \
               "check notifications registry"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario228' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'WGET 5000 5' )
        info = getNotificationInfo( ns_client, False, 1, 0 )
        if info[ 'any_job' ] != True:
            raise Exception( "any_job expected True" )
        if info [ 'active' ] != False:
            raise Exception( "active expected False" )
        if info[ 'use_preferred_affinities' ] != False:
            raise Exception( "use_preferred_affinities expected False" )
        if info[ 'recepient_address' ] != 'localhost:5000':
            raise Exception( "recepient_address expected 'localhost:5000'" )
        return True

class Scenario229( TestBase ):
    " Scenario 229 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "WGET as identified, " \
               "check notifications registry"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario229' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'WGET 5010 5' )
        info = getNotificationInfo( ns_client, False, 1, 0 )
        if info[ 'any_job' ] != True:
            raise Exception( "any_job expected True" )
        if info [ 'active' ] != False:
            raise Exception( "active expected False" )
        if info[ 'client_node' ] != 'mynode':
            raise Exception( "client node expected 'mynode'" )
        if info[ 'use_preferred_affinities' ] != False:
            raise Exception( "use_preferred_affinities expected False" )
        if info[ 'recepient_address' ] != 'localhost:5010':
            raise Exception( "recepient_address expected 'localhost:5010'" )
        return True

class Scenario230( TestBase ):
    " Scenario 230 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "WGET as identified with affinities, " \
               "check notifications registry"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario230' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'WGET 5010 5 aff=a1,a2' )
        info = getNotificationInfo( ns_client, True, 1, 0 )
        if info[ 'any_job' ] != False:
            raise Exception( "any_job expected False" )
        if info [ 'active' ] != False:
            raise Exception( "active expected False" )
        if info[ 'client_node' ] != 'mynode':
            raise Exception( "client node expected 'mynode'" )
        if info[ 'use_preferred_affinities' ] != False:
            raise Exception( "use_preferred_affinities expected False" )
        if info[ 'recepient_address' ] != 'localhost:5010':
            raise Exception( "recepient_address expected 'localhost:5010'" )
        if len( info[ 'explicit_affinities' ] ) != 2:
            raise Exception( "Expected 2 explicit affinities, received: " + \
                             str( len( info[ 'explicit_affinities' ] ) ) )
        if "a1" not in info[ 'explicit_affinities' ]:
            raise Exception( "a1 not found in the explicit affinities list" )
        if "a2" not in info[ 'explicit_affinities' ]:
            raise Exception( "a2 not found in the explicit affinities list" )
        return True

class Scenario231( TestBase ):
    " Scenario 231 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "WGET as identified with any_aff=1, " \
               "check notifications registry"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario231' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'WGET 5000 5' )
        info = getNotificationInfo( ns_client, False, 1, 0 )
        if info[ 'any_job' ] != True:
            raise Exception( "any_job expected True" )
        if info [ 'active' ] != False:
            raise Exception( "active expected False" )
        if info[ 'use_preferred_affinities' ] != False:
            raise Exception( "use_preferred_affinities expected False" )
        if info[ 'recepient_address' ] != 'localhost:5000':
            raise Exception( "recepient_address expected 'localhost:5000'" )
        return True

class Scenario232( TestBase ):
    " Scenario 232 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "WGET as identified, SUBMIT as anonymous, " \
               "check notifications registry"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario232' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        execAny( ns_client, 'WGET 5000 15' )
        self.ns.submitJob( 'TEST', 'bla' )
        time.sleep( 1 )
        info = getNotificationInfo( ns_client, True, 1, 0 )
        if info[ 'any_job' ] != True:
            raise Exception( "any_job expected True" )
        if info [ 'active' ] != True:
            raise Exception( "active expected True" )
        if info[ 'use_preferred_affinities' ] != False:
            raise Exception( "use_preferred_affinities expected False" )
        if info[ 'recepient_address' ] != 'localhost:5000':
            raise Exception( "recepient_address expected 'localhost:5000'" )
        return True

class Scenario233( TestBase ):
    " Scenario 233 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " DUMP 3 jobs after the first one "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )

        dump = self.ns.getQueueDump( 'TEST', start_after = jobID1,
                                     node = 'mynode', session = 'mysession' )
        count = 0
        for line in dump:
            if line.startswith( "key: " ):
                count += 1
        if count != 3:
            raise Exception( "Unexpected number of jobs in the output. " \
                             "Expected: 3. Received: " + str( count ) )
        return True


class Scenario234( TestBase ):
    " Scenario 234 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " DUMP 2 jobs after the first one "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )

        dump = self.ns.getQueueDump( 'TEST', '', jobID1, 2, '',
                                     'mynode', 'mysession' )
        count = 0
        for line in dump:
            if line.startswith( "key: " ):
                count += 1
        if count != 2:
            raise Exception( "Unexpected number of jobs in the output. " \
                             "Expected: 2. Received: " + str( count ) )
        return True

class Scenario235( TestBase ):
    " Scenario 235 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        self.jobID = None
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " DUMP 1 jobs after the first one "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )

        dump = self.ns.getQueueDump( 'TEST', '', '', 1, '',
                                     'mynode', 'mysession' )
        count = 0
        jobLine = ""
        for line in dump:
            if line.startswith( "key: " ):
                count += 1
                jobLine = line
        if count != 1:
            raise Exception( "Unexpected number of jobs in the output. " \
                             "Expected: 1. Received: " + str( count ) )
        if jobLine != "key: " + jobID1:
            raise Exception( "Unexpected job in the dump output. " \
                             "Expected: 'key: " + jobID1 + \
                             "' Received: '" + jobLine + "'" )
        return True

class Scenario236( TestBase ):
    " Scenario 236 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " DUMP 50 jobs, expect to have 2 (2 submitted) "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )
        self.ns.submitJob( 'TEST', 'bla', '', '', 'mynode', 'mysession' )

        dump = self.ns.getQueueDump( 'TEST', '', '', 50, '',
                                     'mynode', 'mysession' )
        count = 0
        for line in dump:
            if line.startswith( "key: " ):
                count += 1
        if count != 2:
            raise Exception( "Unexpected number of jobs in the output. " \
                             "Expected: 2. Received: " + str( count ) )
        return True

class Scenario237( TestBase ):
    " Scenario 237 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT as c1, GET and RETURN as c2, GET and PUT as c3, " \
               "READ and CFRM as c4, CANCEL as c5, DUMP and check history"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # c1
        jobID = self.ns.submitJob( 'TEST', 'bla', '', '', 'c1', 's1' )

        # c2
        jobInfo = self.ns.getJob( 'TEST', -1, '', "",
                                        'c2', 's2' )
        receivedJobID = jobInfo[ 0 ]
        if receivedJobID != jobID:
            raise Exception( "Inconsistency" )
        self.ns.returnJob( 'TEST', jobID, jobInfo[ 1 ], 'c2', 's2' )

        # c3
        jobInfo = self.ns.getJob( 'TEST', -1, '', "", 'c3', 's3' )
        receivedJobID = jobInfo[ 0 ]
        if receivedJobID != jobID:
            raise Exception( "Inconsistency" )
        self.ns.putJob( 'TEST', jobID, jobInfo[ 1 ], 0, "", 'c3', 's3' )

        # c4
        readJobID, passport = self.ns.getJobForReading( 'TEST', 'c4', 's4' )
        if readJobID != jobID:
            raise Exception( "Inconsistency" )
        self.ns.confirmReading( 'TEST', jobID, passport, 'c4', 's4' )

        # c5
        self.ns.cancelJob( 'TEST', jobID, 'c5', 's5')

        raise Exception( "Not implemented" )


class Scenario238( TestBase ):
    " Scenario 238 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " Get affinites list "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario238' )
        getAffinityInfo( ns_client, False, 0 )
        return True

class Scenario239( TestBase ):
    " Scenario 239 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " Submit job with affinity a1, get affinites list "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario239' )
        self.ns.submitJob( 'TEST', 'bla', 'a1', '', 'mynode', 'mysession' )

        aff = getAffinityInfo( ns_client, False, 1, 0 )
        if aff[ 'number_of_jobs' ] != 1:
            raise Exception( "Expected 1 job, received: " + \
                             str( aff[ 'number_of_jobs' ] ) )
        if aff[ 'affinity_token' ] != 'a1':
            raise Exception( "Expected aff token: a1, received: " + \
                             aff[ 'affinity_token' ] )
        if aff[ 'number_of_clients__preferred' ] != 0:
            raise Exception( "Expected 0 preferred clients, received: " + \
                             str( aff[ 'number_of_clients__preferred' ] ) )
        if aff[ 'number_of_clients__explicit_wget' ] != 0:
            raise Exception( "Expected 0 WGET clients, received: " + \
                             str( aff[ 'number_of_clients__explicit_wget' ] ) )
        return True

class Scenario240( TestBase ):
    " Scenario 240 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " Submit job with affinity a1, get affinites list "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario240' )
        jobID = self.ns.submitJob( 'TEST', 'bla', 'a1', '', 'mynode', 'mysession' )

        aff = getAffinityInfo( ns_client, True, 1, 0 )
        if len( aff[ 'jobs' ] ) != 1:
            raise Exception( "Expected 1 job, received: " + \
                             str( len( aff[ 'jobs' ] ) ) )
        if aff[ 'jobs' ][ 0 ] != jobID:
            raise Exception( "Unexpected job for affinity. Expected: " + \
                             jobID + ", received: " + aff[ 'jobs' ][ 0 ] )
        if aff[ 'affinity_token' ] != 'a1':
            raise Exception( "Expected aff token: a1, received: " + \
                             aff[ 'affinity_token' ] )
        if aff[ 'clients__explicit_wget' ] is not None:
            raise Exception( "Expected 0 WGET clients, received: " + \
                             str( len( aff[ 'clients__explicit_wget' ] ) ) )
        if aff[ 'clients__preferred' ] is not None:
            raise Exception( "Expected 0 preferred clients, received: " + \
                             str( len( aff[ 'clients__preferred' ] ) ) )
        return True

class Scenario241( TestBase ):
    " Scenario 241 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF add=a2, STAT AFFINITIES VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario241' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        changeAffinity( ns_client, [ 'a2' ], [] )

        aff = getAffinityInfo( ns_client, True, 1, 0 )
        if aff[ 'affinity_token' ] != 'a2':
            raise Exception( "Expected aff token: a2, received: " + \
                             aff[ 'affinity_token' ] )
        if aff[ 'clients__explicit_wget' ] is not None:
            raise Exception( "Expected 0 WGET clients, received: " + \
                             str( len( aff[ 'clients__explicit_wget' ] ) ) )
        if len( aff[ 'clients__preferred' ] ) != 1:
            raise Exception( "Expected 1 preferred client, received: " + \
                             str( len( aff[ 'clients__preferred' ] ) ) )
        if aff[ 'clients__preferred' ][ 0 ] != 'mynode':
            raise Exception( "Unexpected preferred client. " \
                             "Expected 'mynode', received: '" + \
                             aff[ 'clients__preferred' ][ 0 ] + "'" )
        return True

class Scenario242( TestBase ):
    " Scenario 242 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF del=a3, STAT AFFINITIES VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario242' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        ns_client.on_warning = None

        changeAffinity( ns_client, [], [ 'a3' ] )

        aff = getAffinityInfo( ns_client, True, 0, 0 )
        return True

class Scenario243( TestBase ):
    " Scenario 243 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "CHAFF add=a4, CHAFF del=a4, STAT AFFINITIES VERBOSE"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario243' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        changeAffinity( ns_client, [ 'a4' ], [] )
        changeAffinity( ns_client, [], [ 'a4' ] )

        aff = getAffinityInfo( ns_client, True, 1, 0 )
        if aff[ 'jobs' ] is not None:
            raise Exception( "Expected no jobs, received: " + \
                             str( len( aff[ 'jobs' ] ) ) )

        if aff[ 'affinity_token' ] != 'a4':
            raise Exception( "Expected aff token: a4, received: " + \
                             aff[ 'affinity_token' ] )
        if aff[ 'clients__explicit_wget' ] is not None:
            raise Exception( "Expected 0 WGET clients, received: " + \
                             str( len( aff[ 'clients__explicit_wget' ] ) ) )
        if aff[ 'clients__preferred' ] is not None:
            raise Exception( "Expected 0 preferred clients, received: " + \
                             str( len( aff[ 'clients__preferred' ] ) ) )
        return True

class Scenario244( TestBase ):
    " Scenario 244 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " WGET a job with affinity a5, " \
               "get affinites list "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario244' )
        ns_client.set_client_identification( 'mynode', 'mysession' )

        execAny( ns_client, 'WGET 5000 10 aff=a5' )
        aff = getAffinityInfo( ns_client, True, 1, 0 )
        if aff[ 'affinity_token' ] != 'a5':
            raise Exception( "Expected aff token: a5, received: " + \
                             aff[ 'affinity_token' ] )
        if aff[ 'jobs' ] is not None:
            raise Exception( "Expected 0 job, received: " + \
                             str( len( aff[ 'jobs' ] ) ) )
        if len( aff[ 'clients__explicit_wget' ] ) != 1:
            raise Exception( "Expected 1 WGET clients, received: " + \
                             str( len( aff[ 'clients__explicit_wget' ] ) ) )
        if aff[ 'clients__explicit_wget' ][ 0 ] != 'mynode':
            raise Exception( "Expected explicit client 'mynode', received: " + \
                             aff[ 'clients__explicit_wget' ][ 0 ] )
        if aff[ 'clients__preferred' ] is not None:
            raise Exception( "Expected 0 preferred clients, received: " + \
                             str( len( aff[ 'clients__preferred' ] ) ) )
        return True

class Scenario245( TestBase ):
    " Scenario 245 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return " WGET a job with affinity a6, " \
               "wait till timeout, get affinites list "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario245' )

        self.ns.getJob( 'TEST', 10, 'a6', '', 'mynode', 'mysession' )
        time.sleep( 5 )

        aff = getAffinityInfo( ns_client, True, 1, 0 )
        if aff[ 'affinity_token' ] != 'a6':
            raise Exception( "Expected aff token: a6, received: " + \
                             aff[ 'affinity_token' ] )
        if aff[ 'jobs' ] is not None:
            raise Exception( "Expected 0 job, received: " + \
                             str( len( aff[ 'jobs' ] ) ) )
        if aff[ 'clients__explicit_wget' ] is not None:
            raise Exception( "Expected 0 WGET clients, received: " + \
                             str( len( aff[ 'clients__explicit_wget' ] ) ) )
        if aff[ 'clients__preferred' ] is not None:
            raise Exception( "Expected 0 preferred clients, received: " + \
                             str( len( aff[ 'clients__preferred' ] ) ) )
        return True

class Scenario246( TestBase ):
    " Scenario 246 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2 as anonymous"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario246' )
        try:
            execAny( ns_client,
                     'GET2 wnode_aff=0 any_aff=1' )
        except Exception, exc:
            if "Anonymous client" in str( exc ):
                return True
            raise
        return False

class Scenario247( TestBase ):
    " Scenario 247 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "PUT2 as anonymous"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario247' )
        try:
            execAny( ns_client, 'PUT2 ' + NON_EXISTED_JOB + ' passport 0 77' )
        except Exception, exc:
            if "Anonymous client" in str( exc ):
                return True
            raise
        return False

class Scenario248( TestBase ):
    " Scenario 248 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "FPUT2 as anonymous"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario248' )
        try:
            execAny( ns_client, 'FPUT2 ' + NON_EXISTED_JOB + ' passport err_msg out 1' )
        except Exception, exc:
            if "Anonymous client" in str( exc ):
                return True
            raise
        return False

class Scenario249( TestBase ):
    " Scenario 249 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "RETURN2 as anonymous"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario249' )
        try:
            execAny( ns_client, 'RETURN2 ' + NON_EXISTED_JOB + ' passport' )
        except Exception, exc:
            if "Anonymous client" in str( exc ):
                return True
            raise
        return False

class Scenario251( TestBase ):
    " Scenario 251 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET2 as identified, check passport"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob(  'TEST', 'bla', '', '', 'node', '000' )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario251' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=1' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID != receivedJobID:
            raise Exception( "Inconsistency" )

        if passport == "" or '_' not in passport:
            raise Exception( "Unexpected passport - empty or wrong format" )
        return True

class Scenario252( TestBase ):
    " Scenario 252 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET2, PUT2 as identified, check the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob(  'TEST', 'bla', '', '', 'node', '000' )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario252' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=1' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID != receivedJobID:
            raise Exception( "Inconsistency" )

        execAny( ns_client, 'PUT2 ' + jobID + ' ' + passport + ' 0 output' )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Done" or \
           status2 != "Done" or \
           status3 != "Done":
            return False
        return True

class Scenario253( TestBase ):
    " Scenario 253 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET2, FPUT2 as identified, check the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob(  'TEST', 'bla', '', '', 'node', '000' )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario253' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=1' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID != receivedJobID:
            raise Exception( "Inconsistency" )

        execAny( ns_client, 'FPUT2 ' + jobID + ' ' + passport + ' ' + \
                            'ErrMsg Output 2' )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Pending" or \
           status2 != "Pending" or \
           status3 != "Pending":
            return False
        return True

class Scenario254( TestBase ):
    " Scenario 254 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT, GET2, RETURN2 as identified, check the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob(  'TEST', 'bla', '', '', 'node', '000' )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario254' )
        ns_client.set_client_identification( 'mynode', 'mysession' )
        output = execAny( ns_client,
                          'GET2 wnode_aff=0 any_aff=1' )
        if '&' in output:
            values = parse_qs( output, True, True )
            receivedJobID = values[ 'job_key' ][ 0 ]
            passport = values[ 'auth_token' ][ 0 ]
        else:
            receivedJobID = output.split()[ 0 ].strip()
            passport = output.split( '"' )[ -1 ].strip().split()[ -1 ].strip()

        if jobID != receivedJobID:
            raise Exception( "Inconsistency" )

        execAny( ns_client, 'RETURN2 ' + jobID + ' ' + passport )

        # Check the status
        status1 = self.ns.getFastJobStatus( 'TEST', jobID )
        status2 = self.ns.getJobStatus( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        status3 = info[ "status" ]

        if status1 != "Pending" or \
           status2 != "Pending" or \
           status3 != "Pending":
            return False
        return True

class Scenario256( TestBase ):
    " Scenario 256 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "GET2, STAT NOTIFICATIONS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        # Spawn GET2 with waiting
        process = self.ns.spawnGet2Wait( 'TEST', 3,
                                         [], False, True,
                                         'node', 'session' )

        # STAT NOTIFICATIONS
        ns_client = self.getNetScheduleService( 'TEST', 'scenario227' )
        info = getNotificationInfo( ns_client, True, 1, 0 )
        process.wait()

        if info[ "use_preferred_affinities" ] != False:
            raise Exception( "Unexpected use_preferred_affinities" )
        if info[ "any_job" ] != True:
            raise Exception( "Unexpected any_job" )
        if info[ "slow_rate_active" ] != False:
            raise Exception( "Unexpected slow_rate_active" )
        if info[ 'active' ] != False:
            raise Exception( "Unexpected active" )

        return True

class Scenario257( TestBase ):
    " Scenario 257 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "STAT GROUPS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario257' )
        output = execAny( ns_client, 'STAT GROUPS' ).strip()
        if output != "END":
            raise Exception( "Expected no groups, received some" )
        return True

class Scenario258( TestBase ):
    " Scenario 258 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with group=000, STAT GROUPS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.submitJob( 'TEST', 'bla', "", "000", "", "" )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario258' )
        info = getGroupInfo( ns_client, False, 1, 0 )
        if info[ 'group' ] != '000':
            raise Exception( "Unexpected group" )
        if info[ 'number_of_jobs' ] != 1:
            raise Exception( "Unexpected number of jobs in the group" )
        return True

class Scenario259( TestBase ):
    " Scenario 259 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "BSUB 2 jobs with group=kt312a, STAT GROUPS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        self.ns.connect( 10 )
        self.ns.directLogin( 'TEST' )
        self.ns.directSendCmd( 'BSUB group=kt312a' )
        reply = self.ns.directReadSingleReply()
        if reply[ 0 ] != True or reply[ 1 ] != "Batch submit ready":
            self.ns.disconnect()
            raise Exception( "BSUB error" )

        self.ns.directSendCmd( 'BTCH 2' )
        self.ns.directSendCmd( 'bla1' )
        self.ns.directSendCmd( 'bla2' )
        self.ns.directSendCmd( 'ENDB' )
        self.ns.directSendCmd( 'ENDS' )
        reply = self.ns.directReadSingleReply()
        self.ns.disconnect()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario259' )
        info = getGroupInfo( ns_client, False, 1, 0 )
        if info[ 'group' ] != 'kt312a':
            raise Exception( "Unexpected group" )
        if info[ 'number_of_jobs' ] != 2:
            raise Exception( "Unexpected number of jobs in the group" )
        return True

class Scenario260( TestBase ):
    " Scenario 260 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with group=gita, BSUB 2 jobs with group=gita, " \
               "STAT GROUPS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        jobID = self.ns.submitJob(  'TEST', 'bla', '', 'gita', '', '' )

        self.ns.connect( 10 )
        self.ns.directLogin( 'TEST' )
        self.ns.directSendCmd( 'BSUB group=gita' )
        reply = self.ns.directReadSingleReply()
        if reply[ 0 ] != True or reply[ 1 ] != "Batch submit ready":
            self.ns.disconnect()
            raise Exception( "BSUB error" )

        self.ns.directSendCmd( 'BTCH 2' )
        self.ns.directSendCmd( 'bla1' )
        self.ns.directSendCmd( 'bla2' )
        self.ns.directSendCmd( 'ENDB' )
        self.ns.directSendCmd( 'ENDS' )
        reply = self.ns.directReadSingleReply()
        self.ns.disconnect()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario260' )
        info = getGroupInfo( ns_client, False, 1, 0 )
        if info[ 'group' ] != 'gita':
            raise Exception( "Unexpected group" )
        if info[ 'number_of_jobs' ] != 3:
            raise Exception( "Unexpected number of jobs in the group" )
        return True

class Scenario261( TestBase ):
    " Scenario 261 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT with group=0xFF, CANCEL the job, " \
               "STAT GROUPS, wait till jobs are wiped out, STAT GROUPS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID = self.ns.submitJob(  'TEST', 'bla', '', '0xFF', '', '' )
        ns_client = self.getNetScheduleService( 'TEST', 'scenario261' )
        info = getGroupInfo( ns_client, False, 1, 0 )
        if info[ 'group' ] != '0xFF':
            raise Exception( "Unexpected group" )
        if info[ 'number_of_jobs' ] != 1:
            raise Exception( "Unexpected number of jobs in the group" )

        self.ns.cancelJob( 'TEST', jobID )
        time.sleep( 40 )

        info = getGroupInfo( ns_client, False, 0, 0 )
        return True

class Scenario262( TestBase ):
    " Scenario 262 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT 3 jobs with group=111, GET 2 jobs, " \
               "PUT 1 job, STAT JOBS group=111"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        jobID1 = self.ns.submitJob(  'TEST', 'bla', '', '111', '', '' )
        jobID2 = self.ns.submitJob(  'TEST', 'bla', '', '111', '', '' )
        jobID3 = self.ns.submitJob(  'TEST', 'bla', '', '111', '', '' )

        j1 = self.ns.getJob( 'TEST' )
        self.ns.getJob( 'TEST' )

        self.ns.putJob( 'TEST', j1[ 0 ], j1[ 1 ], 0 )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario262' )

        info = ns_client.get_job_counters( None, "111" )
        if info[ 'Confirmed' ] != 0:
            raise Exception( "Unexpected number of confirmed jobs" )
        if info[ 'Total' ] != 3:
            raise Exception( "Unexpected number of total jobs" )
        if info[ 'Failed' ] !=  0:
            raise Exception( "Unexpected number of failed jobs" )
        if info[ 'ReadFailed' ] != 0:
            raise Exception( "Unexpected number of read failed jobs" )
        if info[ 'Canceled' ] != 0:
            raise Exception( "Unexpected number of canceled jobs" )
        if info[ 'Running' ] != 1:
            raise Exception( "Unexpected number of running jobs" )
        if info[ 'Done' ] != 1:
            raise Exception( "Unexpected number of done jobs" )
        if info[ 'Reading' ] != 0:
            raise Exception( "Unexpected number of reading jobs" )
        if info[ 'Pending' ] != 1:
            raise Exception( "Unexpected number of pending jobs" )
        return True

class Scenario263( TestBase ):
    " Scenario 263 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT 1 job with group=222, CANCEL group=222, " \
               "STAT JOBS group=222"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        jobID = self.ns.submitJob(  'TEST', 'bla', '', '222', '', '' )
        self.ns.cancelGroup( 'TEST', '222' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario263' )

        info = ns_client.get_job_counters( None, "222" )
        if info[ 'Confirmed' ] != 0:
            raise Exception( "Unexpected number of confirmed jobs" )
        if info[ 'Total' ] != 1:
            raise Exception( "Unexpected number of total jobs" )
        if info[ 'Failed' ] !=  0:
            raise Exception( "Unexpected number of failed jobs" )
        if info[ 'ReadFailed' ] != 0:
            raise Exception( "Unexpected number of read failed jobs" )
        if info[ 'Canceled' ] != 1:
            raise Exception( "Unexpected number of canceled jobs" )
        if info[ 'Running' ] != 0:
            raise Exception( "Unexpected number of running jobs" )
        if info[ 'Done' ] != 0:
            raise Exception( "Unexpected number of done jobs" )
        if info[ 'Reading' ] != 0:
            raise Exception( "Unexpected number of reading jobs" )
        if info[ 'Pending' ] != 0:
            raise Exception( "Unexpected number of pending jobs" )
        return True

class Scenario264( TestBase ):
    " Scenario 264 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT 1 job with group=222, SUBMIT 1 job with group=333, " \
               "GET a job, PUT the job, READ group=333, READ group=222"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        jobID1 = self.ns.submitJob(  'TEST', 'bla', '', '222', '', '' )
        jobID2 = self.ns.submitJob(  'TEST', 'bla', '', '333', '', '' )

        j1 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', j1[ 0 ], j1[ 1 ], 0 )

        r1, s, p  = self.ns.getJobsForReading2( 'TEST', -1, "333", "n", "s" )
        if r1 != "":
            raise Exception( "Expected no job, received: " + r1 )

        r2, s, p = self.ns.getJobsForReading2( 'TEST', -1, "222", "n", "s" )
        if r2 != jobID1:
            raise Exception( "Expected: " + jobID1 + ", got: " + r2 )
        return True

class Scenario265( TestBase ):
    " Scenario 265 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT 1 job with group=444, SUBMIT 1 job with group=555, " \
               "DUMP group=555"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        jobID1 = self.ns.submitJob(  'TEST', 'bla', '', '444', '', '' )
        jobID2 = self.ns.submitJob(  'TEST', 'bla', '', '555', '', '' )

        dump = self.ns.getQueueDump( 'TEST', '', '', 0, '555', '', '' )
        if "group: 2 ('555')" in dump:
            return True
        raise Exception( "DUMP did not provide the expected group 555" )

class Scenario266( TestBase ):
    " Scenario 266 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "DUMP group=777"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()

        ns_client = self.getNetScheduleService( 'TEST', 'scenario266' )
        output = execAny( ns_client, 'DUMP group=777', 0, True )
        if output:
            raise Exception( "Expected no output, received: " + str( output ) )
        return True

class Scenario267( TestBase ):
    " Scenario 267 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Provides the scenario "
        return "SUBMIT 4 jobs with all combinations of " \
               "aff a1,a2 and groups g1,g2, STAT JOBS aff=a2, " \
               "STAT JOBS aff=a2 group=g2"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.fromScratch()
        jobID1 = self.ns.submitJob(  'TEST', 'bla', 'a1', 'g1', '', '' )
        jobID2 = self.ns.submitJob(  'TEST', 'bla', 'a1', 'g2', '', '' )
        jobID3 = self.ns.submitJob(  'TEST', 'bla', 'a2', 'g1', '', '' )
        jobID4 = self.ns.submitJob(  'TEST', 'bla', 'a2', 'g2', '', '' )

        ns_client = self.getNetScheduleService( 'TEST', 'scenario267' )

        info = ns_client.get_job_counters( 'a2', None )
        if info[ 'Confirmed' ] != 0:
            raise Exception( "Unexpected number of confirmed jobs" )
        if info[ 'Total' ] != 2:
            raise Exception( "Unexpected number of total jobs" )
        if info[ 'Failed' ] !=  0:
            raise Exception( "Unexpected number of failed jobs" )
        if info[ 'ReadFailed' ] != 0:
            raise Exception( "Unexpected number of read failed jobs" )
        if info[ 'Canceled' ] != 0:
            raise Exception( "Unexpected number of canceled jobs" )
        if info[ 'Running' ] != 0:
            raise Exception( "Unexpected number of running jobs" )
        if info[ 'Done' ] != 0:
            raise Exception( "Unexpected number of done jobs" )
        if info[ 'Reading' ] != 0:
            raise Exception( "Unexpected number of reading jobs" )
        if info[ 'Pending' ] != 2:
            raise Exception( "Unexpected number of pending jobs" )

        info = ns_client.get_job_counters( 'a2', 'g2' )
        if info[ 'Confirmed' ] != 0:
            raise Exception( "Unexpected number of confirmed jobs" )
        if info[ 'Total' ] != 1:
            raise Exception( "Unexpected number of total jobs" )
        if info[ 'Failed' ] !=  0:
            raise Exception( "Unexpected number of failed jobs" )
        if info[ 'ReadFailed' ] != 0:
            raise Exception( "Unexpected number of read failed jobs" )
        if info[ 'Canceled' ] != 0:
            raise Exception( "Unexpected number of canceled jobs" )
        if info[ 'Running' ] != 0:
            raise Exception( "Unexpected number of running jobs" )
        if info[ 'Done' ] != 0:
            raise Exception( "Unexpected number of done jobs" )
        if info[ 'Reading' ] != 0:
            raise Exception( "Unexpected number of reading jobs" )
        if info[ 'Pending' ] != 1:
            raise Exception( "Unexpected number of pending jobs" )
        return True
