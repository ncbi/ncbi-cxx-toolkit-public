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


VERBOSE = False
COMMUNICATION_TIMEOUT = 1


def printStderr( msg ):
    " Prints onto stderr with a prefix "
    print >> sys.stderr, "NetSchedule check script. " + msg
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

    def execute( self, cmd, multiline = False):
        " Sends the given command to NS "
        self.__sock.sendall( cmd + "\n" )
        if multiline:
            return self.readMultiLineReply()
        return self.readSingleLineReply()

    def readSingleLineReply( self ):
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

        if reply.startswith( "OK:" ):
            return reply.split( ':', 1 )[ 1 ].strip()
        if reply.startswith( "ERR:" ):
            if "shuttingdown" in reply.lower():
                raise NSShuttingDown( "Server is shutting down" )
            elif "access denied" in reply.lower():
                raise NSAccessDenied( reply.split( ':', 1 )[ 1 ].strip() )
            raise NSError( reply.split( ':', 1 )[ 1 ].strip() )
        raise UnexpectedNSResponse( "Unexpected NS response: " + reply.strip() )

    def readMultiLineReply( self ):
        " Reads a multi line reply "
        lines = []
        oneLine = self.readSingleLineReply()
        while oneLine != "END":
            if oneLine:
                lines.append( oneLine )
            oneLine = self.readSingleLineReply()
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
            "DUMP":                 self.__dump,
            "AFLS":                 self.__afls,
            "GETP":                 self.__getp,
            "GETC":                 self.__getc,
            "CANCELQ":              self.__cancelq,
            "REFUSESUBMITS":        self.__refusesubmits,
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

    def __statClients( self ):
        pass

    def __statNotifications( self ):
        pass

    def __statAffinities( self ):
        pass

    def __statJobs( self ):
        pass

    def __statGroups( self ):
        pass

    def __statQClasses( self ):
        pass

    def __statQueues( self ):
        pass

    def __dump( self ):
        pass

    def __afls( self ):
        pass

    def __getp( self ):
        pass

    def __getc( self ):
        pass

    def __cancelq( self ):
        pass

    def __refusesubmits( self ):
        pass

    def __setqueue( self ):
        pass

    def __mget( self ):
        pass

    def __sst( self ):
        pass

    def __sst2( self ):
        pass

    def __submit( self ):
        output = self.__nsConnect.execute( "SUBMIT testJob", False )
        if not output.startswith( "OK:" ):
            raise UnexpectedNSResponse( "Submit does not return job key" )
        return output.replace( "OK:", "", 1 ).strip()

    def __listen( self ):
        pass

    def __cancel( self ):
        pass

    def __batch( self ):
        pass

    def __mput( self ):
        pass

    def __clrn( self ):
        pass

    def __wst( self ):
        pass

    def __wst2( self ):
        pass

    def __chaff( self ):
        pass

    def __setaff( self ):
        pass

    def __get( self ):
        pass

    def __get2( self ):
        pass

    def __put( self ):
        pass

    def __put2( self ):
        pass

    def __return( self ):
        pass

    def __return2( self ):
        pass

    def __wget( self ):
        pass

    def __cwget( self ):
        pass

    def __fput( self ):
        pass

    def __fput2( self ):
        pass

    def __jxcg( self ):
        pass

    def __jdex( self ):
        pass

    def __read( self ):
        pass

    def __cfrm( self ):
        pass

    def __fred( self ):
        pass

    def __rdrb( self ):
        pass




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

    # parse the command line options
    options, args = parser.parse_args()
    global VERBOSE
    VERBOSE = options.verbose

    if len( args ) < 2:
        printStderr( "Incorrect number of arguments" )
        return 4

    connectionPoint = args[ 0 ]
    if len( connectionPoint.split( ":" ) ) != 2:
        printStderr( "invalid connection point format. Expected host:port" )
        return 4

    queueName = args[ 1 ]

    printVerbose( "Connection point: " + connectionPoint + "\n"
                  "Queue: " + queueName )


    nsConnect = NSDirectConnect( connectionPoint, queueName )
    nsConnect.connect( COMMUNICATION_TIMEOUT )
    nsConnect.login()

    commandsSender = NSCommandsSender( nsConnect, True )
    commandsSender.visitAll()
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
