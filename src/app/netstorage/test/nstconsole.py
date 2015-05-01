#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Interactive console to imitate NetStorage packets
"""

import sys
import socket
import errno
from optparse import OptionParser
from ncbi_grid_dev import json_over_uttp, uttp
#from ncbi_grid_1_0.ncbi import json_over_uttp, uttp
import pprint
import readline
#import rlcompleter
import atexit
import os
import simplejson as json

historyPath = os.path.expanduser( "~/.nstconsole" )

readline.parse_and_bind( 'tab: complete' )

def save_rlhistory():
    " Saves the commands history to use it in the future sessions "
    readline.write_history_file( historyPath )
    return

if os.path.exists( historyPath ):
    readline.read_history_file( historyPath )

atexit.register( save_rlhistory )


try:
    hostIP = socket.gethostbyname( socket.gethostname() )
except:
    hostIP = "127.0.0.1"

SESSIONID = '1111111111111111_0000SID'
NCBI_PHID = 'Fake_NCBI_PHID'



class NetStorageConsole:
    " NetStorage debug console implementation "

    def __init__( self, host_, port_ ):
        # Initialize the command map
        self.__commandMap = {
             '?':              [ self.printCommandList,
                                 "prints the list of commands" ],
             'help':           [ self.printHelp,
                                 "prints the list of commands or help for an individual one" ],
             'hello':          [ self.sendHello,
                                 "sends HELLO message; [client name] [service name] [metadata option]" ],
             'bye':            [ self.sendBye,
                                 "sends BYE message; no arguments" ],
             'info':           [ self.sendInfo,
                                 "sends INFO message; no arguments" ],
             'configuration':  [ self.sendConfiguration,
                                 "sends CONFIGURATION message; no arguments" ],
             'shutdown':       [ self.sendShutdown,
                                 "sends SHUTDOWN message; [how: soft (default), hard]" ],
             'getclientsinfo': [ self.sendGetClientsInfo,
                                 "sends GETCLIENTSINFO message; no arguments" ],
             'getobjectinfo':  [ self.sendGetObjectInfo,
                                 "sends GETOBJECTINFO message; <object locator>" ],
             'getattrlist':    [ self.sendGetAttrList,
                                 "sends GETATTRLIST message; <object locator>" ],
             'getclientobjects':
                               [ self.sendGetClientObjects,
                                 "sends GETCLIENTOBJECTS message; <client name> [limit]" ],
             'getattr':        [ self.sendGetAttr,
                                 "sends GETATTR message; <object locator> <attribute name>" ],
             'delattr':        [ self.sendDelAttr,
                                 "sends DELATTR message; <object locator> <attribute name>" ],
             'setattr':        [ self.sendSetAttr,
                                 "sends SETATTR message; <object locator> <attribute name> <attribute value>" ],
             'no-type':        [ self.sendNoType,
                                 "sends a malformed message without a type" ],
             'no-dict':        [ self.sendNoDictionary,
                                 "sends a malformed non-dictionary message" ],
             'createft':       [ self.sendCreateOnFT,
                                 "sends CREATE message which creates an object on FileTrack; <file name>" ],
             'createnc':       [ self.sendCreateOnNC,
                                 "sends CREATE message which creates an object on NetCache; <file name>" ],
             'upload':         [ self.upload,
                                 "sends WRITE message; <object locator> <file name>" ],
             'delete':         [ self.delete,
                                 "sends DELETE message; <object locator>" ],
             'download':       [ self.download,
                                 "sends READ message; <object locator>" ],
             'exists':         [ self.exists,
                                 "sends EXISTS message; <object locator>" ],
             'getsize':        [ self.getsize,
                                 "sends GETSIZE message; <object locator>" ],
             'relocate':       [ self.relocate,
                                 "sends RELOCATE message; <object locator> <new location flags>" ],
             'health':         [ self.health,
                                 "sends HEALTH message; no arguments" ],
             'reconfigure':    [ self.reconfigure,
                                 "sends RECONFIGURE message; no arguments" ],
             'ackalert':       [ self.ackalert,
                                 "sends ACKALERT message; <alert name> <user>" ],
             'getmetadatainfo':[ self.getMetadataInfo,
                                 "sends GETMETADATAINFO message; no arguments" ],
             'junk':           [ self.sendJunk,
                                 "sends an unterminated malformed message" ],
             'setexp':         [ self.sendSetexp,
                                 "sends SETEXPTIME message; <object locator> <ttl in secs or infinity>" ],
           }

        self.__commandSN = 0

        # Create the connection
        socket.socket.write = socket.socket.send
        socket.socket.flush = lambda ignore: ignore
        socket.socket.readline = socket.socket.recv

        self.__sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
        self.__sock.connect( ( host_, port_ ) )

        self.__nst = json_over_uttp.MessageExchange( self.__sock, self.__sock )

    def clientMain( self ):
        " The driver "

        # Main loop
        userInput = ""
        while True:
            userInput = raw_input( "command >> " )
            userInput = userInput.strip()
            if userInput == "":
                self.checkIncomingMessage()
                continue

            # Split the input
            try:
                arguments = self.splitArguments( userInput )
            except Exception, exc:
                print "Error splitting command into parts: " + str( exc )
                continue

            # There is at least one argument which is a command name
            if arguments[ 0 ].lower() in [ 'exit', 'quit', 'q' ]:
                break

            # Find the command in the map
            processor = self.pickProcessor( arguments[ 0 ] )
            if processor is None:
                continue

            # Call the command
            processor( arguments[ 1 : ] )

        return 0


    def socketHasData( self ):
        " Checks if the socket has some data in it "
        try:
            data = self.__sock.recv( 8, socket.MSG_PEEK | socket.MSG_DONTWAIT )
            if len( data ) == 0:
                return False
        except socket.error, e:
            if e.args[0] == errno.EAGAIN:
                return False
            if e.args[0] == errno.ECONNRESET:
                print "Server socket has been closed"
                sys.exit( errno.ECONNRESET )
            raise
        return True

    def checkIncomingMessage( self ):
        " Checks if there is something in the socket and if so reads it "
        if self.socketHasData():
            response = self.__nst.receive()
            self.printMessage( "Message from server", response )
        return


    def pickProcessor( self, command ):
        " Picks the command from the map "
        if command in self.__commandMap:
            return self.__commandMap[ command.lower() ][ 0 ]

        # Try to find candidates
        candidates = self.getCandidates( command )
        if len( candidates ) == 0:
            print "The command '" + command + "' is not supported. " + \
                  "Type'help' to get the list of supported commands"
            return None

        if len( candidates ) == 1:
            return candidates[ 0 ][ 1 ]

        # ambiguity
        names = candidates[ 0 ][ 0 ]
        for cand in candidates[  1 : ]:
            names += ", " + cand[ 0 ]
        print "The command '" + command + \
              "' is ambiguous. Candidates are: " + names
        return None

    def getCandidates( self, cmd ):
        " provides a list of candidates "
        candidates = []
        for key in self.__commandMap.keys():
            if key.startswith( cmd.lower() ):
                candidates.append( [ key, self.__commandMap[ key ][ 0 ] ] )
        return candidates

    @staticmethod
    def splitArguments( cmdLine ):
        " Splits the given line into the parts "

        result = []

        cmdLine = cmdLine.strip()
        expectQuote = False
        expectDblQuote = False
        lastIndex = len( cmdLine ) - 1
        argument = ""
        index = 0
        while index <= lastIndex:
            if expectQuote:
                if cmdLine[ index ] == "'":
                    if cmdLine[ index - 1 ] != '\\':
                        if argument != "":
                            result.append( argument )
                            argument = ""
                        expectQuote = False
                    else:
                        argument = argument[ : -1 ] + "'"
                else:
                    argument += cmdLine[ index ]
                index += 1
                continue
            if expectDblQuote:
                if cmdLine[ index ] == '"':
                    if cmdLine[ index - 1 ] != '\\':
                        if argument != "":
                            result.append( argument )
                            argument = ""
                        expectDblQuote = False
                    else:
                        argument = argument[ : -1 ] + '"'
                else:
                    argument += cmdLine[ index ]
                index += 1
                continue
            # Not in a string literal
            if cmdLine[ index ] == "'":
                if index == 0 or cmdLine[ index - 1 ] != '\\':
                    expectQuote = True
                    if argument != "":
                        result.append( argument )
                        argument = ""
                else:
                    argument = argument[ : -1 ] + "'"
                index += 1
                continue
            if cmdLine[ index ] == '"':
                if index == 0 or cmdLine[ index - 1 ] != '\\':
                    expectDblQuote = True
                    if argument != "":
                        result.append( argument )
                        argument = ""
                else:
                    argument = argument[ : -1 ] + '"'
                index += 1
                continue
            if cmdLine[ index ] in [ ' ', '\t' ]:
                if argument != "":
                    result.append( argument )
                    argument = ""
                index += 1
                continue
            argument += cmdLine[ index ]
            index += 1

        if argument != "":
            result.append( argument )

        if expectQuote or expectDblQuote:
            raise Exception( "No closing quotation" )
        return result

    def printCommandList( self, arguments ):
        " Prints the available commands "
        print "Available commands:"
        for key in self.__commandMap.keys():
            print key
        return

    def printHelp( self, arguments ):
        " Provides a list of commands or a help message for a certain command "
        if len( arguments ) == 0:
            self.printCommandList( [] )
            return
        if len( arguments ) > 1:
            print "Too many arguments. 0 or 1 (command name) arguments are accepted"
            return

        command  = arguments[ 0 ]
        candidates = self.getCandidates( command )
        if len( candidates ) == 0:
            print "Cannot find any commands like '" + command + "'"
            self.printCommandList( [] )
            return

        for candidate in candidates:
            key = candidate[ 0 ]
            print key + ": " + self.__commandMap[ key ][ 1 ]
        return

    def sendHello( self, arguments ):
        " Sends the HELLO message "
        if len( arguments ) > 3:
            print "The 'hello' command accepts 0 or 1 argument (client name) " \
                  "or 2 arguments (+ service name) " \
                  "or 3 arguments (+ metadata option)"
            return

        client = "nstconsole - debugging NetStorage console"
        message = { 'Type':         'HELLO',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'Application':  'test/nstconsole.py',
                    'Ticket':       'No ticket at all' }

        if len( arguments ) == 1:
            client = arguments[ 0 ]
        if len( arguments ) == 2:
            client = arguments[ 0 ]
            service = arguments[ 1 ]
            message[ "Service" ] = service
        if len( arguments ) == 3:
            client = arguments[ 0 ]
            service = arguments[ 1 ]
            metadataOption = arguments[ 2 ]
            message[ "Service" ] = service
            message[ "Metadata" ] = metadataOption
        message[ "Client" ] = client

        self.exchange( message )
        return

    def sendJunk( self, arguments ):
        " Sends an unterminated message "

        self.__sock.send('4JUNK[\n')
        message = { 'Type':         'HELLO',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'Application':  'test/nstconsole.py',
                    'Ticket':       'No ticket at all' }
        self.exchange( message )
        return

    def sendBye( self, arguments ):
        " Sends the BYE message "
        if len( arguments ) != 0:
            print "The 'bye' command does not accept any arguments"
            return

        message = { 'Type':         'BYE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendInfo( self, arguments ):
        " Sends INFO request "
        if len( arguments ) != 0:
            print "The 'info' command does not accept any arguments"
            return

        message = { 'Type':         'INFO',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendConfiguration( self, arguments ):
        " Sends CONFIGURATION request "
        if len( arguments ) != 0:
            print "The 'configuration' command does not accept any arguments"
            return

        message = { 'Type':         'CONFIGURATION',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendShutdown( self, arguments ):
        " Sends SHUTDOWN request "
        if len( arguments ) > 1:
            print "The 'shutdown' commands takes 0 " \
                  "(default: soft) or 1 argument "
            return

        mode = "soft"
        if len( arguments ) == 1:
            mode = arguments[ 0 ].lower()
            if mode != "soft" and mode != "hard":
                print "The allowed values of the argument are 'soft' and 'hard'"
                return

        message = { 'Type':         'SHUTDOWN',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'Mode':         mode }
        self.exchange( message )
        return

    def sendGetClientsInfo( self, arguments ):
        " Sends GETCLIENTSINFO request "
        if len( arguments ) != 0:
            print "The 'getclientsinfo' command does not accept any arguments"
            return

        message = { 'Type':         'GETCLIENTSINFO',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendNoType( self, arguments ):
        " Sends a malformed request without a type "
        message = { 'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return

    def sendNoDictionary( self, arguments ):
        " Sends a malformed non-dictionary request "
        message = [ 'blah', 'blah', 'blah' ]
        self.printMessage( "Message to server", message )
        response = self.__nst.exchange( message )
        self.printMessage( "Message from server", response )
        return

    def __create( self, arguments, storage ):
        " Sends the create command "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        fileName = arguments[ 0 ]

        if not os.path.exists( fileName ):
            print "File '" + fileName + "' is not found"
            return

        try:
            f = open( fileName, "r" )
            content = f.read()
        except Exception, ex:
            print "File operation error: " + str( ex )
            return

        message = { 'Type':         'CREATE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }

        if storage == 'FT':
            storageFlags = { "Persistent": True, "Movable": True }
        else:
            storageFlags = { "Fast": True, "Movable": True }

        message["StorageFlags"] = storageFlags

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed, no data will be transferred"
            return

        uttp_writer = self.__nst.get_uttp_writer()
        data = uttp_writer.send_chunk(content)
        if data:
            self.__sock.send(data)
        data = uttp_writer.send_control_symbol('\n')
        if data:
            self.__sock.send(data)
        data = uttp_writer.flush_buf()
        if data:
            self.__sock.send(data)

        response = self.receive()
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed, the blob has not been written [completely]"
        return

    def sendCreateOnFT( self, arguments ):
        " Create an object using the FileTrack backend. "
        self.__create( arguments, 'FT' )

    def sendCreateOnNC( self, arguments ):
        " Create an object using the NetCache backend. "
        self.__create( arguments, 'NC' )

    def upload( self, arguments ):
        " Uploads a file "
        if len( arguments ) < 2:
            print "Two arguments required: locator and file"
            return

        locator = arguments[ 0 ]
        fileName = arguments[ 1 ]

        if not os.path.exists( fileName ):
            print "File '" + fileName + "' is not found"
            return

        try:
            f = open( fileName, "r" )
            content = f.read()
        except Exception, ex:
            print "File operation error: " + str( ex )
            return

        message = { 'Type':         'WRITE',
                    'ObjectLoc':    locator,
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed, no data will be transferred"
            return

        uttp_writer = self.__nst.get_uttp_writer()
        data = uttp_writer.send_chunk(content)
        if data:
            self.__sock.send(data)
        data = uttp_writer.send_control_symbol('\n')
        if data:
            self.__sock.send(data)
        data = uttp_writer.flush_buf()
        if data:
            self.__sock.send(data)

        response = self.receive()
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed, the blob has not been written [completely]"
        return

    def delete( self, arguments ):
        " Deletes the given object "

        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        objectLoc = arguments[ 0 ]

        message = { 'Type':         'DELETE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def download( self, arguments ):
        " Reads the given object "

        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        objectLoc = arguments[ 0 ]

        message = { 'Type':         'READ',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
            return

        uttp_reader = self.__nst.get_uttp_reader()

        while True:
            buf = self.__sock.recv( 1024 * 1024 )
            uttp_reader.set_new_buf(buf)
            while True:
                event = uttp_reader.next_event()
                if event == uttp.Reader.END_OF_BUFFER:
                    break

                if event == uttp.Reader.CHUNK_PART or \
                        event == uttp.Reader.CHUNK:
                    print uttp_reader.get_chunk(),
                elif event == uttp.Reader.CONTROL_SYMBOL:
                    if uttp_reader.get_control_symbol() != '\n':
                        raise Exception( "Invalid data stream terminator" )
                    print '<<EOF'
                    response = self.receive()
                    if "Status" not in response or response[ "Status" ] != "OK":
                        print "Command failed"
                    return
                else:
                    raise Exception( "Unexpected UTTP packet type" )

    def exists( self, arguments ):
        " Tells if an object exists "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        objectLoc = arguments[ 0 ]
        message = { 'Type':         'EXISTS',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def getsize( self, arguments ):
        " Tells if an object exists "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        objectLoc = arguments[ 0 ]
        message = { 'Type':         'GETSIZE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendGetObjectInfo( self, arguments ):
        " Sends the getobjectinfo request "
        if len( arguments ) != 1:
            print "Exactly one argument is required "
            return

        objectLoc = arguments[ 0 ]
        message = { 'Type':         'GETOBJECTINFO',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendGetAttrList( self, arguments ):
        " Sends GETATTRLIST message "
        if len( arguments ) != 1:
            print "Exactly 1 argument is required: locator"
            return

        objectLoc = arguments[ 0 ]

        message = { 'Type':         'GETATTRLIST',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendGetClientObjects( self, arguments ):
        " Sends GETCLIENTOBJECTS message "
        if len( arguments ) < 1:
            print "At least client name must be provided"
            return

        clientName = arguments[ 0 ]
        message = { 'Type':         'GETCLIENTOBJECTS',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ClientName':   clientName }
        if len( arguments ) > 1:
            message[ 'Limit' ] = int( arguments[ 1 ] )

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendGetAttr( self, arguments ):
        " Sends GETATTR message "
        if len( arguments ) != 2:
            print "Exactly 2 arguments are required: locator and attribute name "
            return

        objectLoc = arguments[ 0 ]
        attrName = arguments[ 1 ]

        message = { 'Type':         'GETATTR',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc,
                    'AttrName':     attrName }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendDelAttr( self, arguments ):
        " Sends DELATTR message "
        if len( arguments ) != 2:
            print "Exactly 2 arguments are required: locator and attribute name "
            return

        objectLoc = arguments[ 0 ]
        attrName = arguments[ 1 ]

        message = { 'Type':         'DELATTR',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc,
                    'AttrName':     attrName }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return


    def relocate( self, arguments ):
        " Sends RELOCATE message "
        if len( arguments ) < 2:
            print "At least two arguments are required "
            return

        srcLoc = arguments[ 0 ]
        message = { 'Type':         'RELOCATE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    srcLoc,
                    'NewLocation':  json.loads( ' '.join( arguments[ 1 : ] ) ) }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return


    def health( self, arguments ):
        " Sends HEALTH message "
        if len( arguments ) != 0:
            print "The 'health' command does not accept any arguments"
            return

        message = { 'Type':         'HEALTH',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return


    def getMetadataInfo( self, arguments ):
        " Sends GETMETADATAINFO message "
        if len( arguments ) != 0:
            print "The 'getmetadatainfo' command does not accept any arguments"
            return

        message = { 'Type':         'GETMETADATAINFO',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return


    def reconfigure( self, arguments ):
        " Sends RECONFIGURE message "
        if len( arguments ) != 0:
            print "The 'reconfigure' command does not accept any arguments"
            return

        message = { 'Type':         'RECONFIGURE',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP }
        self.exchange( message )
        return


    def ackalert( self, arguments ):
        " Sends ACKALERT message "
        if len( arguments ) != 2:
            print "Exactly two argument are required: alert name and the user"
            return

        alertName = arguments[ 0 ]
        userName = arguments[ 1 ]

        message = { 'Type':         'ACKALERT',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'Name':         alertName,
                    'User':         userName }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return


    def sendSetAttr( self, arguments ):
        " Sends SETATTR message "
        if len( arguments ) != 3:
            print "Exactly 3 arguments are required: locator, " \
                  "attribute name and atrribute value"
            return

        objectLoc = arguments[ 0 ]
        attrName = arguments[ 1 ]
        attrValue = arguments[ 2 ]

        message = { 'Type':         'SETATTR',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc,
                    'AttrName':     attrName,
                    'AttrValue':    attrValue }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return

    def sendSetexp( self, arguments ):
        " Sends SETEXPTIME message "
        if len( arguments ) != 2:
            print "Exactly 2 arguments are required: locator " \
                  "and ttl in seconds"
            return

        objectLoc = arguments[ 0 ]
        ttl = arguments[ 1 ]

        message = { 'Type':         'SETEXPTIME',
                    'SessionID':    SESSIONID,
                    'ncbi_phid':    NCBI_PHID,
                    'ClientIP':     hostIP,
                    'ObjectLoc':    objectLoc,
                    'TTL':          ttl }

        response = self.exchange( message )
        if "Status" not in response or response[ "Status" ] != "OK":
            print "Command failed"
        return


    def exchange( self, message ):
        " Does the basic exchange "
        self.__commandSN += 1

        message[ "SN" ] = self.__commandSN
        self.printMessage( "Message to server", message )
        response = self.__nst.exchange( message )
        self.printMessage( "Message from server", response )
        return response


    def receive( self ):
        " Receives a single server message "
        response = self.__nst.receive()
        self.printMessage( "Message from server", response )
        return response


    @staticmethod
    def printMessage( prefix, response ):
        " Prints the server response "
        print prefix + ":"
        prettyPrinter = pprint.PrettyPrinter( indent = 4 )
        prettyPrinter.pprint( response )
        return


def parserError( parser_, message ):
    " Prints the message and help on stderr "
    sys.stdout = sys.stderr
    print message
    parser_.print_help()
    return

# The script execution entry point
if __name__ == "__main__":
    try:
        parser = OptionParser(
        """
        %prog  <host>  <port>
        %prog  <host:port>
        Netstorage debug console
        """ )

        # parser.add_option( "-v", "--verbose",
        #                   action="store_true", dest="verbose", default=False,
        #                   help="be verbose (default: False)" )

        # parse the command line options
        options, args = parser.parse_args()

        if len( args ) not in [ 1, 2 ]:
            parserError( parser, "Incorrect number of arguments" )
            sys.exit( 1 )

        if len( args ) == 2:
            host = args[ 0 ]
            port = args[ 1 ]
        else:
            parts = args[0].split( ":" )
            if len( parts ) != 2 :
                parserError( parser, "Expected format host:port" )
                sys.exit( 1 )
            host = parts[ 0 ]
            port = parts[ 1 ]

        try:
            port = int( port )
        except:
            parserError( parser, "The port must be integer" )
            sys.exit( 1 )

        returnValue = NetStorageConsole(host, port).clientMain()
    except Exception, exc:
        print >> sys.stderr, str( exc )
        returnValue = 4

    sys.exit( returnValue )
