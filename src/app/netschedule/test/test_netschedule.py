#!/opt/python-2.5/bin/python -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server test script
"""

import os, os.path, sys, pwd, socket, re
import time
import tempfile
from subprocess import Popen, PIPE
from optparse import OptionParser


defaultGridCliPath = ""
defaultNetschedulePath = "netscheduled"
verbose = False


def getUsername():
    " Provides the current user name "
    return pwd.getpwuid( os.getuid() )[ 0 ]


class NetSchedule:
    " Represents a single netschedule server "

    def __init__( self, host, port, path, grid_cli_path, db_path ):
        " initializes the netschedule object "
        self.__host = host
        self.__port = port
        self.__path = path.strip()

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

        self.checkPresence()
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

    def checkPresence( self ):
        " Checks that the given file exists on the remote host "

        # Check for grid_cli
        if os.system( self.__grid_cli + " help > /dev/null 2>&1" ) != 0:
            raise Exception( "Cannot find grid_cli at the " \
                             "specified location: " + self.__grid_cli )

        # Executable file
        cmdLine = [ "ls", self.__path + "netscheduled" ]
        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        try:
            if safeRun( cmdLine ).strip() != self.__path + "netscheduled":
                raise Exception()
        except:
            raise Exception( "netscheduled is not found on " + self.__host )

        # Config files
        cmdLine = [ "ls", self.__path + "netscheduled.ini.1" ]
        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        try:
            if safeRun( cmdLine ).strip() != self.__path + "netscheduled.ini.1":
                raise Exception()
        except:
            raise Exception( "netscheduled.ini.1 configuration " \
                             "is not found on " + self.__host )

        cmdLine = [ "ls", self.__path + "netscheduled.ini.2" ]
        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        try:
            if safeRun( cmdLine ).strip() != self.__path + "netscheduled.ini.2":
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

    def setConfig( self, number = 1 ):
        " Copies the required config file to the current one "
        fName = self.__path + "netscheduled.ini." + str( number )
        cmdLine = [ "ls", fName ]
        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        if safeRun( cmdLine ).strip() != fName:
            raise Exception( "Cannot find " + fName )

        cmdLine = [ "rm", "-rf", self.__path + "netscheduled.ini" ]
        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )

        # Substitute DB path
        replaceWhat = "\\$DBPATH"
        replaceTo = self.__dbPath.replace( "/", "\\/" )
        cmdLine = "sed -e 's/" + replaceWhat + "/" + replaceTo + "/g' " + \
                  fName + " > " + self.__path + "netscheduled.ini.temp"
        if not self.isLocal():
            cmdLine = "ssh " + self.__host + " " + cmdLine
        if verbose:
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
        if not self.isLocal():
            cmdLine = "ssh " + self.__host + " " + cmdLine
        if verbose:
            print "Executing command: " + cmdLine
        retCode = os.system( cmdLine )
        if retCode != 0:
            raise Exception( "Error executing command: " + cmdLine )

        return

    def isLocal( self ):
        " Returns True if it is a local netschedule "
        if self.__host == "localhost":
            return True
        if self.__host == "127.0.0.1":
            return True
        return False

    def isRunning( self ):
        " Provides True if the server is still running "
        cmdLine = [ "ps", "-ef", "|", "grep", "netscheduled",
                    "|", "grep", "conffile",
                    "|", "grep", "logfile",
                    "|", "grep", "-v", "grep",
                    "|", "grep", getUsername(), "|", "wc", "-l" ]

        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine

            if verbose:
                print "Executing command: " + " ".join( cmdLine )
            output = safeRun( cmdLine ).split( '\n' )
            return int( output[ 0 ].strip() ) != "0"

        if verbose:
            print "Executing command: " + " ".join( cmdLine )

        psProc = Popen( " ".join( cmdLine ), shell = True,
                    stdout = PIPE )
        output = psProc.stdout.read()
        psProc.stdout.close()
        psProc.wait()
        return output.strip() != "0"

    def start( self ):
        " starts netschedule "
        if self.isRunning():
            return
        cmdLine = [ self.__path + "netscheduled",
                    "-conffile", self.__path + "netscheduled.ini",
                    "-logfile", self.__path + "netscheduled.log" ]
        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine

        count = 7
        while count > 0:
            if verbose:
                print "Executing command: " + " ".join( cmdLine )
            safeRun( cmdLine )
            if self.isRunning():
                time.sleep( 5 )
                return
            time.sleep( 2 )
            count -= 1
        if not self.isRunning():
            raise Exception( "Error starting netschedule" )

        time.sleep( 10 )
        return

    def kill( self, signal = "SIGKILL", seconds = 30 ):
        " kills netschedule "
        if not self.isRunning():
            return
        try:
            cmdLine = [ "killall",
                        "-s", signal, "netscheduled" ]
            if not self.isLocal():
                cmdLine = [ "ssh", self.__host ] + cmdLine
            if verbose:
                print "Executing command: " + " ".join( cmdLine )
            safeRun( cmdLine )
        except:
            pass

        count = seconds
        while count > 0:
            if not self.isRunning():
                return
            count -= 1
            time.sleep( 1 )
        raise Exception( "Cannot kill netschedule on " + self.__host )

    def safeStop( self ):
        " Kills netschedule without exception "
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
        return

    def deleteDB( self ):
        " Deletes the data directory from the disk "
        if self.isRunning():
            raise Exception( "Cannot delete data directory " \
                             "while netschedule is running" )

        cmdLine = [ "rm", "-rf", self.__dbPath ]
        if not self.isLocal():
            cmdLine = [ "ssh", self.__host ] + cmdLine
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )
        return

    def shutdown( self, auth = 'netschedule_admin' ):
        " Sends the shutdown command "
        if not self.isRunning():
            raise Exception( "Inconsistency: shutting down netschedule " \
                             "while it is not running" )

        cmdLine = [ self.__grid_cli, "shutdown",
                    "--auth=" + auth, "--compat-mode",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )
        return

    def getVersion( self ):
        " Gets the netschedule server version "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "serverinfo",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).strip()

    def getActiveJobsCount( self, qname ):
        " Gets the active jobs count "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats", "--active-job-count",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return int( safeRun( cmdLine ).strip() )

    def getQueueList( self ):
        " Get the list of queues "
        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "getqueuelist",
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        queues = safeRun( cmdLine ).split( '\n' )

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
        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).split( '\n' )

    def getQueueInfo( self, qname ):
        " Provides information about the given queue "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "queueinfo", qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if verbose:
            print "Executing command: " + " ".join( cmdLine )

        result = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).split( '\n' )

    def deleteQueue( self, qname ):
        " Deletes a dynamic queue "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "deletequeue", qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).strip()

    def getFastJobStatus( self, qname, jobID ):
        " Gets the fast job status SST (Pending, Running etc)"

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", jobID,
                    "--status-only", "--defer-expiration",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).strip()

    def getJobStatus( self, qname, jobID ):
        " Gets the job status (no lifetime change) WST (Pending, Running etc)"

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "jobinfo", jobID,
                    "--status-only", "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).strip()

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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )

        result = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

    def removeAllQueueJobs( self, qname, auth = 'netschedule_admin' ):
        " removes all the jobs from the queue "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "kill", "--all-jobs",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]
        if auth != "":
            cmdLine += [ "--auth=" + auth, "--compat-mode" ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )
        return

    def cancelJob( self, qname, jobid ):
        " Cancels the given job "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "canceljob", jobid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )
        return

    def dropJob( self, qname, jobid ):
        " Drops the given job "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "kill", jobid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )
        return

    def rescheduleJob( self, qname, jobid ):
        " Reschedules the job "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob", "--force-reschedule",
                    "--queue=" + qname, jobid,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        safeRun( cmdLine )
        return

    def getQueueConfiguration( self, qname ):
        " Provides the server configuration "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "getconf",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )

        params = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )

        result = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )

        recognized = [ 'Started', 'Pending', 'Running', 'Returned',
                       'Canceled', 'Failed', 'Done', 'Reading',
                       'Confirmed', 'ReadFailed', 'Timeout',
                       'ReadTimeout' ]

        result = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        output = safeRun( cmdLine ).split( '\n' )

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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

    def returnJob( self, qname, jobID ):
        " returns the job "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "returnjob",
                    "--queue=" + qname, jobID,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

    def getServerInfo( self, qname ):
        " Provides the server information "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "serverinfo",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = []
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).strip()

    def setJobProgressMessage( self, qname, jobID, msg ):
        " Updates the job progress message "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "updatejob", "--queue=" + qname,
                    "--progress-message=" + msg, jobID,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )


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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

    def registerWorkerNode( self, qname, port ):
        " Registers the worker node "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--register-wnode", "--wnode-port=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

    def unregisterWorkerNode( self, qname, port ):
        " Unregisters the worker node "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--unregister-wnode", "--wnode-port=" + str( port ),
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

    def getWorkerNodeStat( self, qname ):
        " Provides the worker nodes statistics attached to the queue "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--worker-nodes",
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = []
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

    def getAffinityStatus( self, qname, affinity ):
        " Provides the affinity status "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "stats",
                    "--queue=" + qname,
                    "--jobs-by-status", "--affinity=" + affinity,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        result = {}
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine ).strip()

    def resetWorkerNode( self, qname, guid ):
        " De-registers the worker node "

        if not self.isRunning():
            raise Exception( "netschedule is not running" )

        cmdLine = [ self.__grid_cli, "regwnode",
                    "--unregister-wnode=" + guid,
                    "--queue=" + qname,
                    "--ns=" + self.__host + ":" + str( self.__port ) ]

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        groupID = -1
        jobs = []
        for line in safeRun( cmdLine ).split( '\n' ):
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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )


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

        if verbose:
            print "Executing command: " + " ".join( cmdLine )
        return safeRun( cmdLine )

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
            jobids = safeRun( cmdLine )

            # Remove the temporary file
        except:
            os.unlink( filename )
            raise

        os.unlink( filename )
        return jobids





class TestBase:
    " Base class for tests "

    def __init__( self, netschedule ):
        self.ns = netschedule
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        raise Exception( "Not implemented" )

    def execute( self ):
        " Should return True if the execution completed successfully "
        raise Exception( "Not implemented" )

    def clear( self ):
        " Stops and cleans everything "
        self.ns.kill( "SIGKILL" )
        self.ns.deleteDB()
        self.ns.setConfig( 1 )
        return



def safeRun( commandArgs ):
    " Provides the process stdout "
    stdOut, stdErr = safeRunWithStderr( commandArgs )
    return stdOut


def safeRunWithStderr( commandArgs ):
    " Runs the given command and provides both stdout and stderr "

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
    try:
        # On WinXX the file might still be kept and unlink generates exception
        os.unlink( errTmp[ 1 ] )
    except:
        pass

    # 'grep' return codes:
    # 0 - OK, lines found
    # 1 - OK, no lines found
    # 2 - Error occured

    if process.returncode == 0 or \
       ( os.path.basename( commandArgs[ 0 ] ) == "grep" and \
         process.returncode == 1 ):
        # No problems, the ret code is 0 or the grep special case
        return processStdout, err.strip()

    # A problem has been identified
    raise Exception( "Error in '%s' invocation: %s" % \
                     (commandArgs[0], err) )


def parserError( parser, message ):
    " Prints the message and help on stderr "

    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return 1



def main():
    " main function for the netcache sync test "

    parser = OptionParser(
    """
    %prog  <port>
    Note #1: netschedule server will be running on the same host
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "--path-grid-cli", dest="pathGridCli",
                       default=defaultGridCliPath,
                       help="Path to grid_cli utility" )
    parser.add_option( "--path-netschedule", dest="pathNetschedule",
                       default=defaultNetschedulePath,
                       help="Path to the netschedule daemon" )
    parser.add_option( "--db-path", dest="pathDB",
                       default=os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
                               os.path.sep + "data",
                       help="Directory name where data are stored" )
    parser.add_option( "--start-from", dest="start_from",
                       default="0",
                       help="Test index to start from (default: 0)" )
    parser.add_option( "--header", dest="header",
                       default="",
                       help="Header for the tests output" )
    parser.add_option( "--all-to-stderr",
                       action="store_true", dest="alltostderr", default=False,
                       help="print the messages on stderr only (default: False)" )


    # parse the command line options
    options, args = parser.parse_args()

    global verbose
    verbose = options.verbose

    if options.alltostderr:
        sys.stdout = sys.stderr


    startIndex = int( options.start_from )
    if startIndex < 0:
        raise Exception( "Negative start test index" )

    # Check the number of arguments
    if len( args ) != 1:
        return parserError( parser, "Incorrect number of arguments" )
    port = int( args[ 0 ] )
    if port <= 0 or port > 65535:
        raise Exception( "Incorrect port number" )

    if verbose:
        print "Using netschedule path: " + options.pathNetschedule
        print "Using grid_cli path: " + options.pathGridCli
        print "Using DB path: " + options.pathDB
        print "Starting tests from: " + options.start_from

    netschedule = NetSchedule( "127.0.0.1", port,
                                options.pathNetschedule, options.pathGridCli,
                                options.pathDB )

    tests = [ Scenario00( netschedule ), Scenario01( netschedule ),
              Scenario02( netschedule ), Scenario03( netschedule ),
              Scenario04( netschedule ), Scenario05( netschedule ),
              Scenario06( netschedule ), Scenario07( netschedule ),
              Scenario08( netschedule ), Scenario09( netschedule ),
              Scenario10( netschedule ), Scenario11( netschedule ),
              Scenario12( netschedule ), Scenario13( netschedule ),
              Scenario14( netschedule ),
              # Scenario15( netschedule ),
              Scenario16( netschedule ), Scenario17( netschedule ),
              Scenario18( netschedule ),
              # Scenario19( netschedule ),
              # Scenario20( netschedule ),
              Scenario21( netschedule ),
              Scenario22( netschedule ), Scenario23( netschedule ),
              # Scenario24( netschedule ),
              Scenario25( netschedule ),
              Scenario26( netschedule ), Scenario27( netschedule ),
              Scenario28( netschedule ), Scenario29( netschedule ),
              Scenario30( netschedule ), Scenario31( netschedule ),
              Scenario32( netschedule ), Scenario33( netschedule ),
              # Scenario34( netschedule ),
              Scenario35( netschedule ),
              Scenario36( netschedule ), Scenario37( netschedule ),
              Scenario38( netschedule ), Scenario39( netschedule ),
              Scenario40( netschedule ), Scenario41( netschedule ),
              Scenario42( netschedule ),
              # Scenario43( netschedule ),
              Scenario44( netschedule ),
              # Scenario45( netschedule ),
              Scenario46( netschedule ), Scenario47( netschedule ),
              Scenario48( netschedule ), Scenario49( netschedule ),
              # Scenario50( netschedule ),
              Scenario51( netschedule ),
              Scenario52( netschedule ), Scenario53( netschedule ),
              Scenario54( netschedule ),
              # Scenario55( netschedule ),
              # Scenario56( netschedule )
            ]

    successCount = 0
    failureCount = 0
    testCount = startIndex

    if options.header != "":
        print options.header

    try:
        for index in range( startIndex, len( tests ) ):
            aTest = tests[ index ]
            try:
                succeeded = aTest.execute()
                if succeeded:
                    successCount += 1
                    print "Test #" + str( testCount ) + " succeeded"
                    sys.stdout.flush()
                else:
                    print >> sys.stderr, "Test #" + str( testCount ) + \
                                         " failed. Scenario: " + \
                                         aTest.getScenario()
                    sys.stderr.flush()
                    failureCount += 1
            except Exception, exct:
                failureCount += 1
                print >> sys.stderr, "Test #" + str( testCount ) + \
                                     " failed. Scenario: " + \
                                     aTest.getScenario() + "\n" \
                                     "Exception:\n" + str( exct )
                sys.stderr.flush()
            testCount += 1
        netschedule.safeStop()
        netschedule.deleteDB()
    except:
        netschedule.safeStop()
        try:
            netschedule.deleteDB()
        except:
            pass
        raise

    print "Total succeeded: " + str( successCount )
    print "Total failed: " + str( failureCount )

    if failureCount > 0:
        return 1
    return 0


def getRetCode( info ):
    " Provides the ret code for both, old and new NS output "
    if info.has_key( "ret_code" ):
        # Old format
        return info[ "ret_code" ]

    # New format
    if info.has_key( "attempt_counter" ):
        lastAttempt = info[ "attempt_counter" ]
        attemptLine = info[ "attempt" + lastAttempt ]
    else:
        lastAttempt = info[ "event_counter" ]
        attemptLine = info[ "event" + lastAttempt ]

    parts = attemptLine.split( ' ' )
    retcodePart = parts[ 2 ]
    if not retcodePart.startswith( "ret_code=" ):
        raise Exception( "Unknown DUMP <job> format" )

    value = retcodePart.replace( "ret_code=", "" )
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
    else:
        lastAttempt = info[ "event_counter" ]
        attemptLine = info[ "event" + lastAttempt ]

    parts = attemptLine.split( 'err_msg=' )
    value = parts[ 1 ].strip()
    return value


class Scenario00( TestBase ):
    " Scenario 0 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start NS, kill NS (00)"

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
        return "Start NS, shutdown NS (01)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.shutdown()
        time.sleep( 1 )
        return not self.ns.isRunning()


class Scenario02( TestBase ):
    " Scenario 2 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start NS, kill -9 NS, start NS (02)"

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
        return "Start NS, shutdown NS with the 'nobody' account (03)"

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
            if "Access denied: admin privileges required" in str( exc ):
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
        return "Getting netschedule version (04)"

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
        return "Getting the number of active jobs (05)"

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
        return "Getting the list of the configured queues (06)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        queues = self.ns.getQueueList()
        return len( queues ) == 1 and queues[ 0 ] == "TEST"


class Scenario07( TestBase ):
    " Scenario 7 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting the list of the configured queues (07)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.setConfig( 2 )
        self.ns.reconfigure()

        queues = self.ns.getQueueList()
        if len( queues ) != 3 or \
               queues[ 0 ] != "TEST" or \
               queues[ 1 ] != "TEST2" or \
               queues[ 2 ] != "TEST3":
            print >> sys.stderr, "Expected queues: TEST, TEST2 and TEST3." \
                                 " Received: " + ", ".join( queues )
            return False
        return True


class Scenario08( TestBase ):
    " Scenario 8 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting non existed queue (08)"

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
            if "Job queue not found" in str( exc ):
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
        return "Getting existing queue info (09)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        qinfo = self.ns.getQueueInfo( 'TEST' )
        return qinfo[ "Queue name" ] == "TEST" and qinfo[ "Type" ] == "static"


class Scenario10( TestBase ):
    " Scenario 10 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Creating a dynamic queue and then delete it (10)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        self.ns.createQueue( "TEST1", "TEST", "bla" )
        qinfo = self.ns.getQueueInfo( "TEST1" )
        if qinfo[ "Queue name" ] != "TEST1" or \
           qinfo[ "Type" ] != "dynamic" or \
           qinfo[ "Model queue" ] != "TEST" or \
           qinfo[ "Description" ] != "bla":
            print >> sys.stderr, "Unexpected queue info received: " + \
                                 str( qinfo )
            return False

        self.ns.deleteQueue( "TEST1" )
        try:
            qinfo = self.ns.getQueueInfo( "TEST1" )
        except Exception, exc:
            if "Job queue not found" in str( exc ):
                return True
            raise
        return False


class Scenario11( TestBase ):
    " Scenario 11 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submitting a single job and checking the number of " \
               "active jobs and the job status (11)"

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
        return "submit a job, drop all the jobs, get the job fast status (12)"

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

        self.ns.removeAllQueueJobs( "TEST" )
        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        return self.ns.getFastJobStatus( 'TEST', jobID ) == "NotFound"


class Scenario13( TestBase ):
    " Scenario 13 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "submit a job, cancel it (13)"

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


class Scenario14( TestBase ):
    " Scenario 14 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, drop the job (14)"

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

        self.ns.dropJob( "TEST", jobID )
        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        return self.ns.getFastJobStatus( 'TEST', jobID ) == "NotFound"


class Scenario15( TestBase ):
    " Scenario 15 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, cancel the job, reschedule the job (15)"

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
        if self.ns.getFastJobStatus( 'TEST', jobID ) != "Canceled":
            return False

        self.ns.rescheduleJob( "TEST", jobID )
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        return self.ns.getFastJobStatus( 'TEST', jobID ) == "Pending"


class Scenario16( TestBase ):
    " Scenario 16 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting a queue configuration (16)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        params = self.ns.getQueueConfiguration( 'TEST' )
        return params[ 'timeout' ] == '30' and \
               params[ 'run_timeout' ] == "7" and \
               params[ 'failed_retries' ] == "3"



class Scenario17( TestBase ):
    " Scenario 17 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, stop NS, start NS, check the job status (17)"

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
        time.sleep( 1 )
        if self.ns.isRunning():
            raise Exception( "Cannot shutdown netschedule" )

        self.ns.start()
        time.sleep( 1 )
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
               "It will fail until NS is fixed. (18)"

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
        return info[ 'status' ] == 'Pending' and \
               info[ 'progress_msg' ] == "''" and \
               info[ 'mask' ] == "0" and \
               info[ 'aff_id' ] == "0" and \
               info[ 'run_counter' ] == "0" and \
               info[ 'input-storage' ] == "embedded, size=10" and \
               info[ 'input-data' ] == "'D test input'"


class Scenario19( TestBase ):
    " Scenario 19 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, make a pause, touch the job lifetime. " \
               "It will fail until NS is fixed. (19)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( "TEST", 'bla')
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        # Memorize the expiration time
        self.ns.getFastJobStatus( 'TEST', jobID )
        time.sleep( 5 )
        info = self.ns.getJobInfo( 'TEST', jobID )
        # Memorize the expiration time

        # Compare the expiration times
        return False


class Scenario20( TestBase ):
    " Scenario 20 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, make a pause, " \
               "touch the job without changing the lifetime. " \
               "It will fail until NS is fixed. (20)"

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

        info = self.ns.getJobInfo( 'TEST', jobID )
        # print info
        # Memorize the expiration time
        self.ns.getJobStatus( 'TEST', jobID )
        time.sleep( 5 )
        info = self.ns.getJobInfo( 'TEST', jobID )
        # print info
        # Memorize the expiration time

        # Compare the expiration times
        return False


class Scenario21( TestBase ):
    " Scenario 21 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, change the job id slightly, get the job info (21)"

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
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        # Make modifications in the job id
        jobID = jobID[ : -4 ] + "9999"

        # Sic! In spite of the fact the jobID.oring != jobID.changed we GET
        # the same job info!
        # I can check the job status only via a direct connection because
        # the utility [currently] takes the server info from the job key...
        self.ns.connect( 10 )
        self.ns.directLogin( 'TEST' )
        self.ns.directSendCmd( 'SST ' + jobID )
        reply = self.ns.directReadSingleReply()
        self.ns.disconnect()

        if reply[ 0 ] != True or reply[ 1 ] != "0":
            return False
        return True


class Scenario22( TestBase ):
    " Scenario 22 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get statistics (22)"

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
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        stats = self.ns.getStat( "TEST" )
        return stats[ 'Pending' ] == "1" and \
               stats[ 'Running' ] == "0" and \
               stats[ 'Returned' ] == "0" and \
               stats[ 'Canceled' ] == "0" and \
               stats[ 'Failed' ] == "0" and \
               stats[ 'Done' ] == "0" and \
               stats[ 'Reading' ] == "0" and \
               stats[ 'Confirmed' ] == "0" and \
               stats[ 'ReadFailed' ] == "0" and \
               stats[ 'Timeout' ] == "0" and \
               stats[ 'ReadTimeout' ] == "0"


class Scenario23( TestBase ):
    " Scenario 23 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job, check the active job number, " \
               "get the job status, get the job dump (23)"

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
        jobIDReceived = self.ns.getJob( 'TEST' )

        if jobID != jobIDReceived:
            return False
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        status = self.ns.getJobBriefStatus( 'TEST', jobID )
        if status[ "Status" ] != "Running":
            return False

        if self.ns.getFastJobStatus( 'TEST', jobID ) != "Running":
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ "run_counter" ] != "1" or info[ "status" ] != "Running":
            return False

        return True


class Scenario24( TestBase ):
    " Scenario 24 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job, take a break till run-time is " \
               "expired, get the job dump and check the run counter, get " \
               "the job again, put the job and check the return code. " \
               "It will fail until NS is fixed. (24)"

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
        jobIDReceived = self.ns.getJob( 'TEST' )

        if jobID != jobIDReceived:
            return False
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ "status" ] != "Running" or info[ "run_counter" ] != "1":
            return False

        time.sleep( 40 )    # Till the job is expired
        jobIDReceived = self.ns.getJob( 'TEST' )
        if jobID != jobIDReceived:
            return False
        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        self.ns.putJob( 'TEST', jobID, 1, 'OUT' )
        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ "status" ] != "Done" or info[ "run_counter" ] != "2" or \
           info[ "output-data" ] != "OUT" or getRetCode( info ) != "1":
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
               "return the job, check status (25)"

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
        jobIDReceived = self.ns.getJob( 'TEST' )

        self.ns.returnJob( 'TEST', jobID )

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
        return "Getting server info (26)"

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
        if info[ "Maximum output size" ] != "262144" or \
           info[ "Maximum input size" ] != "262144":
            return False
        return True


class Scenario27( TestBase ):
    " Scenario 27 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Get queue info, submit a job, get queue info, " \
               "get the job, get queue info (27)"

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

        dump = self.ns.getQueueDump( 'TEST', 'Pending' )
        if len( dump ) != 2:
            return False

        jobIDReceived = self.ns.getJob( 'TEST' )

        dump = self.ns.getQueueDump( 'TEST', 'Pending' )
        if len( dump ) != 1:
            return False

        dump = self.ns.getQueueDump( 'TEST', 'Running' )
        if len( dump ) != 1:
            return False

        return True


class Scenario28( TestBase ):
    " Scenario 28 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get a job, check the status (28)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobID = self.ns.submitJob( "TEST", '111' )
        jobIDReceived = self.ns.getJob( 'TEST', 1, 8900 )

        if self.ns.getActiveJobsCount( 'TEST' ) != 1:
            return False

        briefInfo = self.ns.getJobBriefStatus( 'TEST', jobID )
        if briefInfo[ 'Status' ] != 'Running':
            return False

        if self.ns.getFastJobStatus( 'TEST', jobID ) != 'Running':
            return False

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ "status" ] != "Running" or info[ "run_counter" ] != "1":
            return False
        return True


class Scenario29( TestBase ):
    " Scenario 29 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting a job whene there are none (29)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        if self.ns.getActiveJobsCount( 'TEST' ) != 0:
            return False

        jobIDReceived = self.ns.getJob( 'TEST', 1, 8900 )
        return jobIDReceived == ""


class Scenario30( TestBase ):
    " Scenario 30 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Getting a job progress message when there is no such job (30)"

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
        except Exception, exc:
            if 'Job not found' in str( exc ):
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
        return "Setting a job progress message when there is no such job (31)"

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
            if 'Job not found' in str( exc ):
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
               "update the job progress message, check the progress message (32)"

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
               "update the job progress message, check the progress message (33)"

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
               "It will fail until NS is fixed. (34)"

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
        self.ns.getJob( 'TEST' )
        self.ns.failJob( 'TEST', jobID, 255,
                         "Test output", "Test error message" )

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ 'status' ] != 'Failed' or \
           info[ 'run_counter' ] != '1' or \
           getRetCode( info ) != '255' or \
           info[ 'output-data' ] != "'D Test output'" or \
           getErrMsg( info ) != "'Test error message'":
            return False

        return True


class Scenario35( TestBase ):
    " Scenario 35 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job for execution, " \
               "get the worker node statistics, extend the job expiration, " \
               "get the worker node statistics (35)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        receivedJobID = self.ns.getJob( 'TEST' )
        if jobID != receivedJobID:
            return False

        stats = self.ns.getWorkerNodeStat( 'TEST' )
        if "grid_cli @ localhost UDP:0" not in stats[0]:
            return False

        # Extract the expiration timeout
        parts = stats[0].split( '(' )
        startExpValue = int( parts[1].split( ')' )[0] )     # 60

        self.ns.extendJobExpiration( 'TEST', jobID, 9000 )
        time.sleep( 2 )

        stats = self.ns.getWorkerNodeStat( 'TEST' )

        # There will be two records here. Find the index we need.
        if len( stats ) != 2:
            return False
        if "jobs= 1" in stats[0]:
            index = 0
        else:
            if "jobs= 1" in stats[1]:
                index = 1
            else:
                return False

        # Extract the expiration timeout
        parts = stats[index].split( '(' )
        finishExpValue = int( parts[1].split( ')' )[0] )

        if 9000 - finishExpValue < 2:
            return False
        return True



class Scenario36( TestBase ):
    " Scenario 36 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit first job, submit second job, get the job, " \
               "exchange the job for another one, get jobs status (36)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID1 = self.ns.submitJob( 'TEST', 'bla' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla' )

        receivedJobID1 = self.ns.getJob( 'TEST' )
        if jobID1 != receivedJobID1:
            return False

        receivedJobID2 = self.ns.exchangeJob( 'TEST', receivedJobID1, 12,
                                              'Test output' )

        info1 = self.ns.getJobInfo( 'TEST', jobID1 )
        if info1[ 'status' ] != 'Done' or \
           info1[ 'run_counter' ] != '1' or \
           getRetCode( info1 ) != '12' or \
           info1[ 'output-data' ] != "'D Test output'":
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
        return "Get status of non-existing affinity (37)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        try:
            self.ns.getAffinityStatus( 'TEST', 'non-existing-affinity' )
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
        return "Submit a job with affinity, get status of the affinity (38)"

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
               "get the list of affinities (39)"

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
        if affList[ "myaffinity1" ] == 1 and \
           affList[ "myaffinity2" ] == 1:
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
               "get the job with affinity (40)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID1 = self.ns.submitJob( 'TEST', 'bla' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'aff0' )

        receivedJobID = self.ns.getJob( 'TEST', -1, -1, 'aff0' )
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
               "get a job with another affinity (41)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( 'TEST', 'bla', 'aff0' )

        try:
            self.ns.getJob( 'TEST', -1, -1, 'other_affinity' )
        except Exception, exc:
            if "NoJobsWithAffinity" in str( exc ):
                raise Exception( "Old style NS exception answer: NoJobsWithAffinity" )
            raise

        return True


class Scenario42( TestBase ):
    " Scenario 42 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit two jobs with an affinity aff0, " \
               "get a job with aff0, exchage it to a job with affinity aff1. " \
               "It will fail till NS is fixed. (42)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID1 = self.ns.submitJob( 'TEST', 'bla', 'aff0' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla', 'aff0' )

        receivedJobID1 = self.ns.getJob( 'TEST', -1, -1, 'aff0' )
        try:
            self.ns.exchangeJob( 'TEST', receivedJobID1, 0, 'bla', 'aff1' )
        except Exception, exc:
            if "NoJobsWithAffinity" in str( exc ):
                raise Exception( "Old style NS exception answer: NoJobsWithAffinity" )
            raise

        return True


class Scenario43( TestBase ):
    " Scenario 43 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "init worker node, init another worker node, " \
               "wait till registration is exceeded (43)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        stats = self.ns.getWorkerNodeStat( 'TEST' )
        if len( stats ) != 0:
            return False

        guid = self.ns.initWorkerNode( 'TEST', 9000 )
        stats = self.ns.getWorkerNodeStat( 'TEST' )

        # Check the worker node guid
        if len( stats ) != 1:
            return False
        if guid not in stats[ 0 ]:
            return False

        guid1 = self.ns.initWorkerNode( 'TEST', 9001 )
        stats = self.ns.getWorkerNodeStat( 'TEST' )

        # Check the both worker nodes guids
        if len( stats ) != 2:
            return False

        combined = " ".join( stats )
        if 'grid_cli @ localhost UDP:9001' not in combined:
            return False
        if guid not in combined:
            return False
        if 'grid_cli @ localhost UDP:9000' not in combined:
            return False
        if guid1 not in combined:
            return False

        # Wait till the registration time is expired
        time.sleep( 61 )

        stats = self.ns.getWorkerNodeStat( 'TEST' )
        if len( stats ) != 0:
            return False
        return True


class Scenario44( TestBase ):
    " Scenario 44 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, init a worker node, get the job, " \
               "clear worker node registration, get the job info (44)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        guid = self.ns.initWorkerNode( 'TEST', 9000 )
        jobID = self.ns.submitJob( 'TEST', 'bla' )

        receivedJobID = self.ns.getJob( 'TEST', -1, 9000, '', guid )
        if receivedJobID != jobID:
            return False

        self.ns.resetWorkerNode( 'TEST', guid )
        info = self.ns.getJobInfo( 'TEST', jobID )
        if getErrMsg( info ) != "'Node closed, cleared'" or \
           info[ 'status' ] != 'Pending' or \
           info[ 'run_counter' ] != '1':
            return False

        return True


class Scenario45( TestBase ):
    " Scenario 45 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Init a worker node, " \
               "init another worker node with the same guid but the other port. " \
               "It will fail until NS is fixed. (45)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        guid = self.ns.initWorkerNode( 'TEST', 9000 )
        try:
            guid = self.ns.initWorkerNode( 'TEST', 9001 )
        except Exception, exc:
            return True

        # The test will fail because this are two different sessions
        return False


class Scenario46( TestBase ):
    " Scenario 46 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Dump the queue, submit 2 jobs, get one job for execution, " \
               "dump the queue (46)"

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

        receivedJobID = self.ns.getJob( 'TEST' )

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


class Scenario47( TestBase ):
    " Scenario 47 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "submit 3 jobs, get all of them for execution, " \
               "complete all of them, get jobs for reading, " \
               "confirm reading for the first and third jobs (47)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )


        jobID1 = self.ns.submitJob( 'TEST', 'bla1' )
        jobID2 = self.ns.submitJob( 'TEST', 'bla2' )
        jobID3 = self.ns.submitJob( 'TEST', 'bla3' )

        receivedJobID1 = self.ns.getJob( 'TEST' )
        receivedJobID2 = self.ns.getJob( 'TEST' )
        receivedJobID3 = self.ns.getJob( 'TEST' )

        self.ns.putJob( 'TEST', receivedJobID1, 0, 'bla' )
        self.ns.putJob( 'TEST', receivedJobID2, 0, 'bla' )
        self.ns.putJob( 'TEST', receivedJobID3, 0, 'bla' )

        groupID, jobs = self.ns.getJobsForReading( 'TEST', 3 )
        if len( jobs ) != 3:
            return False

        self.ns.confirmJobRead( 'TEST', groupID,
                                [ receivedJobID1, receivedJobID3 ] )

        info = self.ns.getQueueDump( 'TEST' )
        statuses = []
        for item in info:
            if item.startswith( "status:" ):
                statuses.append( item )
        statuses.sort()

        if len( statuses ) != 3:
            return False

        if statuses[ 0 ] != 'status: Confirmed' or \
           statuses[ 1 ] != 'status: Confirmed' or \
           statuses[ 2 ] != 'status: Reading':
            return False

        return True



class Scenario48( TestBase ):
    " Scenario 48 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job for execution, " \
               "commit the job, get the job for reading, " \
               "rollback the job reading (48)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )


        jobID1 = self.ns.submitJob( 'TEST', 'bla1' )
        receivedJobID1 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', receivedJobID1, 0, 'bla' )

        groupID, jobs = self.ns.getJobsForReading( 'TEST', 1 )
        if len( jobs ) != 1:
            return False

        info = self.ns.getJobInfo( 'TEST', jobID1 )
        # Check status(Reading)

        self.ns.rollbackJobRead( 'TEST', groupID, [ jobID1 ] )

        info = self.ns.getJobInfo( 'TEST', jobID1 )
        if info[ 'status' ] != 'Done':
            return False
        return True


class Scenario49( TestBase ):
    " Scenario 49 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get job for execution, " \
               "commit the job, get the job for reading with timeout, " \
               "wait till the timeout is over, get the job info (49)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )


        jobID1 = self.ns.submitJob( 'TEST', 'bla1' )
        receivedJobID1 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', receivedJobID1, 0, 'bla' )

        groupID, jobs = self.ns.getJobsForReading( 'TEST', 1, 6 )
        if len( jobs ) != 1:
            print >> sys.stderr, "Scenario 049.\n" \
                                 "Expected: jobs for reading == 1, " \
                                 "received: " + str( len( jobs ) )
            return False

        info = self.ns.getJobInfo( 'TEST', jobID1 )
        if info[ 'status' ] != 'Reading' or \
           info[ 'run_timeout' ] != '6':
            print >> sys.stderr, "Scenario 049.\n" \
                                 "Expected: run_timeout == 6, " \
                                 "received " + info[ 'run_timeout' ] + "\n" \
                                 "Expected: status == Reading, " \
                                 "received " + info[ 'status' ]
            return False

        time.sleep( 16 )

        info = self.ns.getJobInfo( 'TEST', jobID1 )
        if info[ 'status' ] != 'Done':
            print >> sys.stderr, "Scenario 049.\n" \
                                 "Expected: status == Done, " \
                                 "received " + info[ 'status' ]
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
               "check the job run counter. " \
               "It will fail until NS is fixed. (50)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )


        jobID1 = self.ns.submitJob( 'TEST', 'bla1' )
        receivedJobID1 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', receivedJobID1, 0, 'bla' )

        groupID, jobs = self.ns.getJobsForReading( 'TEST', 1 )
        if len( jobs ) != 1:
            return False

        info = self.ns.getJobInfo( 'TEST', jobID1 )
        if info[ 'run_counter' ] != '1':
            return False
        # Will fail until NS is fixed: run_counter is mistakenly 2 here
        return True


class Scenario51( TestBase ):
    " Scenario 51 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job for execution, " \
               "commit the job, get the job for reading, " \
               "fail reading (51)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID1 = self.ns.submitJob( 'TEST', 'bla1' )
        receivedJobID1 = self.ns.getJob( 'TEST' )
        self.ns.putJob( 'TEST', receivedJobID1, 0, 'bla' )

        groupID, jobs = self.ns.getJobsForReading( 'TEST', 1 )
        if len( jobs ) != 1:
            return False

        self.ns.failJobReading( 'TEST', groupID, [ jobID1 ], 'Error-message' )


        info = self.ns.getJobInfo( 'TEST', jobID1 )
        if info[ 'status' ] != 'ReadFailed':
            return False

        return True


class Scenario52( TestBase ):
    " Scenario 52 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Start batch submit, submit two jobs (52)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobIDs = self.ns.submitBatch( 'TEST', [ '"bla1"', '"bla2"' ] )

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
               "submit one job (53)"

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
               "submit two jobs (54)"

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
               "get the job fast status. " \
               "It will fail until NS is fixed. (55)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        receivedJobID = self.ns.getJob( 'TEST' )
        self.ns.failJob( 'TEST', jobID, 1, "error-output" )

        return self.ns.getFastJobStatus( 'TEST', jobID ) == 4


class Scenario56( TestBase ):
    " Scenario 56 "

    def __init__( self, netschedule ):
        TestBase.__init__( self, netschedule )
        return

    @staticmethod
    def getScenario():
        " Should return a textual description of the test "
        return "Submit a job, get the job for execution, cancel the job, " \
               "reschedule the job, check the job status. " \
               "It will fail until NS is fixed. (56)"

    def execute( self ):
        " Should return True if the execution completed successfully "
        self.clear()
        self.ns.start()
        time.sleep( 1 )
        if not self.ns.isRunning():
            raise Exception( "Cannot start netschedule" )

        jobID = self.ns.submitJob( 'TEST', 'bla' )
        receivedJobID = self.ns.getJob( 'TEST' )
        self.ns.cancelJob( 'TEST', jobID )
        self.ns.rescheduleJob( 'TEST', jobID )

        info = self.ns.getJobInfo( 'TEST', jobID )
        if info[ 'status' ] != 'Pending':
            return False

        return True


# The script execution entry point
if __name__ == "__main__":
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        print >> sys.stderr, "Ctrl + C received"
        returnValue = 2

    except Exception, excpt:
        print >> sys.stderr, str( excpt )
        raise
        returnValue = 1

    sys.exit( returnValue )

