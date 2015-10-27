#!/opt/python-2.7/bin/python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
A very basic NetStorage server test
"""

import sys, datetime, socket, os
from optparse import OptionParser
from ncbi_grid_1_1.ncbi import json_over_uttp, uttp
import pprint
import random


CLIENT_NAME = "NST Basic UT"
APPLICATION = os.path.basename( os.path.realpath( sys.argv[ 0 ] ) )
TEST_OBJ_CONTENT = "NST Basic UT data"
TEST_ATTR_NAME = "basic_ut_attr_name"
TEST_ATTR_VALUE = "basic_ut_attr_value"

VERBOSE = False


def generateSessionID():
    " Generates the session ID in an appropriate format "
    # It sould be like 1111111111111111_0000SID
    return str( random.randint( 1111111111111111,
                                9999999999999999 ) ) + "_0000SID"

SESSIONID = generateSessionID()
NCBI_PHID = 'NST_BASIC_UT_PHID'

try:
    hostIP = socket.gethostbyname( socket.gethostname() )
except:
    hostIP = "127.0.0.1"



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
    if VERBOSE:
        timestamp = datetime.datetime.now().strftime( '%m-%d-%y %H:%M:%S' )
        print timestamp + " " + msg
    return

def printStderr( msg ):
    " Prints onto stderr with a prefix "
    timestamp = datetime.datetime.now().strftime( '%m-%d-%y %H:%M:%S' )
    print >> sys.stderr, timestamp + " NetStorage check script. " + msg
    printVerbose( msg )
    return



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
                    'Metadata':     metadataOption,
                    'Client':       CLIENT_NAME }
        if service:
            message[ "Service" ] = service
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

    try:
        parser = OptionParser(
        """
        %prog  <connection point>
        """ )
        parser.add_option( "-v", "--verbose",
                           action="store_true", dest="verbose", default=False,
                           help="be verbose (default: False)" )
        parser.add_option( "--service", dest="service", default=None,
                           help="service name for the HELLO message" )
        parser.add_option( "--loops", dest="loops", default=1,
                           help="number of loops" )
        parser.add_option( "--timeout", dest="timeout", default=5,
                           help="communication timeout" )
        parser.add_option( "--no-db", action="store_true", dest="no_db", default=False,
                           help="without metadata (default: with metadata)" )

        # parse the command line options
        options, args = parser.parse_args()
        global VERBOSE
        VERBOSE = options.verbose

        if len( args ) != 1:
            printStderr( "Incorrect number of arguments" )
            return 1

        # These arguments are always available
        connectionPoint = args[ 0 ]
        if len( connectionPoint.split( ":" ) ) != 2:
            printStderr( "invalid connection point format. Expected host:port" )
            return 1

        options.loops = int( options.loops )
        if options.loops <= 0:
            printStderr( "Number of loops must be an integer > 0" )
            return 1

        options.timeout = int( options.timeout )
        if options.timeout <= 0:
            printStderr( "Communication timeout must be an integer > 0" )
            return 1

        printVerbose( "Connection point: " + connectionPoint )
        printVerbose( "Service name: " + str( options.service ) )
        printVerbose( "Number of loops: " + str( options.loops ) )
        printVerbose( "Communication timeout: " + str( options.timeout ) )
        printVerbose( "With metadata: " + str( not options.no_db ) )
    except Exception, exc:
        printStderr( "Error processing command line arguments: " + str( exc ) )
        return 1


    # First stage - connection
    try:
        parts = connectionPoint.split( ":" )
        nst = NetStorage( parts[ 0 ], int( parts[ 1 ] ) )
        nst.connect( options.timeout )
    except socket.timeout, exc:
        printStderr( "Error connecting to server: socket timeout" )
        return 2
    except Exception, exc:
        printStderr( "Error connecting to server: " + str( exc ) )
        return 2
    except:
        printStderr( "Unknown check script error at the connecting stage" )
        return 2

    count = 0
    while options.loops > 0:
        count += 1
        printVerbose( "Loop #" + str( count ) )
        if options.no_db:
            withoutMetadata( nst, options.service )
        else:
            withMetadata( nst, options.service )
        options.loops -= 1

    return 0



def withMetadata( nst, service ):

    # Second stage - using metadata
    objectLoc = None
    try:
        nst.sendHello( service, 'required' )
        objectLoc = writeReadObject( nst )
        nst.setAttr( objectLoc, TEST_ATTR_NAME, TEST_ATTR_VALUE )
        response = nst.getAttr( objectLoc, TEST_ATTR_NAME )
        if "AttrValue" not in response:
            raise NSTResponseError( "Cannot find attribute value "
                                    "in GETATTR response" )
        if response[ "AttrValue" ] != TEST_ATTR_VALUE:
            raise NSTAttrValueError( "Read attribute value does not "
                                     "match written" )
        nst.delete( objectLoc )
    except socket.timeout, exc:
        raise Exception( "Error communicating to server (with metadata): socket timeout" )

    except NSTProtocolError, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "NetStorage protocol error (with metadata): " + str( exc ) )
    except NSTResponseError, exc:
        safeDelete( nst, objectLoc )
        raise Exception("NetStorage response error (with metadata): " + str( exc ) )
    except NSTObjectContentError, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "NetStorage object read/write error (with metadata): " + str( exc ) )
    except NSTAttrValueError, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "NetStorage attribute read/write error (with metadata): " + str( exc ) )
    except Exception, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "Object life cycle error (with metadata): " + str( exc ) )
    except:
        safeDelete( nst, objectLoc )
        raise Exception( "Unknown object life cycle error (with metadata)" )
    return 0


def withoutMetadata( nst, service ):

    # Here: there was a problem in a meta data involving cycle
    #       try without metadata
    objectLoc = None
    try:
        nst.sendHello( service, 'disabled' )
        objectLoc = writeReadObject( nst )
        nst.delete( objectLoc )
    except socket.timeout, exc:
        raise Exception( "Error communicating to server (without metadata): socket timeout" )
    except NSTProtocolError, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "NetStorage protocol error (without metadata): " + str( exc ) )
    except NSTResponseError, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "NetStorage response error (without metadata): " + str( exc ) )
    except NSTObjectContentError, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "NetStorage object read/write error (without metadata): " + str( exc ) )
    except Exception, exc:
        safeDelete( nst, objectLoc )
        raise Exception( "NetStorage object life cycle error (without metadata): " + str( exc ) )
    except:
        safeDelete( nst, objectLoc )
        raise Exception( "Unknown NetStorage object life cycle error (without metadata)" )
    return 0


# The script execution entry point
if __name__ == "__main__":
    printVerbose( "---------- Start ----------" )
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        printStderr( "Ctrl + C received" )
        returnValue = 4
    except Exception, excpt:
        printStderr( str( excpt ) )
        returnValue = 5
    except:
        printStderr( "Generic unknown script error" )
        returnValue = 6

    printVerbose( "Return code: " + str( returnValue ) )
    printVerbose( "---------- Finish ---------" )
    sys.exit( returnValue )
