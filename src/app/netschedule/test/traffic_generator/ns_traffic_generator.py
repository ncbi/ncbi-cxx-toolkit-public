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
        threads.append( SubmitDropLoader( gridClient ) )



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



######## Loaders definitions begin

class SubmitDropLoader( threading.Thread ):
    " Submit/drop loader "

    def __init__( self, gridClient ):
        threading.Thread.__init__( self )
        self.__gridClient = gridClient
        self.__count = 0
        return

    def getName( self ):
        " Loader identification "
        return "Submit/drop"

    def getCount( self ):
        " Provides haw many loops completed "
        return self.__count

    def run( self ):
        " threaded function "
        pSize = SubmitDropSettings.packageSize
        if pSize <= 0:
            print >> sys.stderr, \
                     "Invalid SubmitDropSettings.packageSize (" + \
                     str( pSize ) + "). Must be > 0"
            return
        pause = SubmitDropSettings.pause
        if pause < 0:
            print >> sys.stderr, \
                     "Invalid SubmitDropSettings.pause (" + \
                     str( pause ) + "). Must be >= 0"
            return

        pCount = SubmitDropSettings.packagesCount
        if not ( pCount == -1 or pCount > 0 ):
            print >> sys.stderr, \
                     "Invalid SubmitDropSettings.packagesCount (" + \
                     str( pCount ) + "). Must be > 0 or -1"
            return

        # Settings are OK
        while True:
            pSize = SubmitDropSettings.packageSize
            while pSize > 0:
                jobKey = ""
                try:
                    jobKey = self.__gridClient.submitJob( queueName, "bla" )
                    try:
                        self.__gridClient.killJob( queueName, jobKey )
                    except Exception, excp:
                        print >> sys.stderr, "Submit/Drop: Cannot kill job: " + jobKey
                        print >> sys.stderr, str( excp )
                except Exception, excp:
                    print >> sys.stderr, "Submit/Drop: Cannot submit job"
                    print >> sys.stderr, str( excp )

                self.__count += 1
                pSize -= 1

            if pause > 0:
                time.sleep( pause )

            if pCount == -1:    # Infinite loop
                continue
            pCount -= 1
            if pCount <= 0:
                break
        return



######## Loaders definitions end



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
        raise
        returnValue = 1

    sys.exit( returnValue )

