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
                raise Exception( "The locally configured NS must have host " \
                                 "argument equal to " \
                                 "'localhost' or '127.0.0.1'." )

        self.__checkPresence()
        return

    def getGridCLIPath( self ):
        return self.__grid_cli

    def verbosePrint( self, cmdLine ):
        " Prints the command line if verbose is True "
        if self.__verbose:
            if type( cmdLine ) == type( 'a' ):
                print "Executing command: " + cmdLine
            else:
                print "Executing command: " + " ".join( cmdLine )
        return

    def getHost( self ):
        return self.__host

    def getPort( self ):
        return self.__port

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
        self.verbosePrint( cmdLine )
        try:
            if self.__safeRun( cmdLine ).strip() != self.__path + \
                                                    "netscheduled":
                raise Exception()
        except:
            raise Exception( "netscheduled is not found on " + self.__host )

        # Config files
        cmdLine = [ "ls", self.__path + "netscheduled.ini.1" ]
        self.verbosePrint( cmdLine )
        try:
            if self.__safeRun( cmdLine ).strip() != self.__path + \
                                                    "netscheduled.ini.1":
                raise Exception()
        except:
            raise Exception( "netscheduled.ini.1 configuration " \
                             "is not found on " + self.__host )

        cmdLine = [ "ls", self.__path + "netscheduled.ini.2" ]
        self.verbosePrint( cmdLine )
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
        self.verbosePrint( cmdLine )
        if self.__safeRun( cmdLine ).strip() != fName:
            raise Exception( "Cannot find " + fName )

        cmdLine = [ "rm", "-rf", self.__path + "netscheduled.ini" ]
        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )

        # Substitute DB path
        replaceWhat = "\\$DBPATH"
        replaceTo = self.__dbPath.replace( "/", "\\/" )
        cmdLine = "sed -e 's/" + replaceWhat + "/" + replaceTo + "/g' " + \
                  fName + " > " + self.__path + "netscheduled.ini.temp"
        self.verbosePrint( cmdLine )
        retCode = os.system( cmdLine )
        if retCode != 0:
            raise Exception( "Error executing command: " + cmdLine )

        # Substitute port
        replaceWhat = "\\$PORT"
        replaceTo = str( self.__port )
        cmdLine = "sed -e 's/" + replaceWhat + "/" + replaceTo + "/g' " + \
                  self.__path + "netscheduled.ini.temp > " + \
                  self.__path + "netscheduled.ini"
        self.verbosePrint( cmdLine )
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

        self.verbosePrint( cmdLine )

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
            self.verbosePrint( cmdLine )
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
            self.kill( "SIGTERM", 7 )
        except:
            pass

        try:
            if self.isRunning():
                self.kill( "SIGKILL", 7 )
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
        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def shutdown( self, auth = 'netschedule_admin' ):
        " Sends the shutdown command "
        if not self.isRunning():
            raise Exception( "Inconsistency: shutting down netschedule " \
                             "while it is not running" )

        cmdLine = [ self.__grid_cli, "shutdown",
                    "--extended-cli",
                    "--auth=" + auth, "--compat-mode",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    @staticmethod
    def __appendNodeSession( cmd, node, session ):
        " Appends node and session if needed "
        cmd.append( "--extended-cli" )
        if node is None:
            cmd.append( "--client-node=default" )
        else:
            cmd.append( "--client-node=" + node )
        if session is None:
            cmd.append( "--client-session=default" )
        else:
            cmd.append( "--client-session=" + session )
        return cmd

    def getVersion( self, queue = '', node = None, session = None ):
        " Gets the netschedule server version. If queue is given => GETP "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "serverinfo",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if queue != '':
            cmdLine.append( "--queue=" + queue )
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getActiveJobsCount( self, qname, node = None, session = None ):
        " Gets the active jobs count "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats", "--active-job-count",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )
        self.verbosePrint( cmdLine )
        output = self.__safeRun( cmdLine ).strip()
        parts = output.split( ":" )
        if len( parts ) not in [1, 2]:
            raise Exception( "Unexpected line format (" + output + ")" )
        return int( parts[ len( parts ) - 1 ].strip() )

    def getQueueList( self, node = None, session = None ):
        " Get the list of queues "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "getqueuelist",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )
        self.verbosePrint( cmdLine )
        queues = self.__safeRun( cmdLine ).split( '\n' )

        for index in range( len(queues) - 1, 0, -1 ):
            if queues[ index ].strip() == "":
                del queues[ index ]
        return queues

    def reconfigure( self, auth = "netschedule_admin" ):
        " Makes netscheduled re-read its config file "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "reconf",
                    "--extended-cli",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if auth != "":
            cmdLine += [ "--auth=" + auth ]
        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).split( '\n' )

    def getQueueInfo( self, qname,
                            node = '', session = '' ):
        " Provides information about the given queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "queueinfo", qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )
        self.verbosePrint( cmdLine )

        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if not line or line.startswith( '[' ):
                continue
            parts = line.split( ':' )
            if len( parts ) != 2:
                raise Exception( "Unexpected output of the 'queueinfo' " \
                                 "command. 'key=value' format expected, " \
                                 "received: '" + line + "'" )
            result[ parts[ 0 ].strip() ] = parts[ 1 ].strip()
        return result

    def createQueue( self, qname, classname, comment = "",
                           node = None, session = None ):
        " Creates a dynamic queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "createqueue", qname, classname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )
        if comment != "":
            cmdLine += [ "--queue-description=" + comment ]

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).split( '\n' )

    def deleteQueue( self, qname,
                           node = None, session = None ):
        " Deletes a dynamic queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "deletequeue", qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def submitJob( self, qname, jobinput, affinity = "", group = "",
                   node = '', session = '' ):
        " Submit a job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "submitjob",
                    "--queue=" + qname,
                    "--input=" + jobinput,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if affinity != "":
            cmdLine += [ "--affinity=" + affinity ]
        if group != "":
            cmdLine += [ "--group=" + group ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getFastJobStatus( self, qname, jobID,
                          node = '', session = '' ):
        " Gets the fast job status SST (Pending, Running etc)"
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", jobID,
                    "--status-only", "--defer-expiration",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getJobStatus( self, qname, jobID,
                      node = '', session = '' ):
        " Gets the job status (no lifetime change) WST (Pending, Running etc)"
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", jobID,
                    "--status-only", "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def getJobBriefStatus( self, qname, jobID,
                           node = '', session = '' ):
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
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )

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

    def cancelAllQueueJobs( self, qname, auth = "netschedule_admin",
                                  node = None, session = None ):
        " removes all the jobs from the queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "canceljob", "--queue=" + qname,
                    "--all-jobs",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        if auth != "":
            cmdLine += [ "--auth=" + auth ]

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def cancelJob( self, qname, jobid,
                   node = '', session = '' ):
        " Cancels the given job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "canceljob", jobid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def cancelGroup( self, qname, group,
                   node = None, session = None ):
        " Cancels the given group of jobs "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "canceljob",
                    "--queue=" + qname,
                    "--job-group=" + group,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def dropJob( self, qname, jobid,
                       node = None, session = None ):
        " Drops the given job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "kill", jobid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def rescheduleJob( self, qname, jobid,
                             node = None, session = None ):
        " Reschedules the job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob", "--force-reschedule",
                    "--queue=" + qname, jobid,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def getServerConfiguration( self, auth = "",
                                      node = None, session = None ):
        " Provides the server configuration "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "getconf",
                    "--extended-cli",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        if auth != "":
            cmdLine.append( "--auth=" + auth )

        self.verbosePrint( cmdLine )

        return self.__safeRun( cmdLine )


    def getJobInfo( self, qname, jobid, node = None, session = None ):
        " Provides the job information "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo",
                    "--queue=" + qname, jobid,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )

        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            if len( parts ) < 2:
                if line.startswith( "[event" ):
                    continue
                raise Exception( "Unexpected job info format. " \
                                 "Expected: 'key: value', " \
                                 "Received: " + line )
            value = ":".join( parts[ 1: ] ).strip()
            result[ parts[0].strip().replace( '-', '_' ) ] = value
        return result


    def getStat( self, qname,
                       node = None, session = None ):
        " Provides the queue statistics "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )

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

    def getJob( self, qname, timeout = -1, aff = '', guid = "",
                      node = None, session = None ):
        " Provides a job for execution "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "requestjob",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if timeout != -1:
            cmdLine += [ "--wait-timeout=" + str( timeout ) ]
        if aff != '':
            cmdLine += [ "--affinity-list=" + aff ]
        if guid != "":
            cmdLine += [ "--wnode-guid=" + guid ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        output = self.__safeRun( cmdLine )
        if not output:
            return None
        lines = output.split( '\n' )

        jobID, attrs, jobInput = lines[ 0 ], lines[ 1 ], lines[ 2: ]
        attrs = attrs.split()
        authToken = attrs[ 0 ]
        attrs = attrs[ 1: ]
        attr_map = {}
        for attr in attrs:
            k, v = attr.split('=', 1)
            attr_map[ k ] = v.strip( '"' )
        return (jobID, authToken, attrs, jobInput)

    def putJob( self, qname, jobID, authToken, retCode, out = "",
                node = None, session = None ):
        " Commits the executed job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "commitjob",
                    "--queue=" + qname, jobID, authToken,
                    "--return-code=" + str( retCode ),
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if out != "":
            cmdLine += [ "--job-output=" + out ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def returnJob( self, qname, jobID, authToken,
                   node = None, session = None ):
        " returns the job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "returnjob",
                    "--queue=" + qname, jobID, authToken,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def getServerInfo( self, qname,
                             node = None, session = None ):
        " Provides the server information "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "serverinfo",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        result = {}
        try:
            for line in self.__safeRun( cmdLine ).split( '\n' ):
                line = line.strip()
                if line == "":
                    continue
                parts = line.split( ":" )
                if len( parts ) != 2:
                    continue
                result[ parts[0].strip() ] = parts[1].strip()
            return result
        except:
            pass

        cmdLine = [ self.__grid_cli, "queueinfo", qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
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


    def getQueueDump( self, qname, status = '',
                            start_after = '', count = 0,
                            group = "",
                            node = '', session = '' ):
        " Provides the server information "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "dumpqueue", "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if status != '':
            cmdLine += [ "--select-by-status=" + status ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )
        if start_after != '':
            cmdLine += [ "--start-after-job=" + start_after ]
        if count != 0:
            cmdLine += [ "--job-count=" + str( count ) ]
        if group != "":
            cmdLine += [ "--job-group=" + group ]

        self.verbosePrint( cmdLine )
        result = []
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            result.append( line )
        return result

    def getJobProgressMessage( self, qname, jobID, node = '', session = '' ):
        " Provides the job progress message "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", "--queue=" + qname,
                    "--progress-message-only", jobID,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def setJobProgressMessage( self, qname, jobID, msg,
                               node = '', session = '' ):
        " Updates the job progress message "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob", "--queue=" + qname,
                    "--progress-message=" + msg, jobID,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )


    def failJob( self, qname, jobID, authToken,
                 retCode, output = "", errmsg = "",
                 node = None, session = None ):
        " Fails the given job "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "commitjob", "--queue=" + qname,
                    jobID, str( authToken ), "--fail-job=" + errmsg,
                    "--return-code=" + str( retCode ),
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if output != "":
            cmdLine += [ "--job-output=" + output ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def registerWorkerNode( self, qname, port,
                                  node = None, session = None ):
        " Registers the worker node "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--register-wnode", "--wnode-port=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def unregisterWorkerNode( self, qname, port,
                                    node = None, session = None ):
        " Unregisters the worker node "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--unregister-wnode", "--wnode-port=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def getWorkerNodeStat( self, qname, node = '', session = '' ):
        " Provides the worker nodes statistics attached to the queue "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--worker-nodes",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        result = []
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            result.append( line )
        return result

    def getBriefStat( self, qname = '', node = '', session = '' ):
        " Runs the STAT command "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--brief",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if qname != '':
            cmdLine.append( "--queue=" + qname )
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        result = []
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            result.append( line )
        return result

    def extendJobExpiration( self, qname, jobid, timeout,
                             node = '', session = '' ):
        " Extends the given job expiration time (JDEX) "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob",
                    jobid, "--queue=" + qname,
                    "--extend-lifetime=" + str( timeout ),
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def getAffinityStatus( self, qname, affinity = None,
                           node = '', session = '' ):
        " Provides the affinity status "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--queue=" + qname,
                    "--jobs-by-status",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if affinity:
            cmdLine.append( "--affinity=" + affinity )
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        result = {}
        for line in self.__safeRun( cmdLine ).split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            result[ parts[0].strip() ] = int( parts[1].strip() )
        return result

    def getAffinityList( self, qname,
                         node = '', session = '' ):
        " Provides the affinity list "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--queue=" + qname, "--jobs-by-affinity",
                    "--output-format=raw",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        result = {}
        output = self.__safeRun( cmdLine )
        for line in output.split( '\n' ):
            line = line.strip()
            if line == "":
                continue
            parts = line.split( ':' )
            affToken = parts[0].strip()
            parts = parts[1].split( ',' )
            result[ affToken ] = [ int( parts[0].strip() ),
                                   int( parts[1].strip() ),
                                   int( parts[2].strip() ),
                                   int( parts[3].strip() ) ]
        return result

    def initWorkerNode( self, qname, port ):
        " new style of registering worker nodes "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--register-wnode=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine ).strip()

    def resetWorkerNode( self, qname, guid ):
        " De-registers the worker node "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--unregister-wnode=" + guid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def getJobsForReading( self, qname, count, timeout = -1,
                                 node = None, session = None ):
        " Requests jobs for reading "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "readjob",
                    "--limit=" + str( count ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        if timeout != -1:
            cmdLine += [ "--timeout=" + str( timeout ) ]

        self.verbosePrint( cmdLine )
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

    def confirmJobRead( self, qname, groupID, jobs,
                              node = None, session = None ):
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
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def rollbackJobRead( self, qname, groupID, jobs,
                               node = None, session = None ):
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
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )


    def failJobReading( self, qname, groupID, jobs, err_msg,
                              node = None, session = None ):
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
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        return self.__safeRun( cmdLine )

    def submitBatch( self, qname, jobs,
                           node = None, session = None ):
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
            cmdLine = self.__appendNodeSession( cmdLine, node, session )
            jobids = self.__safeRun( cmdLine )

            # Remove the temporary file
        except:
            os.unlink( filename )
            raise

        os.unlink( filename )

        result = []
        parts = jobids.split( '\n' )
        for item in parts:
            item = item.strip()
            if item != "":
                result.append( item )
        return result

    def execAny( self, qname, command,
                       node = None, session = None ):
        " Executes an arbitrary command "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli,
                    "--extended-cli",
                    "exec", command,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        return self.__safeRun( cmdLine )

    def spawnGet2Wait( self, qname, timeout,
                       affs, isPreferredAffs, isAnyAffs,
                       node, session ):
        " Spawns grid_cli to wait for GET2 notifications "

        cmdLine = [ self.__grid_cli,
                    "--extended-cli",
                    "requestjob", "--dump-ns-notifications",
                    "--wait-timeout=" + str( timeout ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        if len( affs ) > 0:
            cmdLine.append( "--affinity-list=" + ",".join( affs ) )
        if isPreferredAffs:
            cmdLine.append( "--use-preferred-affinities" )
        if isAnyAffs:
            cmdLine.append( "--any-affinity" )

        if self.__verbose:
            print "Executing command: " + " ".join( cmdLine )

        process = Popen( cmdLine, stdin = PIPE,
                         stdout = PIPE, stderr = PIPE )
        process.stdin.close()
        return process

    def spawnSubmitWait( self, qname, timeout,
                       aff = "",
                       node = None, session = None ):
        " Spawns grid_cli to wait for SUBMIT notifications "

        cmdLine = [ self.__grid_cli,
                    "--extended-cli",
                    "submitjob", "--dump-ns-notifications",
                    "--wait-timeout=" + str( timeout + 2 ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        if aff != "":
            cmdLine.append( "--affinity=" + aff )

        process = Popen( cmdLine, stdin = PIPE,
                         stdout = PIPE, stderr = PIPE )
        process.stdin.close()
        time.sleep( 2 )
        if process.poll() is not None:
            raise Exception( "Failed to spawn with submit" )
        return process

    def getJobsForReading2( self, qname, timeout = -1,  group = "",
                            node = None, session = None ):
        " Runs the READ command and provides jobKey, state and passport "
        cmdLine = [ self.__grid_cli,
                    "--extended-cli",
                    "readjob", "--reliable-read",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        if timeout != -1:
            cmdLine.append( "--timeout=" + str( timeout ) )
        if group != "":
            cmdLine.append( "--job-group=" + group )

        self.verbosePrint( cmdLine )
        output = self.__safeRun( cmdLine )
        if output == '':
            # No jobs for reading
            return '', '', ''

        lines = output.split( '\n' )
        if len( lines ) < 3:
            raise Exception( "Unexpected format of the grid_cli readjob" )
        #      key               state             passport
        return lines[0].strip(), lines[1].strip(), lines[2].strip()

    def failRead2( self, qname, jobID, passport, errMsg = "",
                         node = None, session = None ):
        " FRED for a job "
        cmdLine = [ self.__grid_cli,
                    "--extended-cli",
                    "readjob",
                    "--fail-read=" + passport,
                    "--job-id=" + jobID,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        if errMsg != "":
            cmdLine.append( "--error-message=" + errMsg )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def confirmRead2( self, qname, jobID, passport,
                         node = None, session = None ):
        " CFRM for a job "
        cmdLine = [ self.__grid_cli,
                    "--extended-cli",
                    "readjob",
                    "--confirm-read=" + passport,
                    "--job-id=" + jobID,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return

    def rollbackRead2( self, qname, jobID, passport,
                         node = None, session = None ):
        " RDRB for a job "
        cmdLine = [ self.__grid_cli,
                    "--extended-cli",
                    "readjob",
                    "--rollback-read=" + passport,
                    "--job-id=" + jobID,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        cmdLine = self.__appendNodeSession( cmdLine, node, session )

        self.verbosePrint( cmdLine )
        self.__safeRun( cmdLine )
        return
