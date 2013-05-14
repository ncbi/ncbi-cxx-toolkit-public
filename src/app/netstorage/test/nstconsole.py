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
from ncbi_grid_dev.ncbi import json_over_uttp
import pprint
import readline
import rlcompleter
import atexit
import os

historyPath = os.path.expanduser("~/.nstconsole")

readline.parse_and_bind('tab: complete')

def save_rlhistory():
    readline.write_history_file(historyPath)

if os.path.exists(historyPath):
    readline.read_history_file(historyPath)

atexit.register(save_rlhistory)

commandMap = None
commandSN = 0


def getCommandMap():
    " Forms the command map "
    return { 'help':           printCommandList,
             'hello':          sendHello,
             'bye':            sendBye,
             'info':           sendInfo,
             'configuration':  sendConfiguration,
             'shutdown':       sendShutdown,
             'getclientsinfo': sendGetClientsInfo,
             'getobjectinfo':  sendGetObjectInfo,
             'getattr':        sendGetAttr,
             'setattr':        sendSetAttr
           }


def clientMain():
    " The driver "

    parser = OptionParser(
    """
    %prog  <host>  <port>
    Netstorage debug console
    """ )

#    parser.add_option( "-v", "--verbose",
#                       action="store_true", dest="verbose", default=False,
#                       help="be verbose (default: False)" )

    # parse the command line options
    options, args = parser.parse_args()

    if len( args ) != 2:
        parserError( parser, "Incorrect number of arguments" )
        return 1

    host = args[ 0 ]
    port = args[ 1 ]
    try:
        port = int( port )
    except:
        parserError( parser, "The port must be integer" )
        return 1

    # Create the connection
    socket.socket.write = socket.socket.send
    socket.socket.flush = lambda ignore: ignore
    socket.socket.readline = socket.socket.recv

    sock = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
    sock.connect( ( host, port ) )

    nst = json_over_uttp.MessageExchange( sock, sock )

    # Main loop
    userInput = ""
    while True:
        userInput = raw_input( "command >> " )
        userInput = userInput.strip()
        if userInput == "":
            checkIncomingMessage( sock, nst )
            continue

        # Split the input
        try:
            arguments = splitArguments( userInput )
        except:
            print "Error splitting command into parts"
            continue

        # There is at least one argument which is a command name
        if arguments[ 0 ].lower() in [ 'exit', 'quit', 'q' ]:
            break

        # Find the command in the map
        processor = pickProcessor( arguments[ 0 ] )
        if processor is None:
            continue

        # Call the command
        processor( nst, arguments[ 1 : ] )

    return 0


def checkIncomingMessage( sock, nst ):
    " Checks if there is something in the socket and if so reads it "
    try:
        data = sock.recv( 8, socket.MSG_PEEK | socket.MSG_DONTWAIT )
        if len( data ) == 0:
            return
    except socket.error, e:
        if e.args[0] == errno.EAGAIN:
            return
        if e.args[0] == errno.ECONNRESET:
            print "Server socket has been closed"
            sys.exit( errno.ECONNRESET )
        raise

    response = nst.receive()
    printMessage( "Message from server", response )
    return


def pickProcessor( command ):
    " Picks the command from the map "
    if command in commandMap:
        return commandMap[ command.lower() ]

    # Try to find candidates
    candidates = []
    for key in commandMap.keys():
        if key.startswith( command.lower() ):
            candidates.append( [ key, commandMap[ key ] ] )

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
    print "The command '" + command + "' is ambiguous. Candidates are: " + names
    return None



def parserError( parser, message ):
    " Prints the message and help on stderr "
    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return


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


def printCommandList( nst, arguments ):
    " Prints the available commands "
    print "Available commands:"
    for key in commandMap.keys():
        print key
    return

def sendHello( nst, arguments ):
    " Sends the HELLO message "
    if len( arguments ) > 1:
        print "The 'hello' command accepts 0 or 1 argument (client name)"
        return

    client = "nstconsole - debugging NetStorage console"
    if len( arguments ) == 1:
        client = arguments[ 0 ]

    message = { 'Type':         'HELLO',
                'SessionID':    'No session',
                'ClientIP':     'localhost',
                'Client':       client,
                'Application':  'test/nstconsole.py',
                'Ticket':       'No ticket at all' }
    exchange( nst, message )
    return

def sendBye( nst, arguments ):
    " Sends the BYE message "
    if len( arguments ) != 0:
        print "The 'bye' command does not accept any arguments"
        return

    message = { 'Type':         'BYE',
                'SessionID':    'No session',
                'ClientIP':     'localhost' }
    exchange( nst, message )
    return

def sendInfo( nst, arguments ):
    " Sends INFO request "
    if len( arguments ) != 0:
        print "The 'info' command does not accept any arguments"
        return

    message = { 'Type':         'INFO',
                'SessionID':    'No session',
                'ClientIP':     'localhost' }
    exchange( nst, message )
    return

def sendConfiguration( nst, arguments ):
    " Sends CONFIGURATION request "
    if len( arguments ) != 0:
        print "The 'configuration' command does not accept any arguments"
        return

    message = { 'Type':         'CONFIGURATION',
                'SessionID':    'No session',
                'ClientIP':     'localhost' }
    exchange( nst, message )
    return

def sendShutdown( nst, arguments ):
    " Sends SHUTDOWN request "
    if len( arguments ) > 1:
        print "The 'shutdown' commands takes 0 (default: soft) or 1 argument "
        return

    mode = "soft"
    if len( arguments ) == 1:
        mode = arguments[ 0 ].lower()
        if mode != "soft" and mode != "hard":
            print "The allowed values of the argument are 'soft' and 'hard'"
            return

    message = { 'Type':         'SHUTDOWN',
                'SessionID':    'No session',
                'ClientIP':     'localhost',
                'Mode':         mode }
    exchange( nst, message )
    return

def sendGetClientsInfo( nst, arguments ):
    " Sends GETCLIENTSINFO request "
    if len( arguments ) != 0:
        print "The 'getclientsinfo' command does not accept any arguments"
        return

    message = { 'Type':         'GETCLIENTSINFO',
                'SessionID':    'No session',
                'ClientIP':     'localhost' }
    exchange( nst, message )
    return

def sendGetObjectInfo( nst, arguments ):
    print "Not implemented yet"
    return

def sendGetAttr( nst, arguments ):
    print "Not implemented yet"
    return

def sendSetAttr( nst, arguments ):
    print "Not implemented yet"
    return


def exchange( nst, message ):
    " Does the basic exchange "
    global commandSN
    commandSN += 1

    message[ "SN" ] = commandSN
    printMessage( "Message to server", message )
    response = nst.exchange( message )
    printMessage( "Message from server", response )
    return


def printMessage( prefix, response ):
    " Prints the server response "
    print prefix + ":"
    pp = pprint.PrettyPrinter( indent = 4 )
    pp.pprint( response )
    return


# Initialize the command map
commandMap = getCommandMap()

# The script execution entry point
if __name__ == "__main__":
    try:
        returnValue = clientMain()
    except Exception, e:
        print >> sys.stderr, str( e )
        returnValue = 4

    sys.exit( returnValue )
