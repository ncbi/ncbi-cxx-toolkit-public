#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Sends all the NetSchedule supported commands
"""

import re, socket, sys
from optparse import OptionParser

# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs


VERBOSE = False
COMMUNICATION_TIMEOUT = 5


def printStderr( msg ):
    " Prints onto stderr with a prefix "
    print >> sys.stderr, "NetSchedule commands script. " + msg
    return

def printVerbose( msg ):
    " Prints stdout message conditionally "
    if VERBOSE:
        print msg
    return


class UnexpectedNSResponse( Exception ):
    " NetSchedule produces trash "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSShuttingDown( Exception ):
    " NetSchedule is shutting down "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSAccessDenied( Exception ):
    " No permissions for commands "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSError( Exception ):
    " NetSchedule answer has ERR:... "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return


class NSDirectConnect:
    " Serves the direct connection to the server "
    def __init__( self, connPoint, queueName ):
        parts = connPoint.split( ":" )
        self.__host = parts[ 0 ]
        if self.__host == "":
            self.__host = "localhost"
        self.__port = int( parts[ 1 ] )
        self.__queue = queueName
        self.__sock = None
        self.__readBuf = ""
        self.__replyDelimiter = re.compile( r'\r\n|\n|\r|\0' )
        return

    def getQueueName( self ):
        return self.__queue

    def connect( self, timeout ):
        " Connects to the server "
        self.__sock = socket.socket()
        self.__sock.settimeout( timeout )
        self.__sock.connect( (self.__host, self.__port) )
        return

    def disconnect( self ):
        " Close direct connection if it was opened "
        if self.__sock is not None:
            self.__sock.close()
        self.__sock = None
        return

    def login( self ):
        " Performs a direct login to NS "
        self.__sock.send( "netschedule_admin client_node=commands_check "
                          "client_session=commands_session\n" +
                          self.__queue + "\n" )
        return

    def execute( self, cmd, multiline = False ):
        " Sends the given command to NS "
        self.__sock.sendall( cmd + "\n" )
        if multiline:
            return self.readMultiLineReply()
        return self.readSingleLineReply()

    def readSingleLineReply( self, multilinePart = False ):
        " Reads a single line reply "
        while True:
            parts = self.__replyDelimiter.split( self.__readBuf, 1 )
            if len( parts ) > 1:
                reply = parts[ 0 ]
                self.__readBuf = parts[ 1 ]
                break

            buf = self.__sock.recv( 8192 )
            if not buf:
                if self.__readBuf:
                    return self.__readBuf.strip()
                raise UnexpectedNSResponse( "Unexpected NS response: None" )
            self.__readBuf += buf

        if reply.startswith( "ERR:" ):
            if "shuttingdown" in reply.lower():
                raise NSShuttingDown( "Server is shutting down" )
            elif "access denied" in reply.lower():
                raise NSAccessDenied( reply.split( ':', 1 )[ 1 ].strip() )
            raise NSError( reply.split( ':', 1 )[ 1 ].strip() )

        if reply.startswith( "OK:" ):
            return reply.split( ':', 1 )[ 1 ].strip()

        if multilinePart:
            # In a multiline case it could be anything, e.g. GETCONF
            return reply

        raise UnexpectedNSResponse( "Unexpected NS response: " + reply.strip() )

    def readMultiLineReply( self ):
        " Reads a multi line reply "
        lines = []
        oneLine = self.readSingleLineReply( True )
        while oneLine != "END":
            if oneLine:
                lines.append( oneLine )
            oneLine = self.readSingleLineReply( True )
        return lines


class NSCommandsSender:

    def __init__( self, nsConnect, allowExceptions ):
        self.__nsConnect = nsConnect
        self.__allowExceptions = allowExceptions

        self.__commands = {
            "GETCONF":              self.__getconf,
            "VERSION":              self.__version,
            "RECO":                 self.__reco,
            "ACNT":                 self.__acnt,
            "QLST":                 self.__qlst,
            "QINF":                 self.__qinf,
            "QINF2":                self.__qinf2,
            "DROPQ":                self.__dropq,
            "QCRE":                 self.__qcre,
            "QDEL":                 self.__qdel,
            "HEALTH":               self.__health,
            "STATUS":               self.__status,
            "STATUS2":              self.__status2,
            "STAT":                 self.__stat,
            "STAT CLIENTS":         self.__statClients,
            "STAT NOTIFICATIONS":   self.__statNotifications,
            "STAT AFFINITIES":      self.__statAffinities,
            "STAT JOBS":            self.__statJobs,
            "STAT GROUPS":          self.__statGroups,
            "STAT QCLASSES":        self.__statQClasses,
            "STAT QUEUES":          self.__statQueues,
            "STAT ALERTS":          self.__statAlerts,
            "STAT ALL":             self.__statAll,
            "STAT SERVICES":        self.__statServices,
            "ACKALERT":             self.__ackAlert,
            "DUMP":                 self.__dump,
            "AFLS":                 self.__afls,
            "GETP":                 self.__getp,
            "GETP2":                self.__getp2,
            "GETC":                 self.__getc,
            "CANCELQ":              self.__cancelq,
            "REFUSESUBMITS":        self.__refusesubmits,
            "QPAUSE":               self.__queuePause,
            "QRESUME":              self.__queueResume,
            "SETQUEUE":             self.__setqueue,
            "MGET":                 self.__mget,
            "SST":                  self.__sst,
            "SST2":                 self.__sst2,
            "SUBMIT":               self.__submit,
            "LISTEN":               self.__listen,
            "CANCEL":               self.__cancel,
            "BATCH":                self.__batch,
            "MPUT":                 self.__mput,
            "CLRN":                 self.__clrn,
            "WST":                  self.__wst,
            "WST2":                 self.__wst2,
            "CHAFF":                self.__chaff,
            "SETAFF":               self.__setaff,
            "GET":                  self.__get,
            "GET2":                 self.__get2,
            "PUT":                  self.__put,
            "PUT2":                 self.__put2,
            "RETURN":               self.__return,
            "RETURN2":              self.__return2,
            "WGET":                 self.__wget,
            "CWGET":                self.__cwget,
            "SETCLIENTDATA":        self.__setclientdata,
            "FPUT":                 self.__fput,
            "FPUT2":                self.__fput2,
            "JXCG":                 self.__jxcg,
            "JDEX":                 self.__jdex,
            "READ":                 self.__read,
            "CFRM":                 self.__cfrm,
            "FRED":                 self.__fred,
            "RDRB":                 self.__rdrb }
        return

    def visitAll( self ):
        for cmd in self.__commands.keys():
            self.visit( cmd )
        return

    def visit( self, command ):
        if command not in self.__commands:
            raise Exception( "Unsupported command: " + str( command ) )

        try:
            printVerbose( "Sending " + command + " command..." )
            self.__commands[ command ]()
            printVerbose( "Finished" )
        except Exception, exc:
            if self.__allowExceptions:
                raise
            printStderr( str( exc ) )
            printStderr( "Ignore and continue." )
        except:
            if self.__allowExceptions:
                raise
            printStderr( "Unknown exception. Ignore and continue." )
        return

    def __getconf( self ):
        self.__nsConnect.execute( "GETCONF effective=0", True )
        self.__nsConnect.execute( "GETCONF effective=1", True )
        return

    def __version( self ):
        self.__nsConnect.execute( "VERSION", False )
        return

    def __reco( self ):
        self.__nsConnect.execute( "RECO", False )
        return

    def __acnt( self ):
        self.__nsConnect.execute( "ACNT", False )
        return

    def __qlst( self ):
        self.__nsConnect.execute( "QLST", False )
        return

    def __qinf( self ):
        self.__nsConnect.execute( "QINF " + self.__nsConnect.getQueueName(),
                                  False )
        return

    def __qinf2( self ):
        self.__nsConnect.execute( "QINF2 " + self.__nsConnect.getQueueName(),
                                  False )
        return

    def __dropq( self ):
        self.__nsConnect.execute( "DROPQ", False )
        return

    def __qcre( self ):
        pass

    def __qdel( self ):
        pass

    def __health( self ):
        self.__nsConnect.execute( "HEALTH", False )
        return

    def __status( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "STATUS " + jobKey, False )
        self.__nsConnect.execute( "CANCEL " + jobKey, False )
        return

    def __status2( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "STATUS2 " + jobKey, False )
        self.__nsConnect.execute( "CANCEL " + jobKey, False )
        return

    def __stat( self ):
        self.__nsConnect.execute( "STAT", True )
        return

    def __statClients( self ):
        self.__nsConnect.execute( "STAT CLIENTS VERBOSE", True )
        return

    def __statNotifications( self ):
        self.__submit()
        self.__nsConnect.execute( "STAT NOTIFICATIONS VERBOSE", True )
        return

    def __statAffinities( self ):
        self.__submit()
        self.__nsConnect.execute( "STAT AFFINITIES VERBOSE", True )
        return

    def __statJobs( self ):
        self.__submit()
        self.__nsConnect.execute( "STAT JOBS", True )
        return

    def __statGroups( self ):
        self.__submit()
        self.__nsConnect.execute( "STAT GROUPS VERBOSE", True )
        return

    def __statQClasses( self ):
        self.__nsConnect.execute( "STAT QCLASSES", True )
        return

    def __statQueues( self ):
        self.__nsConnect.execute( "STAT QUEUES", True )
        return

    def __statAlerts( self ):
        self.__nsConnect.execute( "STAT ALERTS", True )
        return

    def __statAll( self ):
        self.__nsConnect.execute( "STAT ALL", True )
        return

    def __statServices( self ):
        self.__nsConnect.execute( "STAT SERVICES", False )
        return

    def __ackAlert( self ):
        self.__nsConnect.execute( "ACKALERT config somebody", False )
        return

    def __dump( self ):
        self.__submit()
        self.__nsConnect.execute( "DUMP", True )
        return

    def __afls( self ):
        self.__submit()
        self.__nsConnect.execute( "AFLS", False )
        return

    def __getp( self ):
        self.__nsConnect.execute( "GETP", False )
        return

    def __getp2( self ):
        self.__nsConnect.execute( "GETP2", False )
        return

    def __getc( self ):
        self.__nsConnect.execute( "GETC", True )
        return

    def __cancelq( self ):
        self.__nsConnect.execute( "CANCELQ", False )
        return

    def __refusesubmits( self ):
        self.__nsConnect.execute( "REFUSESUBMITS 1", False )
        self.__nsConnect.execute( "REFUSESUBMITS 0", False )
        return

    def __queuePause( self ):
        self.__nsConnect.execute( "QPAUSE pullback=0", False )
        self.__nsConnect.execute( "QRESUME", False )
        return

    def __queueResume( self ):
        self.__nsConnect.execute( "QPAUSE pullback=1", False )
        self.__nsConnect.execute( "QRESUME", False )
        return

    def __setqueue( self ):
        self.__nsConnect.execute( "SETQUEUE " + self.__nsConnect.getQueueName(),
                                  False )
        return

    def __mget( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "MGET " + jobKey, False )
        return

    def __sst( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "SST " + jobKey, False )
        return

    def __sst2( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "SST2 " + jobKey, False )
        return

    def __submit( self ):
        output = self.__nsConnect.execute( "SUBMIT input=testJob "
                                           "port=9877 timeout=5 "
                                           "aff=a1 group=g1", False )
        return output

    def __listen( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "LISTEN " + jobKey + " 9878 5", False )
        return

    def __cancel( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "CANCEL " + jobKey, False )
        return

    def __batch( self ):
        self.__nsConnect.execute( "BSUB", False )
        self.__nsConnect.execute( "BTCH 2\ninput1\ninput2\nENDB", False )
        self.__nsConnect.execute( "ENDS", False )
        return

    def __mput( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "MPUT " + jobKey + " MyMessage", False )
        return

    def __clrn( self ):
        self.__nsConnect.execute( "CLRN", False )
        return

    def __wst( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "WST " + jobKey, False )
        return

    def __wst2( self ):
        jobKey = self.__submit()
        self.__nsConnect.execute( "WST2 " + jobKey, False )
        return

    def __chaff( self ):
        self.__nsConnect.execute( "CHAFF add=a1,a2,a3 del=d1,d2,d3", False )
        return

    def __setaff( self ):
        self.__nsConnect.execute( "SETAFF aff=a11,a12,a13", False )
        return

    def __get( self ):
        self.__submit()
        output = self.__nsConnect.execute( "GET", False )
        return output.split( ' ' )[ 0 ]

    def __get2( self ):
        self.__submit()
        output = self.__nsConnect.execute( "GET2 wnode_aff=0 any_aff=0 "
                                           "exclusive_new_aff=0 aff=a1", False )

        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        return jobKey, authToken

    def __put( self ):
        jobKey = self.__get()
        self.__nsConnect.execute( "PUT " + jobKey + " 0 MyOutput", False )
        return jobKey

    def __put2( self ):
        jobKey, authToken = self.__get2()
        self.__nsConnect.execute( "PUT2 job_key=" + jobKey +
                                  " auth_token=" + authToken +
                                  " job_return_code=0 output=nooutput", False )
        return jobKey

    def __return( self ):
        jobKey = self.__get()
        self.__nsConnect.execute( "RETURN job_key=" + jobKey, False )
        return

    def __return2( self ):
        jobKey, authToken = self.__get2()
        self.__nsConnect.execute( "RETURN2 job_key=" + jobKey +
                                  " auth_token=" + authToken, False )
        return

    def __wget( self ):
        self.__nsConnect.execute( "WGET 9978 5 aff=nonexisting", False )
        return

    def __cwget( self ):
        self.__nsConnect.execute( "CWGET", False )
        return

    def __setclientdata( self ):
        self.__nsConnect.execute( "SETCLIENTDATA data=something", False )
        return

    def __fput( self ):
        jobKey = self.__get()
        self.__nsConnect.execute( "FPUT job_key=" + jobKey + 
                                  " err_msg=nomessage output=nooutput "
                                  "job_return_code=1", False )
        return

    def __fput2( self ):
        jobKey, authToken = self.__get2()
        self.__nsConnect.execute( "FPUT2 job_key=" + jobKey +
                                  " auth_token=" + authToken +
                                  " err_msg=nomessage output=nooutput"
                                  " job_return_code=1", False )
        return

    def __jxcg( self ):
        jobKey, authToken = self.__get2()
        self.__nsConnect.execute( "JXCG job_key=" + jobKey +
                                  " job_return_code=1 output=nooutput", False )
        return

    def __jdex( self ):
        jobKey, authToken = self.__get2()
        self.__nsConnect.execute( "JDEX job_key=" + jobKey + " 5", False )
        return

    def __read( self ):
        self.__put2()
        output = self.__nsConnect.execute( "READ", False )

        values = parse_qs( output, True, True )
        jobKey = values[ 'job_key' ][ 0 ]
        authToken = values[ 'auth_token' ][ 0 ]
        return jobKey, authToken

    def __cfrm( self ):
        jobKey, authToken = self.__read()
        self.__nsConnect.execute( "CFRM job_key=" + jobKey +
                                  " auth_token=" + authToken, False )
        return

    def __fred( self ):
        jobKey, authToken = self.__read()
        self.__nsConnect.execute( "FRED job_key=" + jobKey +
                                  " auth_token=" + authToken, False )
        return

    def __rdrb( self ):
        jobKey, authToken = self.__read()
        self.__nsConnect.execute( "RDRB job_key=" + jobKey +
                                  " auth_token=" + authToken, False )
        return



def main():
    " main function for the netschedule commands check "

    parser = OptionParser(
    """
    %prog  <connection point>  <queue name>
    Sends all the supported commands to NetSchedule (except QUIT and SHUTDOWN)
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "-c", "--count",
                       dest="count", default="1",
                       help="number of loops (default: 1)" )

    # parse the command line options
    options, args = parser.parse_args()
    global VERBOSE
    VERBOSE = options.verbose

    try:
        count = int( options.count )
    except:
        printStderr( "Loop count must be integer" )
        return 4

    if count <= 0:
        printStderr( "Loop count must be > 0" )
        return 4

    if len( args ) < 2:
        printStderr( "Incorrect number of arguments" )
        return 4

    connectionPoint = args[ 0 ]
    if len( connectionPoint.split( ":" ) ) != 2:
        printStderr( "invalid connection point format. Expected host:port" )
        return 4

    queueName = args[ 1 ]

    printVerbose( "Connection point: " + connectionPoint + "\n"
                  "Queue: " + queueName + "\n"
                  "Loop count: " + str( count ) )


    nsConnect = NSDirectConnect( connectionPoint, queueName )
    nsConnect.connect( COMMUNICATION_TIMEOUT )
    nsConnect.login()

    while count > 0:
        commandsSender = NSCommandsSender( nsConnect, True )
        commandsSender.visitAll()
        count -= 1
    return 0




# The script execution entry point
if __name__ == "__main__":
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        printStderr( "Ctrl + C received" )
        returnValue = 1
    except Exception, excpt:
        printStderr( str( excpt ) )
        returnValue = 2
    except:
        printStderr( "Generic unknown script error" )
        returnValue = 3

    sys.exit( returnValue )
