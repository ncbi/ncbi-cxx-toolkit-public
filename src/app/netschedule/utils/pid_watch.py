#!/bin/env python

import datetime, sys, os.path
from optparse import OptionParser
from time import sleep


LOG_FILE="/tmp/0/pid_watch"
VERBOSE=False



def now():
    return datetime.datetime.now().strftime( '%m/%d/%y %H:%M:%S' )


def printVerbose( msg ):
    " Prints stdout message conditionally "
    timestamp = now()
    try:
        f = open( LOG_FILE, "a" )
        f.write( timestamp + "," + msg + "\n" )
        f.flush()
        f.close()
    except:
        pass

    if VERBOSE:
        print timestamp + " " + msg

def printStderr( msg ):
    " Prints onto stderr with a prefix "
    print >> sys.stderr, now() + " pid watch script " + msg

def getSockets( pid ):
    path = "/proc/" + str( pid ) + "/fd/"
    count = 0
    for item in os.listdir( path ):
        if item in [ '.', '..' ]:
            continue
        if os.path.islink( path + item ):
            try:
                if 'socket' in os.readlink( path + item ):
                    count += 1
            except:
                pass
    return count

def getMemory( pid ):
    path = "/proc/" + str( pid ) + "/statm"
    try:
        f = open( path, "r" )
        val = f.read()
        f.close()
        return val.strip().replace( ' ', ',' )
    except:
        pass
    return ""


def main():
    " main function "

    parser = OptionParser(
    """
    %prog  <process pid>
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )

    # parse the command line options
    options, args = parser.parse_args()
    global VERBOSE
    VERBOSE = options.verbose

    if len( args ) != 1:
        printStderr( "process pid expected" )
        return 1

    pid = args[ 0 ]
    try:
        pidAsInt = int( pid )
    except:
        printStderr( "process pid must be an integer" )
        return 1

    path = "/proc/" + pid + "/fd/"

    while True:
        sockets = getSockets( pid )
        memory = getMemory( pid )
        printVerbose( ','.join( [ str( sockets ), memory ]  ) )
        sleep( 10 )

    return 0    # never happens


if __name__ == "__main__":
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        returnValue = 0
    except Exception, excpt:
        printStderr( str( excpt ) )
        returnValue = 1
    except:
        printStderr( "Generic unknown script error" )
        returnValue = 2

    sys.exit( returnValue )

