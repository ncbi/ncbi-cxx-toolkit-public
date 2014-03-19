#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server tests pack
"""

import time
from ncbi_grid_1_0.ncbi.grid import ns, ipc

class TestBase:
    " Base class for tests "

    def getNetScheduleService( self, queueName, clientName ):
        return ns.NetScheduleService( self.ns.getHost() + ":" +
                                        str( self.ns.getPort() ),
                                        queueName, clientName,
#                                        rpc_server=ipc.RPCServer(protocol_dump_file='/export/home/satskyse/ns_testsuite/protocol_dump.txt')
                                    )

    def __init__( self, netschedule ):
        self.ns = netschedule
        return

    def getScenarioID( self ):
        " Should return unique integer "
        nameParts = str( self.__class__ ).split( '.' )

        count = len( nameParts )
        if count == 0:
            raise Exception( "Unexpected test class name" )

        className = nameParts[ count - 1 ]
        if not className.startswith( 'Scenario' ):
            raise Exception( "Tests class names must follow 'ScenarioXX' " \
                             "rule where XX is an integer number." )

        testID = className.replace( 'Scenario', '' )
        try:
            return int( testID )
        except:
            pass
        raise Exception( "Tests class names must follow 'ScenarioXX' rule " \
                         "where XX is an integer number." )

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        raise Exception( "Not implemented" )

    def execute( self ):
        " Should return True if the execution completed successfully "
        raise Exception( "Not implemented" )

    def clear( self, configID = 1 ):
        " Stops and cleans everything "
        self.ns.kill( "SIGKILL" )
        self.ns.deleteDB()
        self.ns.setConfig( configID )
        return

    def fromScratch( self, configID = 1 ):
        " Starts a fresh copy of ns "
        self.clear( configID )
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )
        return



class Scenario00( TestBase ):
    " Scenario 0 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start NS, kill NS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.kill( "SIGTERM" )
        time.sleep( 1 )
        return not self.ns.isRunning()


class Scenario01( TestBase ):
    " Scenario 1 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start NS, shutdown NS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.shutdown()
        time.sleep( 6 )
        return not self.ns.isRunning()


class Scenario02( TestBase ):
    " Scenario 2 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start NS, kill -9 NS, start NS"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.kill( "SIGKILL" )
        time.sleep( 1 )
        self.ns.start()
        time.sleep( 1 )
        return self.ns.isRunning()


class Scenario03( TestBase ):
    " Scenario 3 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return
 
    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start NS, shutdown NS with the 'nobody' account"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        # This should not shutdown netschedule due to the authorization
        try:
            self.ns.shutdown( 'nobody' )
        except Exception, exc:
            if "Access denied: admin privileges required" in str( exc ) or \
               "Access denied:  admin privileges required" in str( exc ):
                return True
            raise
        return self.ns.isRunning()


class Scenario04( TestBase ):
    " Scenario 4 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting netschedule version"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        return "version" in self.ns.getVersion().lower()


class Scenario05( TestBase ):
    " Scenario 5 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting the number of active jobs"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        return self.ns.getActiveJobsCount( 'TEST' ) == 0


class Scenario06( TestBase ):
    " Scenario 6 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting the list of the configured queues"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        queues = self.ns.getQueueList()
        return len( queues ) == 1 and queues[ 0 ] == "TEST"


class Scenario08( TestBase ):
    " Scenario 8 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting non existed queue"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        try:
            self.ns.getQueueInfo( 'not_existed' )
        except Exception, exc:
            if "not found" in str( exc ):
                return True
            raise

        return False


class Scenario09( TestBase ):
    " Scenario 9 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting existing queue info"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        qinfo = self.ns.getQueueInfo( 'TEST' )
        if qinfo.has_key( "queue_type" ):
            return qinfo[ "queue_type" ] == "static"
        if qinfo.has_key( "queue type" ):
            return qinfo[ "queue type" ] == "static"
        if qinfo.has_key( "kind" ):
            return qinfo[ "kind" ] == "static"
        raise Exception( "No queue type found" )


class Scenario11( TestBase ):
    " Scenario 11 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submitting a single job and checking the number of " \
               "active jobs and the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )


        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        return self.ns.getFastJobStatus( 'TEST', jobID ) == 'Pending'


class Scenario12( TestBase ):
    " Scenario 12 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "submit a job, drop all the jobs, get the job fast status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( "TEST", 'bla' )
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        self.ns.cancelAllQueueJobs( "TEST" )
        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        return self.ns.getFastJobStatus( 'TEST', jobID ) == "Canceled"


class Scenario13( TestBase ):
    " Scenario 13 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "submit a job, cancel it"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )


        jobID = self.ns.submitJob( "TEST", 'bla' )
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False
        if self.ns.getFastJobStatus( 'TEST', jobID ) != "Pending":
            return False

        self.ns.cancelJob( "TEST", jobID )
        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        return self.ns.getFastJobStatus( 'TEST', jobID ) == "Canceled"


class Scenario16( TestBase ):
    " Scenario 16 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting a queue configuration"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        params = self.ns.getQueueInfo( 'TEST' )
        if params[ 'timeout' ] != "30" and params[ 'timeout' ] != "30.0":
            raise Exception( "Unexpected timeout parameter value" )
        if params[ 'run_timeout' ] != "7" and params[ 'run_timeout' ] != "7.0":
            raise Exception( "Unexpected run_timeout parameter value" )

        return params[ 'failed_retries' ] == "3"



class Scenario17( TestBase ):
    " Scenario 17 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, stop NS, start NS, check the job status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.submitJob( "TEST", 'bla' )
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        self.ns.shutdown()
        self.ns.resetPID()
        time.sleep( 5 )
        if self.ns.isRunning():
            raise Exception( "Cannot shutdown netschedule" )

        self.ns.start()
        time.sleep( 5 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        return self.ns.getActiveJobsCount( 'TEST' ) == 1


class Scenario18( TestBase ):
    " Scenario 18 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job info. " \
               "It will fail until NS is fixed."

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( "TEST", 'test input' )
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )

        # Check input
        if info.has_key( 'input_data' ):
            if info[ 'input_data' ] != "'D test input'":
                raise Exception( "Unexpected input (old format)" )
        elif info.has_key( 'embedded_input_data' ):
            if info[ 'embedded_input_data' ] != '"test input"':
                raise Exception( "Unexpected input (new format)" )
        else:
            raise Exception( "Not found output field" )

        if info[ 'status' ] != 'Pending' or \
           info[ 'progress_msg' ] != "''" or \
           info[ 'mask' ] != "0" or \
           info[ 'run_counter' ] != "0" or \
           info[ 'input_storage' ] != "embedded, size=10":
            return False

        if info.has_key( 'aff_id' ):
            return info[ 'aff_id' ] == "0"

        # NS 4.10.0 and up has different output
        return info[ 'affinity' ] == "n/a"


class Scenario22( TestBase ):
    " Scenario 22 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get statistics"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobID = self.ns.submitJob( "TEST", 'bla' )
        jobID = jobID   # pylint is happy
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        stats = self.ns.getStat( "TEST" )
        return stats[ 'Pending' ] == "1" and \
               stats[ 'Running' ] == "0" and \
               stats[ 'Canceled' ] == "0" and \
               stats[ 'Failed' ] == "0" and \
               stats[ 'Done' ] == "0" and \
               stats[ 'Reading' ] == "0" and \
               stats[ 'Confirmed' ] == "0" and \
               stats[ 'ReadFailed' ] == "0"


class Scenario23( TestBase ):
    " Scenario 23 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job, check the active job number, " \
               "get the job status, get the job dump"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            print "0"
            return False

        jobID = self.ns.submitJob( "TEST", 'bla' )
        jobIDReceived = self.ns.getJob( 'TEST' )[ 0 ]

        if jobID != jobIDReceived:
            return False
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        status = self.ns.getJobBriefStatus( 'TEST', jobID )

        if status.has_key( "Status" ):
            if status[ "Status" ] != "Running":
                return False
        elif status.has_key( "status" ):
            if status[ "status" ] != "Running":
                return False
        else:
            raise Exception( "status field is not found" )

        if self.ns.getFastJobStatus( 'TEST', jobID ) != "Running":
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ "run_counter" ] != "1" or info[ "status" ] != "Running":
            return False

        return True


class Scenario25( TestBase ):
    " Scenario 25 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job for execution, " \
               "return the job, check status"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobID = self.ns.submitJob( "TEST", 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        jobIDReceived = jobInfo[ 0 ]

        if jobID != jobIDReceived:
            return False

        self.ns.returnJob( 'TEST', jobID, jobInfo[ 1 ] )

        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ "status" ] != "Pending" or getRetCode( info ) != "0":
            return False
        return True


class Scenario26( TestBase ):
    " Scenario 26 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting server info"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        info = self.ns.getServerInfo( 'TEST' )
        if info.has_key( "max_output_size" ):
            return info.has_key( "max_input_size" )
        else:
            return info.has_key( "Maximum output size" ) and \
                   info.has_key( "Maximum input size" )


class Scenario27( TestBase ):
    " Scenario 27 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Get queue info, submit a job, get queue info, " \
               "get the job, get queue info"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        dump = self.ns.getQueueDump( 'TEST', 'Pending' )
        if len( dump ) != 0:
            return False

        jobID1 = self.ns.submitJob( "TEST", '111' )
        jobID2 = self.ns.submitJob( "TEST", '222' )
        jobID2 = jobID2     # pylint is happy

        dump = self.ns.getQueueDump( 'TEST', 'Pending' )
        if dump.count( 'status: Pending' ) != 2:
            raise Exception( "Unexpected number of jobs in dump" )

        jobIDReceived = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID1 != jobIDReceived:
            return False

        dump = self.ns.getQueueDump( 'TEST', 'Pending' )
        if dump.count( 'status: Pending' ) != 1:
            return False

        dump = self.ns.getQueueDump( 'TEST', 'Running' )
        if dump.count( 'status: Running' ) != 1:
            return False

        return True


class Scenario30( TestBase ):
    " Scenario 30 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting a job progress message when there is no such job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        try:
            msg = self.ns.getJobProgressMessage( 'TEST',
                                                 'JSID_01_7_130.14.24.83_9101' )
            msg = msg   # pylint is happy
        except Exception, exc:
            if 'Job not found' in str( exc ) or \
                'eJobNotFound' in str( exc ):
                return True
            raise
        return False


class Scenario31( TestBase ):
    " Scenario 31 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Setting a job progress message when there is no such job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        try:
            self.ns.setJobProgressMessage( 'TEST',
                                           'JSID_01_7_130.14.24.83_9101',
                                           'lglglg' )
        except Exception, exc:
            if 'Job not found' in str( exc ) or \
               'eJobNotFound' in str( exc ):
                return True
            raise
        return False


class Scenario32( TestBase ):
    " Scenario 32 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, check its progress message, " \
               "update the job progress message, check the progress " \
               "message"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobID = self.ns.submitJob( "TEST", 'bla' )
        msg = self.ns.getJobProgressMessage( 'TEST', jobID )
        if msg != "":
            return False

        self.ns.setJobProgressMessage( 'TEST', jobID, 'lglglg' )
        msg = self.ns.getJobProgressMessage( 'TEST', jobID )
        if msg != "lglglg":
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        return info[ 'progress_msg' ] == "'lglglg'"



class Scenario33( TestBase ):
    " Scenario 33 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, check its progress message, get the job, " \
               "update the job progress message, check the progress " \
               "message"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobID = self.ns.submitJob( "TEST", 'bla' )
        msg = self.ns.getJobProgressMessage( 'TEST', jobID )
        if msg != "":
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ "progress_msg" ] != "''":
            return False

        self.ns.getJob( 'TEST' )

        self.ns.setJobProgressMessage( 'TEST', jobID, 'lglglg' )
        msg = self.ns.getJobProgressMessage( 'TEST', jobID )
        if msg != "lglglg":
            return False
        return True


class Scenario34( TestBase ):
    " Scenario 34 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job, " \
               "fail the job, check the job info. " \
               "It will fail until NS is fixed."

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobID = self.ns.submitJob( "TEST", 'bla' )
        jobInfo = self.ns.getJob( 'TEST' )
        self.ns.failJob( 'TEST', jobID, jobInfo[1], 255,
                         "Test output", "Test error message" )

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ 'status' ] != 'Pending':
            raise Exception( "Unexpected status" )
        if info[ 'run_counter' ] != '1':
            raise Exception( "Unexpected run counter" )
        if getRetCode( info ) != '255':
            raise Exception( "Unexpected return code" )
        if 'Test output' not in info[ 'embedded_output_data' ]:
            raise Exception( "Unexpected output data" )
        if 'Test error message' not in getErrMsg( info ):
            raise Exception( "Unexpected error message" )

        return True

class Scenario36( TestBase ):
    " Scenario 36 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, submit another job, get the first job, " \
               "get the second job, check status of both jobs."

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID1 = self.ns.submitJob( 'TEST', 'bla' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla' )

        receivedJobID1 = self.ns.getJob( 'TEST' )[ 0 ]
        if jobID1 != receivedJobID1:
            return False

        receivedJobID2 = self.ns.getJob( 'TEST' )[ 0 ]
        if not receivedJobID2.startswith( jobID2 ):
            return False

        info1 = self.ns.getJobInfo( 'TEST', jobID1 )
        if info1[ 'status' ] != 'Running' or \
           info1[ 'run_counter' ] != '1':
            return False

        info2 = self.ns.getJobInfo( 'TEST', jobID2 )
        if info2[ 'status' ] != 'Running' or \
           info2[ 'run_counter' ] != '1':
            return False
        return True


class Scenario37( TestBase ):
    " Scenario 37 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Get status of non-existing affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        try:
            self.ns.getAffinityStatus( 'TEST', 'non_existing_affinity' )
        except Exception, exc:
            if "Unknown affinity token" in str( exc ):
                return True
            raise
        return False


class Scenario38( TestBase ):
    " Scenario 38 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job with affinity, get status of the affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.submitJob( 'TEST', 'bla', 'myaffinity' )
        status = self.ns.getAffinityStatus( 'TEST', 'myaffinity' )

        if status[ "Pending" ] == 1:
            return True

        return False


class Scenario39( TestBase ):
    " Scenario 39 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job with one affinity, " \
               "submit a job with another affinity, " \
               "get the list of affinities"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.submitJob( 'TEST', 'bla', 'myaffinity1' )
        self.ns.submitJob( 'TEST', 'bla', 'myaffinity2' )

        affList = self.ns.getAffinityList( 'TEST' )
        if affList[ "myaffinity1" ][0] == 1 and \
           affList[ "myaffinity2" ][0] == 1:
            return True
        return False


class Scenario40( TestBase ):
    " Scenario 40 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job without affinity, " \
               "submit a job with an affinity, " \
               "get the job with affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID1 = self.ns.submitJob( 'TEST', 'bla' )
        jobID1 = jobID1     # pylint is happy
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'aff0' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 'aff0' )[ 0 ]
        if jobID2 != receivedJobID:
            return False

        info = self.ns.getJobInfo( 'TEST', jobID2 )
        if info[ 'status' ] != 'Running' or \
           info[ 'run_counter' ] != '1':
            return False

        return True


class Scenario41( TestBase ):
    " Scenario 41 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job with an affinity, " \
               "get a job with another affinity"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( 'TEST', 'bla', 'aff0' )
        jobID = jobID   # pylint is happy

        try:
            self.ns.getJob( 'TEST', -1, 'other_affinity' )
        except Exception, exc:
            if "NoJobsWithAffinity" in str( exc ):
                raise Exception( "Old style NS exception answer: " \
                                 "NoJobsWithAffinity" )
            raise

        return True


class Scenario46( TestBase ):
    " Scenario 46 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Dump the queue, submit 2 jobs, get one job for execution, " \
               "dump the queue"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        info = self.ns.getQueueDump( 'TEST' )
        # Check that there are no jobs

        jobID1 = self.ns.submitJob( 'TEST', 'bla' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla' )
        jobID2 = jobID2     # pylint is happy

        receivedJobID = self.ns.getJob( 'TEST' )[ 0 ]
        if receivedJobID != jobID1:
            return False

        info = self.ns.getQueueDump( 'TEST' )
        # Check that one job is in the Running section
        # and another is in the Pending section
        statuses = []
        for item in info:
            if item.startswith( 'status:' ):
                statuses.append( item )

        statuses.sort()
        if statuses[ 0 ] != 'status: Pending' or \
           statuses[ 1 ] != 'status: Running':
            return False

        return True


class Scenario50( TestBase ):
    " Scenario 50 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get job for execution, " \
               "commit the job, get the job for reading, " \
               "check the job run counter. "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )


        jobID1 = self.ns.submitJob( 'TEST', 'bla1' )
        jinfo = self.ns.getJob( 'TEST' )
        receivedJobID1 = jinfo[ 0 ]
        self.ns.putJob( 'TEST', receivedJobID1, jinfo[ 1 ], 0, 'bla' )

        jobID, jobState, jobPassport = self.ns.getJobsForReading2( 'TEST' )
        if jobID1 != jobID:
            raise Exception( "Unexpected job ID" )

        info = self.ns.getJobInfo( 'TEST', jobID1 )
        if info[ 'run_counter' ] != '1':
            return False
        # Will fail until NS is fixed: run_counter is mistakenly 2 here
        return True


class Scenario52( TestBase ):
    " Scenario 52 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start batch submit, submit two jobs"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobIDs = self.ns.submitBatch( 'TEST', [ '"bla1"', '"bla2"' ] )
        if len( jobIDs ) != 2:
            return False

        info = self.ns.getQueueDump( 'TEST' )
        statuses = []
        for item in info:
            if item.startswith( 'status:' ):
                statuses.append( item )

        if len( statuses ) != 2:
            return False

        if statuses[ 0 ] != 'status: Pending' or \
           statuses[ 1 ] != 'status: Pending':
            return False

        return True


class Scenario53( TestBase ):
    " Scenario 53 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start batch submit, tell that there will be 2 jobs but " \
               "submit one job"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.connect( 10 )
        self.ns.directLogin( 'TEST' )
        self.ns.directSendCmd( 'BSUB' )
        reply = self.ns.directReadSingleReply()
        if reply[ 0 ] != True or reply[ 1 ] != "Batch submit ready":
            self.ns.disconnect()
            return False

        self.ns.directSendCmd( 'BTCH 2' )
        self.ns.directSendCmd( 'bla1' )
        self.ns.directSendCmd( 'ENDB' )
        self.ns.directSendCmd( 'ENDS' )

        reply = self.ns.directReadSingleReply()
        if reply[ 0 ] != False or \
            not reply[ 1 ].startswith( 'eProtocolSyntaxError' ):
            self.ns.disconnect()
            return False

        return True



class Scenario54( TestBase ):
    " Scenario 54 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start batch submit, tell that there will be 1 job but " \
               "submit two jobs"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.connect( 10 )
        self.ns.directLogin( 'TEST' )
        self.ns.directSendCmd( 'BSUB' )
        reply = self.ns.directReadSingleReply()
        if reply[ 0 ] != True or reply[ 1 ] != "Batch submit ready":
            self.ns.disconnect()
            return False

        self.ns.directSendCmd( 'BTCH 1' )
        self.ns.directSendCmd( 'bla1' )
        self.ns.directSendCmd( 'bla2' )
        reply = self.ns.directReadSingleReply()
        if reply[ 0 ] != False or \
            not reply[ 1 ].startswith( 'eProtocolSyntaxError' ):
            self.ns.disconnect()
            return False

        return True


class Scenario55( TestBase ):
    " Scenario 55 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job for execution, fail the job, " \
               "get the job fast status. "

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        jInfo = self.ns.getJob( 'TEST' )
        receivedJobID = jInfo[ 0 ]
        if receivedJobID != jobID:
            raise Exception( "Unexpected job ID" )

        self.ns.failJob( 'TEST', jobID, jInfo[1], 1, "error-output" )
        status = self.ns.getFastJobStatus( 'TEST', jobID )
        if status != "Pending":
            raise Exception( "Unexpected job state" )
        return True


def findLastEventIndex( info ):
    " Searches for the last event index "
    if info.has_key( "attempt_counter" ):
        return int( info[ "attempt_counter" ] )
    if info.has_key( "event_counter" ):
        return int( info[ "event_counter" ] )

    # need to detect the index
    index = -1
    for key in info:
        if key.startswith( "event" ):
            key = key.replace( "event", "" )
            candidateIndex = int( key )
            if candidateIndex > index:
                index = candidateIndex
    return index


def getRetCode( info ):
    " Provides the ret code for both, old and new NS output "
    if info.has_key( "ret_code" ):
        # Old format
        return info[ "ret_code" ]

    # New format
    if info.has_key( "attempt_counter" ):
        lastAttempt = info[ "attempt_counter" ]
        attemptLine = info[ "attempt" + lastAttempt ]
    elif info.has_key( "event_counter" ):
        lastAttempt = info[ "event_counter" ]
        attemptLine = info[ "event" + lastAttempt ]
    else:
        # need to detect the last event
        lastIndex = findLastEventIndex( info )
        if lastIndex == -1:
            raise Exception( "Unknown DUMP <job> format; "
                             "cannot find last event index" )
        attemptLine = info[ "event" + str( lastIndex ) ]

    parts = attemptLine.split( ' ' )
    foundIndex = -1
    for index in xrange( len( parts ) ):
        if parts[ index ].startswith( "ret_code=" ):
            foundIndex = index
            break

    if foundIndex == -1:
        raise Exception( "Unknown DUMP <job> format" )

    value = parts[ foundIndex ].replace( "ret_code=", "" )
    return value.strip()


def getErrMsg( info ):
    " Provides the error message for both, old and new NS output "
    if info.has_key( "err_msg" ):
        # Old format
        return info[ "err_msg" ]

    # New format
    if info.has_key( "attempt_counter" ):
        lastAttempt = info[ "attempt_counter" ]
        attemptLine = info[ "attempt" + lastAttempt ]
    elif info.has_key( "event_counter" ):
        lastAttempt = info[ "event_counter" ]
        attemptLine = info[ "event" + lastAttempt ]
    else:
        # need to detect the last event
        lastIndex = findLastEventIndex( info )
        if lastIndex == -1:
            raise Exception( "Unknown DUMP <job> format; "
                             "cannot find last event index" )
        attemptLine = info[ "event" + str( lastIndex ) ]

    parts = attemptLine.split( 'err_msg=' )
    value = parts[ 1 ].strip()
    return value


