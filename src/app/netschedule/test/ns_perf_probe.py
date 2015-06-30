#!/opt/python-2.7/bin/python -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server throughput sampling script
"""

import sys
from optparse import OptionParser
from time import sleep
from ncbi_grid_1_1.ncbi.grid import ns
from datetime import datetime


def parserError( parser_, message ):
    " Prints the message and help on stderr "
    sys.stdout = sys.stderr
    print message
    parser_.print_help()
    return


def accumulateCounters( counters, values ):
    " Values came as a list of strings like 'Name: xxx' "
    for value in values:
        name, val = value.split( ':', 1 )
        val = int( val )

        if name in counters:
            counters[ name ] += val
        else:
            counters[ name ] = val
    return

def formatTimestamp( ts ):
    " As MS Excel likes it "
    return ts.strftime( "%m/%d/%Y %H:%M:%S" )

def main():
    " main function for the netschedule multi test "

    parser = OptionParser(
    """
    %prog <host>  <port>
    %prog  <host:port>
    Collects the number submitted jobs and the up time periodically and saves into a CSV file
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "--interval", dest="interval",
                       default=60,
                       help="Interval between probes, seconds. (Default: 60)" )

    # parse the command line options
    options, args = parser.parse_args()
    if len( args ) not in [ 1, 2 ]:
        parserError( parser, "Incorrect number of arguments" )
        return 3

    if len( args ) == 2:
        host = args[ 0 ]
        port = args[ 1 ]
    else:
        parts = args[0].split( ":" )
        if len( parts ) != 2 :
            parserError( parser, "Expected format host:port" )
            return 3
        host = parts[ 0 ]
        port = parts[ 1 ]

    try:
        port = int( port )
    except:
        parserError( parser, "The port must be integer" )
        return 3

    interval = options.interval
    try:
        interval = int( interval )
    except:
        parserError( parser, "The interval must be an integer > 0" )
        return 3
    if interval <= 0:
        parserError( parser, "The interval must be an integer > 0" )
        return 3

    # Get the server start time once
    server = ns.NetScheduleServer( host + ":" + str( port ),
            client_name = "performance_probe" )
    server.allow_xsite_connections()
    currentDate = datetime.now()
    serverStatus = server.get_server_status()
    if 'Started' not in serverStatus:
        raise Exception( "Cannot get the server start timestamp" )
    startDate = datetime.strptime( serverStatus[ 'Started' ],
                                   "%m/%d/%Y %H:%M:%S" )

    lastTotalSubmits = None
    while True:
        # Open a connection
        server = ns.NetScheduleServer( host + ":" + str( port ),
                client_name = "performance_probe" )
        server.allow_xsite_connections()

        # Interrogate
        currentDate = datetime.now()
        serverStatus = server.get_server_status()

        counters = {}
        for key in serverStatus:
            if key.startswith( "queue " ):
                accumulateCounters( counters, serverStatus[ key ] )
        if 'submits' not in counters:
            raise Exception( "Cannot get the total server submits" )
        totalSubmits = counters[ 'submits' ]

        # Print results
        runtime = currentDate - startDate
        runtime = str( runtime.total_seconds() ).split( '.' )[ 0 ]

        if lastTotalSubmits is None:
            print "Timestamp,seconds since server started,total submits"
        print formatTimestamp( currentDate ) + "," + runtime + "," + str( totalSubmits )

        if totalSubmits == lastTotalSubmits:
            break
        lastTotalSubmits = totalSubmits

        # Sleep
        sleep( interval )

    return 0



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
        returnValue = 1

    sys.exit( returnValue )
