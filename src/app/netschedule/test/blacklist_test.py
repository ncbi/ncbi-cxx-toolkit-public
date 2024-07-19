#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Tests SUBMIT +
- GET2 + RETURN2 from one thread
- GET2 from another thread
"""

import re, socket, sys
from optparse import OptionParser

# Works for python 2.5. Python 2.7 has it in urlparse module
from urllib.parse import parse_qs


VERBOSE = False
COMMUNICATION_TIMEOUT = 25


def printStderr(msg):
    """Prints onto stderr with a prefix"""
    print("NetSchedule commands script. " + msg, file=sys.stderr)

def printVerbose(msg):
    """Prints stdout message conditionally"""
    if VERBOSE:
        print(msg)

class UnexpectedNSResponse(Exception):
    """NetSchedule produces trash"""
    def __init__(self, message):
        Exception.__init__(self, message)

class NSShuttingDown(Exception):
    """NetSchedule is shutting down"""
    def __init__(self, message):
        Exception.__init__(self, message)

class NSAccessDenied(Exception):
    """No permissions for commands"""
    def __init__(self, message):
        Exception.__init__(self, message)

class NSError(Exception):
    """NetSchedule answer has ERR:..."""
    def __init__(self, message):
        Exception.__init__(self, message)


class NSDirectConnect:
    """Serves the direct connection to the server"""
    def __init__(self, connPoint, queueName):
        parts = connPoint.split(":")
        self.__host = parts[0]
        if self.__host == "":
            self.__host = "localhost"
        self.__port = int(parts[1])
        self.__queue = queueName
        self.__sock = None
        self.__readBuf = ""
        self.__replyDelimiter = re.compile(r'\r\n|\n|\r|\0')

    def getQueueName(self):
        return self.__queue

    def connect(self, timeout):
        """Connects to the server"""
        self.__sock = socket.socket()
        self.__sock.settimeout(timeout)
        self.__sock.connect( (self.__host, self.__port) )

    def disconnect(self):
        """Close direct connection if it was opened"""
        if self.__sock is not None:
            self.__sock.close()
        self.__sock = None

    def login(self, session=None):
        """Performs a direct login to NS"""
        if session is None:
            session = "default_session"
        self.__sock.send(b"netschedule_admin client_node=commands_check " +
                         b"client_session=" + session.encode() + b"\n" +
                         self.__queue.encode() + b"\n")

    def send(self, cmd):
        self.__sock.sendall(cmd.encode() + b"\n")

    def receive(self, multiline=False):
        if multiline:
            return self.readMultiLineReply()
        return self.readSingleLineReply()

    def execute(self, cmd, multiline=False):
        """Sends the given command to NS"""
        self.send(cmd)
        return self.receive(multiline)

    def readSingleLineReply(self, multilinePart=False):
        """Reads a single line reply"""
        while True:
            parts = self.__replyDelimiter.split(self.__readBuf, 1)
            if len(parts) > 1:
                reply = parts[0]
                self.__readBuf = parts[1]
                break

            buf = self.__sock.recv(8192)
            if not buf:
                if self.__readBuf:
                    return self.__readBuf.strip()
                raise UnexpectedNSResponse("Unexpected NS response: None")
            self.__readBuf += buf.decode('latin1')

        if reply.startswith("ERR:"):
            if "shuttingdown" in reply.lower():
                raise NSShuttingDown("Server is shutting down")
            elif "access denied" in reply.lower():
                raise NSAccessDenied( reply.split( ':', 1 )[ 1 ].strip() )
            raise NSError( reply.split( ':', 1 )[ 1 ].strip() )

        if reply.startswith("OK:"):
            return reply.split( ':', 1 )[ 1 ].strip()

        if multilinePart:
            # In a multiline case it could be anything, e.g. GETCONF
            return reply

        raise UnexpectedNSResponse("Unexpected NS response: " + reply.strip())

    def readMultiLineReply(self):
        " Reads a multi line reply "
        lines = []
        oneLine = self.readSingleLineReply( True )
        while oneLine != "END":
            if oneLine:
                lines.append( oneLine )
            oneLine = self.readSingleLineReply( True )
        return lines



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

    if len(args) < 2:
        printStderr("Incorrect number of arguments")
        return 4

    connectionPoint = args[0]
    if len(connectionPoint.split(":")) != 2:
        printStderr("invalid connection point format. Expected host:port")
        return 4

    queueName = args[1]

    printVerbose("Connection point: " + connectionPoint + "\n"
                 "Queue: " + queueName)


    nsConnect = NSDirectConnect(connectionPoint, queueName)
    nsConnect.connect( COMMUNICATION_TIMEOUT )
    nsConnect.login()

    submOutput = nsConnect.execute("SUBMIT input=testJob", False)
    printVerbose("SUBMIT output: " + submOutput)


    nsConnect_1 = NSDirectConnect(connectionPoint, queueName)
    nsConnect_1.connect( COMMUNICATION_TIMEOUT )
    nsConnect_1.login()

    nsConnect_2 = NSDirectConnect(connectionPoint, queueName)
    nsConnect_2.connect( COMMUNICATION_TIMEOUT )
    nsConnect_2.login()

    getOutput_1 = nsConnect_1.execute("GET2 wnode_aff=0 any_aff=1 exclusive_new_aff=0", False)
    values = parse_qs(getOutput_1 , True, True)
    jobKey = values['job_key'][0]
    authToken = values['auth_token'][0]
    printVerbose("First GET2. Job: " + jobKey + " Auth token: " + authToken)

    nsConnect_1.send("RETURN2 job_key=" + jobKey + " auth_token=" + authToken)
    nsConnect_2.send("GET2 wnode_aff=0 any_aff=1 exclusive_new_aff=0")

    retOutput = nsConnect_1.receive(False)
    getOutput_2 = nsConnect_2.receive(False)

    printVerbose("RETURN2 output: " + retOutput)
    printVerbose("Second GET2 output: " + getOutput_2)

    count = 0
    while count < -1 and len(getOutput_2) < 10:
        nsConnect_2.send("GET2 wnode_aff=0 any_aff=1 exclusive_new_aff=0")
        getOutput_2 = nsConnect_2.receive(False)
        count += 1

    if len(getOutput_2) > 10:
        printVerbose("Got the job on attempt " + str(count) + ". Output: " + getOutput_2)
    else:
        printVerbose("No job")

    return 0



# The script execution entry point
if __name__ == "__main__":
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        printStderr( "Ctrl + C received" )
        returnValue = 1
    except Exception as excpt:
        printStderr( str( excpt ) )
        returnValue = 2
    except:
        printStderr( "Generic unknown script error" )
        returnValue = 3

    sys.exit( returnValue )
