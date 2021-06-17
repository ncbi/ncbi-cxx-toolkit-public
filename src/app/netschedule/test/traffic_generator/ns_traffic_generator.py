#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
NetScheduler traffic generator
"""


import sys, threading, time, datetime
from optparse import OptionParser
from libgridclient import GridClient
from ns_traffic_settings import IndividualLoaders, SubmitDropSettings
from submitdroploader import SubmitDropLoader
from batchsubmitdroploader import BatchSubmitDropLoader
from singlefullok import SingleFullOKLoopLoader


queueName = 'TEST'


def main():
    " main function for the netschedule traffic loader "

    global queueName

    parser = OptionParser(
    """
    %prog  <host>  <port>
    NetScheduler traffic generator, see ns_traffic_settings.py to configure
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "-q", "--queue",
                       dest="qname", default=queueName,
                       help="NS queue name" )


    # parse the command line options
    options, args = parser.parse_args()
    verbose = options.verbose
    queueName = options.qname

    # Check the number of arguments
    if len( args ) != 2:
        return parserError( parser, "Incorrect number of arguments" )
    host = args[ 0 ]
    port = int( args[ 1 ] )
    if port <= 0 or port > 65535:
        raise Exception( "Incorrect port number" )

    if verbose:
        print "Using NetSchedule port: " + str( port )
        print "Using NetSchedule host: " + host

    # This instance will be shared between threads
    gridClient = GridClient( host, port, verbose )

    # Create threads and run them
    threads = []

    if IndividualLoaders.SubmitDropLoader:
        threads.append( SubmitDropLoader( gridClient, queueName ) )
    if IndividualLoaders.BatchSubmitDropLoader:
        threads.append( BatchSubmitDropLoader( gridClient, queueName ) )
    if IndividualLoaders.SingleFullOKLoopLoader:
        threads.append( SingleFullOKLoopLoader( gridClient, queueName ) )


    if len( threads ) == 0:
        print "No loaders enabled. Exiting."
        return 0

    # Run all the enabled loaders and their watcher
    monitor = ExecutionMonitor( threads )
    monitor.start()
    for thread in threads:
        thread.start()

    # Wait till all the loaders finished
    for thread in threads:
        thread.join()

    monitor.stopRequest()
    monitor.join()
    return 0


class ExecutionMonitor( threading.Thread ):
    " prints statistics every 5 seconds "
    def __init__( self, threads ):
        threading.Thread.__init__( self )
        self.__threads = threads
        self.__stopRequest = False
        return

    def stopRequest( self ):
        " request the thread to stop "
        self.__stopRequest = True
        return

    def run( self ):
        " threaded function "
        while self.__stopRequest == False:
            self.printOnce()
            time.sleep( 10 )
        self.printOnce()
        return

    def printOnce( self ):
        " prints statistics once "
        now = datetime.datetime.now()
        output = "Timestamp: " + now.isoformat() + "\n"
        for thread in self.__threads:
            output += thread.getName() + " loader: " + \
                      str( thread.getCount() ) + "\n"
        print output
        return



def parserError( parser, message ):
    " Prints the message and help on stderr "

    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return 1


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

