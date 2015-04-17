#!/opt/python-2.7/bin/python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
NetStorage server health check script
"""

import sys, datetime, socket, os
from optparse import OptionParser
from ncbi_grid_1_1.ncbi import json_over_uttp, uttp
import pprint


# Defines three script modes
class ScriptMode:
    LBSMD222_OLD_CLIENT = 1
    LBSMD222 = 2

SCRIPT_MODE = ScriptMode.LBSMD222
CLIENT_NAME = "NST health check script"
LBSMD_SERVICE_NAME = "LBSMDNSTTestService"
APPLICATION = os.path.basename( os.path.realpath( sys.argv[ 0 ] ) )
TEST_OBJ_CONTENT = "Health check data"
TEST_ATTR_NAME = "health_check_attr"
TEST_ATTR_VALUE = "health_check"

VERBOSE = False
COMMUNICATION_TIMEOUT = 1

BASE_RESERVE_CODE = 100
BASE_DOWN_CODE = 111
BASE_STANDBY_CODE = 200
BASE_NO_ACTION_ALERT_CODE = 211
NO_CHANGE_CODE = 123

PENALTY_CURVE = [ 90, 99 ]



# Set the value to None and no log file operations will be done
VERBOSE_LOG_FILE = None
#VERBOSE_LOG_FILE = "/home/satskyse/nst_health_check.log"

SESSIONID = '1111111111111111_0000SID'
NCBI_PHID = 'NST_HEALTH_CHECK.NCBI.PHID'

try:
    hostIP = socket.gethostbyname( socket.gethostname() )
except:
    hostIP = "127.0.0.1"


LOG_FILE = None
if VERBOSE_LOG_FILE is not None:
    try:
        LOG_FILE = open( VERBOSE_LOG_FILE, "a" )
    except:
        LOG_FILE = None


class NSTProtocolError( Exception ):
    " NetStorage response does not fit the protocol "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSTResponseError( Exception ):
    " NetStorage response has an error in it "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSTObjectContentError( Exception ):
    " NetStorage read object content does not match written "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return

class NSTAttrValueError( Exception ):
    " NetStorage read attribute value does not match written "
    def __init__( self, message ):
        Exception.__init__( self, message )
        return


def printVerbose( msg ):
    " Prints stdout message conditionally "
    timestamp = datetime.datetime.now().strftime( '%m-%d-%y %H:%M:%S' )
    if LOG_FILE is not None:
        try:
            LOG_FILE.write( timestamp + " " + msg + "\n" )
            LOG_FILE.flush()
        except:
            pass

    if VERBOSE:
        print timestamp + " " + msg
    return

def printStderr( msg ):
    " Prints onto stderr with a prefix "
    timestamp = datetime.datetime.now().strftime( '%m-%d-%y %H:%M:%S' )
    print >> sys.stderr, timestamp + " NetStorage check script. " + msg
    printVerbose( msg )
    return


LAST_EXIT_CODE = None
# Try to extract last check error code first
if len( sys.argv ) >= 4:
    try:
        LAST_EXIT_CODE = int( sys.argv[ 3 ] )
    except:
        pass


def adjustReturnCode( code ):
    " Adjusts the return code depending on the current script mode "

    if SCRIPT_MODE == ScriptMode.LBSMD222_OLD_CLIENT:
        printVerbose( "ScriptMode.LBSMD222_OLD_CLIENT is ON. "
                      "Adjusting return value. Was: " +
                      str( code ) )
        if code >= BASE_RESERVE_CODE and \
           code <= (BASE_RESERVE_CODE + 10):
            code += BASE_STANDBY_CODE - BASE_RESERVE_CODE

        printVerbose( "Adjusted to: " + str( code ) )

    # Otherwise no adjustments required
    return code


def log( code, message ):
    " Logs if it was not the last check return code "
    adjustedCode = adjustReturnCode( code )
    if str( adjustedCode ) != str( LAST_EXIT_CODE ):
        printStderr( message )
    return code



class NetStorage:
    " Implements communications with a NetStorage server "

    def __init__( self, host_, port_ ):
        self.__commandSN = 0
        self.__sock = None
        self.__host = host_
        self.__port = port_
        self.__nst = None
        return

    def connect( self, timeout ):
        " Establishes a connection to the server "
        socket.socket.write = socket.socket.send
        socket.socket.flush = lambda ignore: ignore
        socket.socket.readline = socket.socket.recv

        self.__sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
        self.__sock.settimeout( timeout )
        self.__sock.connect( ( self.__host, self.__port ) )

        self.__nst = json_over_uttp.MessageExchange( self.__sock, self.__sock )
        return

    def exchange( self, message ):
        " Does the basic exchange "
        self.__commandSN += 1

        message[ "SN" ] = self.__commandSN
        self.printMessage( "Message to server", message )
        response = self.__nst.exchange( message )
        self.printMessage( "Message from server", response )
        self.checkResponse( response )
        return response

    def receive( self ):
        " Receives a single server message "
        response = self.__nst.receive()
        self.printMessage( "Message from server", response )
        return response

    @staticmethod
    def printMessage( prefix, msg ):
        " Prints the message "
        if VERBOSE:
            print prefix + ":"
            prettyPrinter = pprint.PrettyPrinter( indent = 4 )
            prettyPrinter.pprint( msg )
        return

    def sendHello( self, service, metadataOption ):
        " Sends the HELLO message "
        message = { 'Type':         'HELLO',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'Application':  APPLICATION,
                    'Ticket':       'None',
                    'Service':      service,
                    'Metadata':     metadataOption,
                    'Client':       CLIENT_NAME }
        response = self.exchange( message )
        return response

    def createInNetCache( self ):
        " Creates an object in NetCache "
        # Flags for NetCache
        storageFlags = { "Fast": True, "Movable": True }
        message = { 'Type':         'CREATE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'StorageFlags': storageFlags }

        response = self.exchange( message )
        return response

    def writeTestObj( self ):
        " Writes test object content "
        uttp_writer = self.__nst.get_uttp_writer()
        data = uttp_writer.send_chunk( TEST_OBJ_CONTENT )
        if data:
            self.__sock.send(data)
        data = uttp_writer.send_control_symbol('\n')
        if data:
            self.__sock.send(data)
        data = uttp_writer.flush_buf()
        if data:
            self.__sock.send(data)

        response = self.receive()
        self.checkResponse( response )
        return response

    def setAttr( self, objectLoc, attrName, attrValue ):
        " Sends SETATTR message "
        message = { 'Type':         'SETATTR',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc,
                    'AttrName':     attrName,
                    'AttrValue':    attrValue }

        response = self.exchange( message )
        return response

    def getAttr( self, objectLoc, attrName ):
        " Sends GETATTR message "
        message = { 'Type':         'GETATTR',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc,
                    'AttrName':     attrName }

        response = self.exchange( message )
        return response

    def readPrologue( self, objectLoc ):
        " Reads the given object "
        message = { 'Type':         'READ',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }

        response = self.exchange( message )
        return response

    def readTestObj( self ):
        " Reads the test object "
        content = ""
        uttp_reader = self.__nst.get_uttp_reader()

        while True:
            buf = self.__sock.recv( 1024 )
            uttp_reader.set_new_buf(buf)
            while True:
                event = uttp_reader.next_event()
                if event == uttp.Reader.END_OF_BUFFER:
                    break

                if event == uttp.Reader.CHUNK_PART or \
                        event == uttp.Reader.CHUNK:
                    content += uttp_reader.get_chunk()
                elif event == uttp.Reader.CONTROL_SYMBOL:
                    if uttp_reader.get_control_symbol() != '\n':
                        raise NSTProtocolError( "Invalid data stream terminator" )
                    response = self.receive()
                    self.checkResponse( response )
                    return response, content
                else:
                    raise NSTProtocolError( "Unexpected UTTP packet type" )

    def delete( self, objectLoc ):
        " Deletes the given object "
        message = { 'Type':         'DELETE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }
        response = self.exchange( message )
        return response

    @staticmethod
    def checkResponse( response ):
        " Checks the server response message and throws an exception if errors "
        if "Status" not in response:
            raise NSTResponseError( "Server response does not have 'Status'" )
        if response[ "Status" ] != "OK":
            if "Errors" in response:
                # It is a list of errors
                errors = []
                for item in response[ "Errors" ]:
                    code = "???"
                    msg = "???"
                    if "Code" in item:
                        code = str( item[ "Code" ] )
                    if "Message" in item:
                        msg = str( item[ "Message" ] )
                    errors.append( "Code: " + code + " Message: " + msg )
                raise NSTResponseError( "\n".join( errors ) )
            else:
                raise NSTResponseError( "Unknown server error response" )



def writeReadObject( nst ):
    " Creates, writes, reads an object and compares the content "
    response = nst.createInNetCache()
    if "ObjectLoc" not in response:
        raise NSTResponseError( "Cannot find object "
                                "locator in CREATE response" )
    objectLoc = response[ "ObjectLoc" ]
    nst.writeTestObj()
    nst.readPrologue( objectLoc )
    response, content = nst.readTestObj()
    if content != TEST_OBJ_CONTENT:
        raise NSTObjectContentError( "Read object content does not "
                                     "match written" )
    return objectLoc


def safeDelete( nst, objectLoc ):
    " Suppress exceptions while deleting an object "
    if nst is None:
        return
    if objectLoc is None:
        return
    try:
        nst.delete( objectLoc )
    except:
        pass


def main():
    " main function for the netstorage health check "

    lastCheckExitCode = None
    lastCheckRepeatCount = None
    secondsSinceLastCheck = None

    try:
        parser = OptionParser(
        """
        %prog  <service name> <connection point> [ lastCheckCode [ lastCheckRepeatCount [ timeSinceLastCheck ] ] ]
        """ )
        parser.add_option( "-v", "--verbose",
                           action="store_true", dest="verbose", default=False,
                           help="be verbose (default: False)" )

        # parse the command line options
        options, args = parser.parse_args()
        global VERBOSE
        VERBOSE = options.verbose

        if len( args ) < 2:
            return log( BASE_NO_ACTION_ALERT_CODE + 0,
                        "Incorrect number of arguments" )

        # These arguments are always available
        serviceName = args[ 0 ]                 # analysis:ignore
        connectionPoint = args[ 1 ]             # analysis:ignore
        if len( connectionPoint.split( ":" ) ) != 2:
            return log( BASE_NO_ACTION_ALERT_CODE + 1,
                        "invalid connection point format. Expected host:port" )

        # Arguments below may be missed
        if len( args ) > 2:
            lastCheckExitCode = int( args[ 2 ] )
        if len( args ) > 3:
            lastCheckRepeatCount = int( args[ 3 ] )
        if len( args ) > 4:
            secondsSinceLastCheck = int( args[ 4 ] )

        printVerbose( "Service name: " + serviceName )
        printVerbose( "Connection point: " + connectionPoint )
        printVerbose( "Last check exit code: " + str( lastCheckExitCode ) )
        printVerbose( "Last check repeat count: " + str( lastCheckRepeatCount ) )
        printVerbose( "Seconds since last check: " + str( secondsSinceLastCheck ) )
    except Exception, exc:
        return log( BASE_NO_ACTION_ALERT_CODE + 2,
                    "Error processing command line arguments: " + str( exc ) )


    # First stage - connection
    try:
        parts = connectionPoint.split( ":" )
        nst = NetStorage( parts[ 0 ], int( parts[ 1 ] ) )
        nst.connect( COMMUNICATION_TIMEOUT )
    except socket.timeout, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 1,
                     "Error connecting to server: socket timeout" ) )
    except Exception, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 2,
                     "Error connecting to server: " + str( exc ) ) )
    except:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 3,
                     "Unknown check script error at the connecting stage" ) )

    # Second stage - using metadata
    objectLoc = None
    try:
        nst.sendHello( LBSMD_SERVICE_NAME, 'required' )
        objectLoc = writeReadObject( nst )
        nst.setAttr( objectLoc, TEST_ATTR_NAME, TEST_ATTR_VALUE )
        response = nst.getAttr( objectLoc, TEST_ATTR_NAME )
        if "AttrValue" not in response:
            raise NSTResponseError( "Cannot find attribute value "
                                    "in GETARRT response" )
        if response[ "AttrValue" ] != TEST_ATTR_VALUE:
            raise NSTAttrValueError( "Read attribute value does not "
                                     "match written" )
        nst.delete( objectLoc )

        # Everything is fine here
        penaltyValue = pickPenaltyValue( lastCheckExitCode, 0 )
        return penaltyValue
    except socket.timeout, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 1,
                     "Error communicating to server (with metadata): socket timeout" ) )

    except NSTProtocolError, exc:
        safeDelete( nst, objectLoc )
        log( BASE_RESERVE_CODE + 2,
             "NetStorage protocol error (with metadata): " + str( exc ) )
    except NSTResponseError, exc:
        safeDelete( nst, objectLoc )
        log( BASE_RESERVE_CODE + 3,
             "NetStorage response error (with metadata): " + str( exc ) )
    except NSTObjectContentError, exc:
        safeDelete( nst, objectLoc )
        log( BASE_RESERVE_CODE + 4,
             "NetStorage object read/write error (with metadata): " + str( exc ) )
    except NSTAttrValueError, exc:
        safeDelete( nst, objectLoc )
        log( BASE_RESERVE_CODE + 5,
             "NetStorage attribute read/write error (with metadata): " + str( exc ) )
    except Exception, exc:
        safeDelete( nst, objectLoc )
        log( BASE_NO_ACTION_ALERT_CODE + 3,
             "Object life cycle error (with metadata): " + str( exc ) )
    except:
        safeDelete( nst, objectLoc )
        log( BASE_NO_ACTION_ALERT_CODE + 4,
             "Unknown object life cycle error (with metadata)" )


    # Here: there was a problem in a meta data involving cycle
    #       try without metadata
    objectLoc = None
    try:
        nst.sendHello( LBSMD_SERVICE_NAME, 'disabled' )
        objectLoc = writeReadObject( nst )
        nst.delete( objectLoc )
    except socket.timeout, exc:
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 1,
                     "Error communicating to server (without metadata): socket timeout" ) )
    except NSTProtocolError, exc:
        safeDelete( nst, objectLoc )
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 2,
                     "NetStorage protocol error (without metadata): " + str( exc ) ) )
    except NSTResponseError, exc:
        safeDelete( nst, objectLoc )
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 3,
                     "NetStorage response error (without metadata): " + str( exc ) ) )
    except NSTObjectContentError, exc:
        safeDelete( nst, objectLoc )
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_RESERVE_CODE + 4,
                     "NetStorage object read/write error (without metadata): " + str( exc ) ) )
    except Exception, exc:
        safeDelete( nst, objectLoc )
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_NO_ACTION_ALERT_CODE + 3,
                     "NetStorage object life cycle error (without metadata): " + str( exc ) ) )
    except:
        safeDelete( nst, objectLoc )
        return pickPenaltyValue( lastCheckExitCode,
                log( BASE_NO_ACTION_ALERT_CODE + 4,
                     "Unknown NetStorage object life cycle error (without metadata)" ) )

    # Everything is fine here
    penaltyValue = pickPenaltyValue( lastCheckExitCode, 0 )
    return penaltyValue


def pickPenaltyValue( lastCheckExitCode, calculatedCode ):
    " Provides a new value for the returned penalty "
    if lastCheckExitCode is None:
        lastCheckExitCode = BASE_RESERVE_CODE      # By default the previous
                                                    # return code is 100

    # No intersection => the calculated value
    # There is intersection => the point of intersection
    if lastCheckExitCode == calculatedCode:
        return calculatedCode

    minRange = min( [lastCheckExitCode, calculatedCode] )
    maxRange = max( [lastCheckExitCode, calculatedCode] )
    crossedPoints = []
    for value in PENALTY_CURVE:
        if value > minRange and value < maxRange:
            crossedPoints.append( value )

    if len( crossedPoints ) == 0:
        return calculatedCode

    if calculatedCode < lastCheckExitCode:
        # Getting better
        return crossedPoints[ -1 ]
    # Getting worse
    return crossedPoints[ 0 ]



# The script execution entry point
if __name__ == "__main__":
    printVerbose( "---------- Start ----------" )
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        printStderr( "Ctrl + C received" )
        returnValue = BASE_NO_ACTION_ALERT_CODE + 7
    except Exception, excpt:
        printStderr( str( excpt ) )
        returnValue = BASE_NO_ACTION_ALERT_CODE + 8
    except:
        printStderr( "Generic unknown script error" )
        returnValue = BASE_NO_ACTION_ALERT_CODE + 9


    printVerbose( "Return code: " + str( returnValue ) )
    returnValue = adjustReturnCode( returnValue )

    if str( LAST_EXIT_CODE ) == str( returnValue ):
        returnValue = NO_CHANGE_CODE

    sys.exit( returnValue )
