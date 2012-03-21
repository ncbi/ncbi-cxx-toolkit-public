#!/opt/python-2.5/bin/python -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server crash test script
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
    %prog <# of queues> <# of crash_tests per queue>
    Note #1: netschedule server will be running on the same host
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "--path-netschedule", dest="pathNetschedule",
                       default="",
                       help="Path to directory where netschedule " \
                            "daemon binary is" )
    parser.add_option( "--path-crash-test", dest="pathCrashTest",
                       default="",
                       help="Path to the directory where " \
                            "test_netschedule_crash binary is" )
    parser.add_option( "--db-path", dest="pathDB",
                       default="",
                       help="Path to the directory where netschedule must " \
                            "store its DB. (Default: data.<port>)" )
    parser.add_option( "--jobs", dest="jobs",
                       default=10000,
                       help="Number of jobs for each instance of " \
                            "test_netschedule_crash. (Default: 10000)" )
    parser.add_option( "--port", dest="port",
                       default=9109,
                       help="Netschedule daemon port to be used for tests. " \
                            "Default: 9109" )
    parser.add_option( "--affs", dest="affs",
                       default=500,
                       help="Number of affinities for each instance of " \
                            "test_netschedule_crash. (Default: 500)" )
    parser.add_option( "--groups", dest="groups",
                       default=500,
                       help="Number of groups for each instance of " \
                            "test_netschedule_crash. (Default: 500)" )
    parser.add_option( "--clients", dest="clients",
                       default=0,
                       help="Number of clients for each instance of " \
                            "test_netschedule_crash. (Default: 0)" )

    # parse the command line options
    options, args = parser.parse_args()

    if len( args ) != 2:
        return parserError( parser, "Two arguments are expected" )

    numberOfQueues = int( args[ 0 ] )
    if numberOfQueues <= 0:
        raise Exception( "Invalid number of queues" )
    numberOfCrashTests = int( args[ 1 ] )
    if numberOfCrashTests <= 0:
        raise Exception( "Invalid number of test_netschedule_crash" )
    jobs = int( options.jobs )
    if jobs <= 0:
        raise Exception( "Invalid number of jobs" )
    port = int( options.port )
    if port < 1024 or port > 65535:
        raise Exception( "Invalid port number " + str( port ) )
    affs = int( options.affs )
    if affs < 0:
        raise Exception( "Invalid number of affinities" )
    groups = int( options.groups )
    if groups < 0:
        raise Exception( "Invalid number of groups" )
    clients = int( options.clients )
    if clients < 0:
        raise Exception( "Invalid number of clients" )

    baseDir = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + os.path.sep

    pathNetschedule = options.pathNetschedule
    if pathNetschedule == "":
        pathNetschedule = baseDir
    if not pathNetschedule.endswith( os.path.sep ):
        pathNetschedule += os.path.sep

    pathCrashTest = options.pathCrashTest
    if pathCrashTest == "":
        pathCrashTest = baseDir
    if not pathCrashTest.endswith( os.path.sep ):
        pathCrashTest += os.path.sep

    pathDB = options.pathDB
    if pathDB == "":
        pathDB = baseDir + "data." + str( port )
    else:
        if not os.path.abspath( pathDB ):
            pathDB = baseDir + pathDB

    if options.verbose:
        print "Parameters to use:"
        print "Number of queues: " + str( numberOfQueues )
        print "Number of crash tests per queue: " + str( numberOfCrashTests )
        print "Number of jobs per crash test: " + str( jobs )
        print "Port: " + str( port )
        print "Dir where NS is: " + pathNetschedule
        print "Dir where crash test is: " + pathCrashTest
        print "Dir where NS stores DB: " + pathDB

    checkPrerequisites( baseDir, pathNetschedule, pathCrashTest, port )
    generateNSConfig( baseDir, numberOfQueues, port, pathDB )

    # It's time to run all the components
    if options.verbose:
        print "Launching netschedule..."

    nsCmdLine = [ pathNetschedule + "netscheduled",
                  "-conffile", baseDir + "netscheduled.ini",
                  "-logfile", baseDir + "netscheduled.log" ]
    nsProc = Popen( nsCmdLine, shell = True, stdout = PIPE, stderr = PIPE )

    time.sleep( 10 )

    crashProcs = []
    for index in xrange( numberOfQueues ):
        qName = "CRASH" + str( index )
        for crash_index in xrange( numberOfCrashTests ):
            cmdLine = pathCrashTest + "test_netschedule_crash " + \
                      " -service localhost:" + str( port ) + \
                      " -queue " + qName + \
                      " -jobs " + str( jobs ) + \
                      " -delay 1 " \
                      " -naff " + str( affs ) + \
                      " -ngroup " + str( groups ) + \
                      " -nclients " + str( clients )
            if options.verbose:
                print "Launching test_netschedule_crash #" + \
                      str( crash_index ) + " for queue " + qName
                print cmdLine
            DEVNULL = open( '/dev/null', 'w' )
            crashProcs.append( Popen( cmdLine, shell = True,
                                      stdout = DEVNULL, stderr = PIPE ) )

    if options.verbose:
        print "Waiting for crash tests finish"
    for proc in crashProcs:
        proc.wait()
        err = proc.stderr.read()
        if err != "":
            print "test_netschedule_crash stderr: " + err
        if options.verbose:
            print "test_netschedule_crash finished"

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
                   "timeout=9000\n" \
                   "notif_timeout=1.0\n" \
                   "run_timeout=8000\n" \
                   "run_timeout_precision=30\n" \
                   "delete_done=false\n" \
                   "max_input_size=1M\n" \
                   "max_output_size=1M\n"

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


def checkPrerequisites( baseDir, pathNetschedule, pathCrashTest, port ):
    " Checks that all the required files are in place "

    fname = baseDir + "netscheduled.ini.template"
    if not os.path.exists( fname ):
        raise Exception( "Cannot find configuration template file. " \
                         "Expected here: " + fname )

    fname = pathNetschedule + "netscheduled"
    if not os.path.exists( fname ):
        raise Exception( "Cannot find netschedule binary. " \
                         "Expected here: " + fname )

    fname = pathCrashTest + "test_netschedule_crash"
    if not os.path.exists( fname ):
        raise Exception( "Cannot find crash test binary. " \
                         "Expected here: " + fname )

    # Check that NS is not running
    retCode = os.system( "echo 'bla' | netcat localhost " + str( port ) )
    if retCode == 0:
        raise Exception( "NS is still running on port " + str( port ) + \
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

