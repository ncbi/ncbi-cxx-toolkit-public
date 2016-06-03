#!/opt/python-all/bin/python2 -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server loader test script
"""

import os, os.path, sys, time
import logging
from subprocess import Popen, PIPE
from optparse import OptionParser


def parserError( parser, message ):
    " Prints the message and help on stderr "

    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return 1



def main():
    " main function for the netschedule multi test "

    setupLogging()

    parser = OptionParser(
    """
    %prog <# of queues> <# of loaders per queue>
    Note #1: netschedule server will be running on the same host
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "--path-netschedule", dest="pathNetschedule",
                       default="",
                       help="Path to directory where netschedule " \
                            "daemon binary is" )
    parser.add_option( "--path-loader", dest="pathLoader",
                       default="",
                       help="Path to the directory where " \
                            "ns_loader binary is" )
    parser.add_option( "--db-path", dest="pathDB",
                       default="",
                       help="Path to the directory where netschedule must " \
                            "store its DB. (Default: data.<port>)" )
    parser.add_option( "--jobs", dest="jobs",
                       default=10000,
                       help="Number of jobs for each instance of " \
                            "ns_loader. (Default: 10000)" )
    parser.add_option( "--port", dest="port",
                       default=9109,
                       help="Netschedule daemon port to be used for tests. " \
                            "Default: 9109" )

    # parse the command line options
    options, args = parser.parse_args()

    if len( args ) != 2:
        return parserError( parser, "Two arguments are expected" )

    numberOfQueues = int( args[ 0 ] )
    if numberOfQueues <= 0:
        raise Exception( "Invalid number of queues" )
    numberOfLoaders = int( args[ 1 ] )
    if numberOfLoaders <= 0:
        raise Exception( "Invalid number of ns_loader" )
    jobs = int( options.jobs )
    if jobs <= 0:
        raise Exception( "Invalid number of jobs" )
    port = int( options.port )
    if port < 1024 or port > 65535:
        raise Exception( "Invalid port number " + str( port ) )

    baseDir = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + os.path.sep

    pathNetschedule = options.pathNetschedule
    if pathNetschedule == "":
        pathNetschedule = baseDir
    if not pathNetschedule.endswith( os.path.sep ):
        pathNetschedule += os.path.sep

    pathLoader = options.pathLoader
    if pathLoader == "":
        pathLoader = baseDir
    if not pathLoader.endswith( os.path.sep ):
        pathLoader += os.path.sep

    pathDB = options.pathDB
    if pathDB == "":
        pathDB = baseDir + "data." + str( port )
    else:
        if not os.path.abspath( pathDB ):
            pathDB = baseDir + pathDB

    if options.verbose:
        print "Parameters to use:"
        print "Number of queues: " + str( numberOfQueues )
        print "Number of loaders per queue: " + str( numberOfLoaders )
        print "Number of jobs per loader: " + str( jobs )
        print "Port: " + str( port )
        print "Dir where NS is: " + pathNetschedule
        print "Dir where loader is: " + pathLoader
        print "Dir where NS stores DB: " + pathDB

    checkPrerequisites( baseDir, pathNetschedule, pathLoader, port )
    generateNSConfig( baseDir, numberOfQueues, port, pathDB )

    # It's time to run all the components
    if options.verbose:
        print "Launching netschedule..."

    nsCmdLine = [ pathNetschedule + "netscheduled",
                  "-conffile", baseDir + "netscheduled.ini",
                  "-logfile", baseDir + "netscheduled.log" ]
    nsProc = Popen( nsCmdLine, shell = True, stdout = PIPE, stderr = PIPE )

    time.sleep( 10 )
    nsProc.wait()

    crashProcs = []
    for index in xrange( numberOfQueues ):
        qName = "CRASH" + str( index )
        for crash_index in xrange( numberOfLoaders ):
            cmdLine = pathLoader + "ns_loader " + \
                      " -service localhost:" + str( port ) + \
                      " -queue " + qName + \
                      " -jobs " + str( jobs )
            if options.verbose:
                print "Launching ns_loader #" + \
                      str( crash_index ) + " for queue " + qName
                print cmdLine
            DEVNULL = open( '/dev/null', 'w' )
            crashProcs.append( Popen( cmdLine, shell = True,
                                      stdout = DEVNULL, stderr = PIPE ) )

    if options.verbose:
        print "Waiting for loader finish"
    for proc in crashProcs:
        proc.wait()
        err = proc.stderr.read()
        if err != "":
            print "ns_loader stderr: " + err
        if options.verbose:
            print "ns_loader finished"

    if options.verbose:
        print "Please do not forget to stop netschedule before running again"

    return 0



def generateNSConfig( baseDir, numberOfQueues, port, pathDB ):
    " Generates the actual config and saves it as netscheduled.ini "

    content = open( baseDir + "netscheduled.ini.template" ).read()
    content = content.replace( "$PORT", str( port ) )
    content = content.replace( "$DBPATH", pathDB )

    for index in xrange( numberOfQueues ):
        content += "\n" \
                   "[queue_CRASH" + str( index ) + "]\n" \
                   "failed_retries=3\n" \
                   "timeout=300\n" \
                   "blacklist_time=600\n" \
                   "notif_timeout=1.0\n" \
                   "run_timeout=600\n" \
                   "run_timeout_precision=30\n" \
                   "delete_done=false\n" \
                   "max_input_size=1M\n" \
                   "max_output_size=1M\n" \
                   "scramble_job_keys=true\n"

    f = open( baseDir + "netscheduled.ini", "w" )
    f.write( content )
    f.close()
    return



def setupLogging():
    " Sets up the logging "

    fName = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
            os.path.sep + "crash_test.log"

    logging.basicConfig( level = logging.DEBUG,
                         format = "%(levelname) -10s %(asctime)s %(message)s",
                         filename = fName)
    return


def checkPrerequisites( baseDir, pathNetschedule, pathLoader, port ):
    " Checks that all the required files are in place "

    fname = baseDir + "netscheduled.ini.template"
    if not os.path.exists( fname ):
        raise Exception( "Cannot find configuration template file. "
                         "Expected here: " + fname )

    fname = pathNetschedule + "netscheduled"
    if not os.path.exists( fname ):
        raise Exception( "Cannot find netschedule binary. "
                         "Expected here: " + fname )

    fname = pathLoader + "ns_loader"
    if not os.path.exists( fname ):
        raise Exception( "Cannot find ns loader test binary. "
                         "Expected here: " + fname )

    # Check that NS is not running
    retCode = os.system( "echo 'blah' | netcat localhost " + str( port ) )
    if retCode == 0:
        raise Exception( "NS is still running on port " + str( port ) +
                         ". Please shut it down before starting tests." )
    return



# The script execution entry point
if __name__ == "__main__":
    try:
        returnValue = main()
    except KeyboardInterrupt:
        # Ctrl+C
        print >> sys.stderr, "Ctrl + C received"
        logging.error( "Tests have been interrupted (Ctrl+C)" )
        returnValue = 2

    except Exception, excpt:
        print >> sys.stderr, str( excpt )
        logging.error( str( excpt ) )
        returnValue = 1

    sys.exit( returnValue )

