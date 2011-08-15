#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server wrapper
"""

import os, os.path, sys, pwd, socket, re
import time
import tempfile
from subprocess import Popen, PIPE


ps = "/bin/ps"
grep = "/bin/grep"
wc = "/usr/bin/wc"
killall = "/usr/bin/killall"
defaultVerbose = False



class NetSchedule:
    """ Represents a single netschedule server.
        It sometimes works directly with the server but in most of the cases
        uses a nice grid_cli utility """

    def __init__( self, host, port,
                        grid_cli_path = "",
                        netschedule_path = "",
                        db_path = "", **params ):
        " initializes the netschedule object "
        self.__host = host
        self.__port = port
        self.__path = netschedule_path.strip()
        self.__pid = -1

        self.__verbose = params.pop( "verbose", defaultVerbose )
        if params:
            raise Exception( "Unsupported configuration options %s",
                             list( params ) )

        # For direct connection
        self.__sock = None
        self.__readBuf = ""
        self.__replyDelimiter = re.compile( '\r\n|\n|\r|\0' )

        if not self.__path.endswith( os.path.sep ):
            self.__path += os.path.sep

        self.__grid_cli = grid_cli_path.strip()
        if not self.__grid_cli.endswith( os.path.sep ) and \
           self.__grid_cli != "":
            self.__grid_cli += os.path.sep
        self.__grid_cli += "grid_cli"

        self.__dbPath = db_path
        while self.__dbPath.endswith( os.path.sep ):
            self.__dbPath = self.__dbPath[ : -1 ]

        if netschedule_path != "":
            # Local NS is configured, so host must be local as well
            if not self.isLocal():
                raise Exception( "The loaclly configured NS must have host " \
                                 "argument equal to " \
                                 "'localhost' or '127.0.0.1'." )

        self.__checkPresence()
        return

    @staticmethod
    def __getUsername():
        " Provides the current user name "
        return pwd.getpwuid( os.getuid() )[ 0 ]

    @staticmethod
    def __safeRun( commandArgs ):
        " Provides the process stdout "
        errTmp = tempfile.mkstemp()
        errStream = os.fdopen( errTmp[ 0 ] )
        process = Popen( commandArgs, stdin = PIPE,
                         stdout = PIPE, stderr = errStream )
        process.stdin.close()
        processStdout = process.stdout.read()
        process.stdout.close()
        errStream.seek( 0 )
        err = errStream.read()
        errStream.close()
        process.wait()
        os.unlink( errTmp[ 1 ] )

        # 'grep' return codes:
        # 0 - OK, lines found
        # 1 - OK, no lines found
        # 2 - Error occured

        if process.returncode == 0 or \
           ( os.path.basename( commandArgs[ 0 ] ) == "grep" and \
             process.returncode == 1 ):
            # No problems, the ret code is 0 or the grep special case
            return processStdout

        # A problem has been identified
        if err.strip() == "":
            raise Exception( "Error in '%s' invocation. Return code: " + \
                             str( process.returncode ) )

        raise Exception( "Error in '%s' invocation: %s" % \
                         (commandArgs[0], err) )

    def __checkPresence( self ):
        " Checks that the given file exists on the remote host "

        # Check for grid_cli
        if os.system( self.__grid_cli + " help > /dev/null 2>&1" ) != 0:
            raise Exception( "Cannot find grid_cli at the " \
                             "specified location: " + self.__grid_cli )


        if self.__path == "":
            # Initialization is for a remote NS only
            return

        # Executable file
        cmdLine = [ "ls", self.__path + "netscheduled" ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        try:
            if self.__safeRun( cmdLine ).strip() != self.__path + \
                                                    "netscheduled":
                raise Exception()
        except:
            raise Exception( "netscheduled is not found on " + self.__host )

        # Config files
        cmdLine = [ "ls", self.__path + "netscheduled.ini.1" ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        try:
            if self.__safeRun( cmdLine ).strip() != self.__path + \
                                                    "netscheduled.ini.1":
                raise Exception()
        except:
            raise Exception( "netscheduled.ini.1 configuration " \
                             "is not found on " + self.__host )

        cmdLine = [ "ls", self.__path + "netscheduled.ini.2" ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        try:
            if self.__safeRun( cmdLine ).strip() != self.__path + \
                                                    "netscheduled.ini.2":
                raise Exception()
        except:
            raise Exception( "netscheduled.ini.2 configuration " \
                             "is not found on " + self.__host )

        # DB path
        dirname = os.path.dirname( self.__dbPath )
        if not os.path.exists( dirname ) or not os.path.isdir( dirname ):
            raise Exception( "DB path is invalid. The '" + dirname + \
                             "' expected to be an existing directory." )

        return

    def connect( self, timeout = None ):
        " Direct connect to NS "
        if self.__sock is not None:
            self.__sock.close()

        self.__sock = socket.socket()
        self.__sock.settimeout( timeout )
        self.__sock.connect( (self.__host, int( self.__port )) )
        return

    def disconnect( self ):
        " Close direct connection if it was opened "
        if self.__sock is not None:
            self.__sock.close()
        self.__sock = None
        self.__readBuf = ""
        return

    def directLogin( self, qname, auth = "netschedule_admin" ):
        " Performs a direct login to NS "
        if self.__sock is None:
            raise Exception( "Cannot login: socket was not created" )
        self.__sock.send( auth + "\n" + qname + "\n" )
        return

    def directSendCmd( self, cmd ):
        " Sends the given command to NS "
        if self.__sock is None:
            raise Exception( "Cannot send command: socket was not created" )
        self.__sock.send( cmd + "\n" )
        return

    def directReadSingleReply( self ):
        " Reads a single line reply "
        if self.__sock is None:
            raise Exception( "Cannot read reply: socket was not created" )

        while True:
            parts = self.__replyDelimiter.split( self.__readBuf, 1 )
            if len( parts ) > 1:
                reply = parts[ 0 ]
                self.__readBuf = parts[ 1 ]
                break
            buf = self.__sock.recv( 8192 )
            if not buf:
                if self.__readBuf:
                    return True, self.__readBuf.strip()
                return None, None
            self.__readBuf += buf

        if reply.startswith( "OK:" ):
            return True, reply.split( ':', 1 )[ 1 ].strip()
        if reply.startswith( "ERR:" ):
            return False, reply.split( ':', 1 )[ 1 ].strip()
        return True, reply.strip()

    def directReadMultiReply( self ):
        " Reads a multiple line reply "
        lines = []
        oneLine = self.directReadSingleReply()
        while oneLine[ 0 ] != False and \
              ( oneLine[ 0 ] != True or oneLine[ 1 ] != "END" ):
            if oneLine[ 1 ]:
                lines.append( oneLine[ 1 ] )
            oneLine = self.directReadSingleReply()

        if oneLine[ 0 ] == False:
            return oneLine
        return True, lines

    def __localOperationAssert( self ):
        " Used before executing any local operation "
        if self.__path == "":
            raise Exception( "Cannot perform this operation. " \
                             "NS is initialized as remote." )

    def setConfig( self, number = 1 ):
        " Copies the required config file to the current one "
        self.__localOperationAssert()

        fName = self.__path + "netscheduled.ini." + str( number )
        cmdLine = [ "ls", fName ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        if self.__safeRun( cmdLine ).strip() != fName:
            raise Exception( "Cannot find " + fName )

        cmdLine = [ "rm", "-rf", self.__path + "netscheduled.ini" ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )

        # Substitute DB path
        replaceWhat = "\\$DBPATH"
        replaceTo = self.__dbPath.replace( "/", "\\/" )
        cmdLine = "sed -e 's/" + replaceWhat + "/" + replaceTo + "/g' " + \
                  fName + " > " + self.__path + "netscheduled.ini.temp"
        if self.__verbose:
            print "Executing command: " + cmdLine
        retCode = os.system( cmdLine )
        if retCode != 0:
            raise Exception( "Error executing command: " + cmdLine )

        # Substitute port
        replaceWhat = "\\$PORT"
        replaceTo = str( self.__port )
        cmdLine = "sed -e 's/" + replaceWhat + "/" + replaceTo + "/g' " + \
                  self.__path + "netscheduled.ini.temp > " + \
                  self.__path + "netscheduled.ini"
        if self.__verbose:
            print "Executing command: " + cmdLine
        retCode = os.system( cmdLine )
        if retCode != 0:
            raise Exception( "Error executing command: " + cmdLine )

        return

    def isLocal( self ):
        " Returns True if it is a local netschedule "
        if self.__host == "localhost" or self.__host == "127.0.0.1":
            return True
        return False

    def isRunning( self ):
        " Provides True if the server is still running "
        if self.__path == "":
            # Check if it is running via opening a socket
            try:
                self.connect( 3 )
            except:
                return False
            return True

        # Here: local instance, use a pid

        if self.__pid != -1:
            # The NS pid is known, use it to check if it is still running
            cmdLine = [ ps, "-p", str( self.__pid ),
                        "|", grep, "netscheduled",
                        "|", grep, "-v", "grep",
                        "|", wc, "-l" ]
        else:
            cmdLine = [ ps, "-ef",
                        "|", grep, "netscheduled",
                        "|", grep, "conffile",
                        "|", grep, "logfile",
                        "|", grep, "-v", "grep",
                        "|", grep, self.__getUsername(),
                        "|", wc, "-l" ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        psProc = Popen( " ".join( cmdLine ), shell = True,
                        stdout = PIPE )
        output = psProc.stdout.read()
        psProc.stdout.close()
        psProc.wait()
        return output.strip() != "0"

    def start( self ):
        " starts netschedule "
        self.__localOperationAssert()

        if self.isRunning():
            return
        cmdLine = [ self.__path + "netscheduled",
                    "-conffile", self.__path + "netscheduled.ini",
                    "-logfile", self.__path + "netscheduled.log" ]

        count = 7
        while count > 0:
            if self.__verbose:
                print "Executing command: " + " ".join( cmdLine )
            try:
                self.__safeRun( cmdLine )
                if self.isRunning():
                    time.sleep( 5 )
                    self.__pid = self.__getNetschedulePID()
                    return
            except:
                if self.__verbose:
                    print >> sys.stderr, "Try #" + str( 8 - count ) + \
                                         " starting netschedule failed."
            time.sleep( 2 )
            count -= 1
        raise Exception( "Error starting netschedule" )

    def resetPID( self ):
        " Resets the PID after killing NS "
        self.__pid = -1
        return

    def __getNetschedulePID( self ):
        " provides the netschedule pid "
        cmdLine = [ ps, "-ef", "|", "grep", "netscheduled",
                    "|", grep, "conffile",
                    "|", grep, "logfile",
                    "|", grep, "-v", "grep",
                    "|", grep, self.__getUsername() ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        psProc = Popen( " ".join( cmdLine ), shell = True,
                        stdout = PIPE )
        output = psProc.stdout.read()
        psProc.stdout.close()
        psProc.wait()
        output = output.split( '\n' )[ 0 ].strip()

        return int( output.split()[ 1 ] )

    def kill( self, signal = "SIGKILL", timeout = 30 ):
        " kills netschedule "
        self.__localOperationAssert()

        if not self.isRunning():
            self.resetPID()
            return

        try:
            cmdLine = [ killall,
                        "-s", signal, "netscheduled" ]
            if self.__verbose:
                print "Executing command: " + " ".join( cmdLine )
            self.__safeRun( cmdLine )
        except:
            pass

        count = timeout
        while count > 0:
            if not self.isRunning():
                self.resetPID()
                return
            count -= 1
            time.sleep( 1 )
        raise Exception( "Could not kill netschedule on " + self.__host )

    def safeStop( self ):
        " Kills netschedule without exception "
        self.__localOperationAssert()

        try:
            self.kill( "SIGKILL", 7 )
        except:
            pass

        try:
            if self.isRunning():
                self.kill( "SIGTERM", 7 )
        except:
            pass

        if self.isRunning():
            print >> sys.stderr, "Warning: failed to stop netschedule daemon"
        self.resetPID()
        return

    def deleteDB( self ):
        " Deletes the data directory from the disk "
        self.__localOperationAssert()

        if self.isRunning():
            raise Exception( "Cannot delete data directory " \
                             "while netschedule is running" )

        cmdLine = [ "rm", "-rf", self.__dbPath ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )
        return

    def shutdown( self, auth = 'netschedule_admin' ):
        " Sends the shutdown command "
        if not self.isRunning():
            raise Exception( "Inconsistency: shutting down netschedule " \
                             "while it is not running" )

        cmdLine = [ self.__grid_cli, "shutdown",
                    "--auth=" + auth, "--compat-mode",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )
        return

    def getVersion( self ):
        " Gets the netschedule server version "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "serverinfo",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getActiveJobsCount( self, qname ):
        " Gets the active jobs count "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats", "--active-job-count",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return int( self.__safeRun( cmdLine ).strip() )

    def getQueueList( self ):
        " Get the list of queues "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "getqueuelist",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        queues = self.__safeRun( cmdLine ).split( '\n' )

        for index in range( len(queues) - 1, 0, -1 ):
            if queues[ index ].strip() == "":
                del queues[ index ]
        return queues

    def reconfigure( self ):
        " Makes netscheduled re-read its config file "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "reconf",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).split( '\n' )

    def getQueueInfo( self, qname ):
        " Provides information about the given queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "queueinfo", qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            result[ parts[ 0 ].strip() ] = parts[ 1 ].strip()
        return result

    def createQueue( self, qname, classname, comment = "" ):
        " Creates a dynamic queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "createqueue", qname, classname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if comment != "":
            cmdLine += [ "--queue-description=" + comment ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).split( '\n' )

    def deleteQueue( self, qname ):
        " Deletes a dynamic queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "deletequeue", qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )
        return

    def submitJob( self, qname, jobinput, affinity = "" ):
        " Submit a job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "submitjob",
                    "--queue=" + qname,
                    "--input=" + jobinput,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if affinity != "":
            cmdLine += [ "--affinity=" + affinity ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getFastJobStatus( self, qname, jobID ):
        " Gets the fast job status SST (Pending, Running etc)"
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", jobID,
                    "--status-only", "--defer-expiration",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getJobStatus( self, qname, jobID ):
        " Gets the job status (no lifetime change) WST (Pending, Running etc)"
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", jobID,
                    "--status-only", "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getJobBriefStatus( self, qname, jobID ):
        " Provides the brief job status STATUS "

        """ eg:
            Job number: 8
            Created by: iebdev4.be-md.ncbi.nlm.nih.gov:9101
            Status: Pending
            Input size: 5 """

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", "--brief", jobID,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            if len( parts ) < 2:
                raise Exception( "Unexpected job info format. " \
                                 "Expected: 'key: value', " \
                                 "Received: " + line )
            value = ":".join( parts[ 1: ] ).strip()
            result[ parts[0].strip() ] = value
        return result

    def cancelAllQueueJobs( self, qname, auth = "netschedule_admin" ):
        " removes all the jobs from the queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "deletequeue", qname, "--drop-jobs",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if auth != "":
            cmdLine += [ "--auth=" + auth ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )
        return

    def cancelJob( self, qname, jobid ):
        " Cancels the given job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "canceljob", jobid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )
        return

    def dropJob( self, qname, jobid ):
        " Drops the given job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "kill", jobid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )
        return

    def rescheduleJob( self, qname, jobid ):
        " Reschedules the job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob", "--force-reschedule",
                    "--queue=" + qname, jobid,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        self.__safeRun( cmdLine )
        return

    def getQueueConfiguration( self, qname ):
        " Provides the server configuration "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "getconf",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        params = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if ':' in line or line == "":
                continue
            parts = line.split( '=' )
            if len( parts ) != 2:
                raise Exception( "Unexpected output of the 'getconf' " \
                                 "command. 'key=value' format expected, " \
                                 "received: '" + line + "'" )
            params[ parts[0].strip() ] = parts[1].strip()

        return params

    def getJobInfo( self, qname, jobid ):
        " Provides the job information "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo",
                    "--queue=" + qname, jobid,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            if len( parts ) < 2:
                raise Exception( "Unexpected job info format. " \
                                 "Expected: 'key: value', " \
                                 "Received: " + line )
            value = ":".join( parts[ 1: ] ).strip()
            result[ parts[0].strip() ] = value
        return result

    def getStat( self, qname ):
        " Provides the queue statistics "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        recognized = [ 'Started', 'Pending', 'Running', 'Returned',
                       'Canceled', 'Failed', 'Done', 'Reading',
                       'Confirmed', 'ReadFailed', 'Timeout',
                       'ReadTimeout' ]

        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            should = False
            for item in recognized:
                if line.startswith( item + ":" ):
                    should = True
                    break
            if should:
                parts = line.split( ':' )
                if len( parts ) < 2:
                    raise Exception( "Unexpected job info format. " \
                                     "Expected: 'key: value', " \
                                     "Received: " + line )
                value = ":".join( parts[ 1: ] )
                value = value.strip()
                if value[ 0 ] == "'" and value[ len(value) - 1 ] == "'":
                    value = value[ 1:-1 ]
                result[ parts[0].strip() ] = value
            else:
                if "localhost UDP" in line:
                    # Worker node statistics should be included
                    parts = line.split( ' ' )
                    if len( parts ) != 6:
                        raise Exception( "Unexpected worker node description." \
                                         " Received: " + line )
                    result[ ' '.join( parts[0:4] ) ] = parts[4] + ' ' + parts[5]

        return result

    def getJob( self, qname, timeout = -1, port = -1, aff = '', guid = "" ):
        " Provides a job for execution "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "requestjob",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if timeout != -1:
            cmdLine += [ "--wait-timeout=" + str( timeout ) ]
        if port != -1:
            cmdLine += [ "--wnode-port=" + str( port ) ]
        if aff != '':
            cmdLine += [ "--affinity=" + aff ]
        if guid != "":
            cmdLine += [ "--wnode-guid=" + guid ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        output = self.__safeRun( cmdLine ).split( '\n' )

        if len( output ) == 0:
            return ""
        return output[ 0 ].strip()      # This is JOB ID

    def putJob( self, qname, jobID, retCode, out = "" ):
        " Commits the executed job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "commitjob",
                    "--queue=" + qname, jobID,
                    "--return-code=" + str( retCode ),
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if out != "":
            cmdLine += [ "--job-output=" + out ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def returnJob( self, qname, jobID ):
        " returns the job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "returnjob",
                    "--queue=" + qname, jobID,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def getServerInfo( self, qname ):
        " Provides the server information "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "serverinfo",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ":" )
            if len( parts ) != 2:
                continue
            result[ parts[0].strip() ] = parts[1].strip()
        return result

    def getQueueDump( self, qname, status = '' ):
        " Provides the server information "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "dumpqueue", "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if status != '':
            cmdLine += [ "--select-by-status=" + status ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = []
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            result.append( line )
        return result

    def getJobProgressMessage( self, qname, jobID ):
        " Provides the job progress message "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", "--queue=" + qname,
                    "--progress-message-only", jobID,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def setJobProgressMessage( self, qname, jobID, msg ):
        " Updates the job progress message "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob", "--queue=" + qname,
                    "--progress-message=" + msg, jobID,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )


    def failJob( self, qname, jobID, retCode, output = "", errmsg = "" ):
        " Fails the given job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "commitjob", "--queue=" + qname,
                    jobID, "--fail-job=" + errmsg,
                    "--return-code=" + str( retCode ),
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if output != "":
            cmdLine += [ "--job-output=" + output ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def registerWorkerNode( self, qname, port ):
        " Registers the worker node "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--register-wnode", "--wnode-port=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def unregisterWorkerNode( self, qname, port ):
        " Unregisters the worker node "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--unregister-wnode", "--wnode-port=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def getWorkerNodeStat( self, qname ):
        " Provides the worker nodes statistics attached to the queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--worker-nodes",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = []
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            result.append( line )
        return result

    def extendJobExpiration( self, qname, jobid, timeout ):
        " Extends the given job expiration time "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob",
                    jobid, "--queue=" + qname,
                    "--extend-lifetime=" + str( timeout ),
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def exchangeJob( self, qname, jobID, retCode, output, aff = '' ):
        " Exchanges the job for another "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "commitjob",
                    jobID, "--queue=" + qname,
                    "--get-next-job", "--job-output=" + str( output ),
                    "--return-code=" + str( retCode ),
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if aff != '':
            cmdLine += [ "--affinity=" + aff ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def getAffinityStatus( self, qname, affinity ):
        " Provides the affinity status "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--queue=" + qname,
                    "--jobs-by-status", "--affinity=" + affinity,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            result[ parts[0].strip() ] = int( parts[1].strip() )
        return result

    def getAffinityList( self, qname ):
        " Provides the affinity list "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--queue=" + qname, "--jobs-by-affinity",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            result[ parts[0].strip() ] = int( parts[1].strip() )
        return result

    def initWorkerNode( self, qname, port ):
        " new style of registering worker nodes "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--register-wnode=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def resetWorkerNode( self, qname, guid ):
        " De-registers the worker node "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--unregister-wnode=" + guid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def getJobsForReading( self, qname, count, timeout = -1 ):
        " Requests jobs for reading "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "readjobs",
                    "--limit=" + str( count ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if timeout != -1:
            cmdLine += [ "--timeout=" + str( timeout ) ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        groupID = -1
        jobs = []
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            if groupID == -1:
                groupID = int( line )
                continue
            jobs.append( line )
        return groupID, jobs

    def confirmJobRead( self, qname, groupID, jobs ):
        " Confirms the fact that jobs have been read "
        if len( jobs ) <= 0:
            raise Exception( "Jobs list expected" )

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "readjobs",
                    "--confirm-read=" + str( groupID ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        for jobid in jobs:
            cmdLine.append( "--job-id=" + jobid )

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def rollbackJobRead( self, qname, groupID, jobs ):
        " Rollbacks reading jobs "
        if len( jobs ) <= 0:
            raise Exception( "Jobs list expected" )

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "readjobs",
                    "--rollback-read=" + str( groupID ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        for jobid in jobs:
            cmdLine.append( "--job-id=" + jobid )

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )


    def failJobReading( self, qname, groupID, jobs, err_msg ):
        " Fails the jobs reading "
        if len( jobs ) <= 0:
            raise Exception( "Jobs list expected" )

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "readjobs",
                    "--fail-read=" + str( groupID ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        for jobid in jobs:
            cmdLine.append( "--job-id=" + jobid )

        if err_msg != "":
            cmdLine += [ "--error-message=" + err_msg ]

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )
        return self.__safeRun( cmdLine )

    def submitBatch( self, qname, jobs ):
        " Performs a batch submit "
        if len( jobs ) <= 0:
            raise Exception( "Jobs list expected" )

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        # Save the jobs in a temporary file
        f, filename = tempfile.mkstemp()
        for job in jobs:
            os.write( f, "input=" + job + "\n" )
        os.close( f )

        try:
            # Execute the command
            cmdLine = [ self.__grid_cli, "submitjob",
                        "--batch=" + str( len(jobs) ),
                        "--input-file=" + filename,
                        "--queue=" + qname,
                        "--ns=" + self.__host + ":" + str( self.__port ) ]
            jobids = self.__safeRun( cmdLine )

            # Remove the temporary file
        except:
            os.unlink( filename )
            raise

        os.unlink( filename )
        return jobids

