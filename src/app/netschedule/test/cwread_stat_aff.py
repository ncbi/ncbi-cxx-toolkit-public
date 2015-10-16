#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Sends two commands from parallel threads: STAT AFFINITIES & CWREAD
That combination lead to a deadlock in NS
"""

import re, socket, sys
from optparse import OptionParser
from multiprocessing import Process


# Works for python 2.5. Python 2.7 has it in urlparse module
from cgi import parse_qs


VERBOSE = False
COMMUNICATION_TIMEOUT = 25


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




def statAffinitiesProc( connectionPoint, queueName ):

    nsConnect = NSDirectConnect( connectionPoint, queueName )
    nsConnect.connect( COMMUNICATION_TIMEOUT )
    nsConnect.login()

    l = 0
    while True:
        try:
            printVerbose( "STAT AFFINITIES PROC #" + str( l ) )
            nsConnect.execute( "SUBMIT input=testJob "
                               "port=9877 timeout=5 "
                               "aff=a1 group=g1", False )
            nsConnect.execute( "STAT AFFINITIES VERBOSE", True )
            l += 1
        except Exception, exc:
            printStderr( "STAT AFFINITIES PROC error: " + str( exc ) )
            return
        except:
            printStderr( "STAT AFFINITIES PROC unknown error" )
            return
    return


def cwreadProc( connectionPoint, queueName ):

    nsConnect = NSDirectConnect( connectionPoint, queueName )
    nsConnect.connect( COMMUNICATION_TIMEOUT )
    nsConnect.login()

    l = 0
    while True:
        try:
            printVerbose( "CWREAD PROC #" + str( l ) )
            nsConnect.execute( "READ2 reader_aff=0 any_aff=0 "
                               "aff=nonexisting port=9008 timeout=17",
                               False )
            nsConnect.execute( "CWREAD", False )
            l += 1
        except Exception, exc:
            printStderr( "CWREAD PROC error: " + str( exc ) )
            return
        except:
            printStderr( "CWREAD PROC unknown error" )
            return
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

    p1 = Process( target = statAffinitiesProc,
                  args = ( connectionPoint, queueName ) )
    p2 = Process( target = cwreadProc,
                  args = ( connectionPoint, queueName ) )

    p1.start()
    p2.start()

    p1.join()
    p2.join()

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

