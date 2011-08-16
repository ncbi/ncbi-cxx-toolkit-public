#!/opt/python-2.5/bin/python -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server test script
"""

import os, os.path, sys, datetime
import time
from optparse import OptionParser
from netschedule import NetSchedule
import netschedule_tests_pack as pack


defaultGridCliPath = ""
defaultNetschedulePath = "netscheduled"

date = "/bin/date"
echo = "/bin/echo"
ps = "/bin/ps"
netstat = "/bin/netstat"
netcat = "/usr/bin/netcat"


latestNetscheduleVersion = "4.10.0"


# The map below describes what tests should be excluded for a certain
# netschedule version. The test ID is coming from netschedule_tests_pack.py
excludeTestsMap = \
{
    "4.8.1":    [ 12 ],
    "4.9.0":    [ 12 ],
    "4.10.0":   [ 12 ]
}


def debug( title, port ):
    " Saves some info when NS did not start "
    if False:
        suffix = " >> debug.log 2>&1"

        os.system( date + " " + suffix )
        os.system( echo + " '" + title + "' " + suffix )
        os.system( ps + " -ef " + suffix )
        os.system( netstat + " -v -p -a -n " + suffix )
        os.system( echo + " o | " + netcat + "  -v localhost " + \
                   str( port ) + " " + suffix )
    return


def getTimestamp():
    " Provides the current timestamp "
    now = datetime.datetime.now()

    year = str( now.year )

    month = str( now.month )
    if now.month <= 9:
        month = "0" + month

    day = str( now.day )
    if now.day <= 9:
        day = "0" + day

    hour = str( now.hour )
    if now.hour <= 9:
        hour = "0" + hour

    minute = str( now.minute )
    if now.minute <= 9:
        minute = "0" + minute

    second = str( now.second )
    if now.second <= 9:
        second = "0" + second

    return year + "-" + month + "-" + day + " " + \
           hour + ":" + minute + ":" + second



def parserError( parser, message ):
    " Prints the message and help on stderr "

    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return 1



def main():
    " main function for the netcache sync test "

    parser = OptionParser(
    """
    %prog  <port>
    Note #1: netschedule server will be running on the same host
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "--path-grid-cli", dest="pathGridCli",
                       default=defaultGridCliPath,
                       help="Path to grid_cli utility" )
    parser.add_option( "--path-netschedule", dest="pathNetschedule",
                       default=defaultNetschedulePath,
                       help="Path to the netschedule daemon" )
    parser.add_option( "--db-path", dest="pathDB",
                       default=os.path.dirname( \
                                    os.path.abspath( sys.argv[ 0 ] ) ) + \
                                    os.path.sep + "data",
                       help="Directory name where data are stored" )
    parser.add_option( "--start-from", dest="start_from",
                       default="0",
                       help="Test index to start from (default: 0)" )
    parser.add_option( "--header", dest="header",
                       default="",
                       help="Header for the tests output" )
    parser.add_option( "--all-to-stderr",
                       action="store_true", dest="alltostderr", default=False,
                       help="print the messages on stderr only " \
                            "(default: False)" )
    parser.add_option( "--ns-version", dest="version",
                       default=latestNetscheduleVersion,
                       help="The tested netschedule version (default:" + \
                            latestNetscheduleVersion + ")" )


    # parse the command line options
    options, args = parser.parse_args()

    if options.alltostderr:
        sys.stdout = sys.stderr


    startID = int( options.start_from )
    if startID < 0:
        raise Exception( "Negative start test ID" )

    # Check the number of arguments
    if len( args ) != 1:
        return parserError( parser, "Incorrect number of arguments" )
    port = int( args[ 0 ] )
    if port <= 0 or port > 65535:
        raise Exception( "Incorrect port number" )

    if options.verbose:
        print "Using netschedule path: " + options.pathNetschedule
        print "Using grid_cli path: " + options.pathGridCli
        print "Using DB path: " + options.pathDB
        print "Starting tests from: " + options.start_from
        print "NS version to test: " + options.version

    netschedule = None
    if options.verbose:
        netschedule = NetSchedule( "127.0.0.1", port,
                                   options.pathGridCli,
                                   options.pathNetschedule,
                                   options.pathDB,
                                   verbose = options.verbose )
    else:
        netschedule = NetSchedule( "127.0.0.1", port,
                                   options.pathGridCli,
                                   options.pathNetschedule,
                                   options.pathDB )

    tests = [ pack.Scenario00( netschedule ),
              pack.Scenario01( netschedule ),
              pack.Scenario02( netschedule ),
              pack.Scenario03( netschedule ),
              pack.Scenario04( netschedule ),
              pack.Scenario05( netschedule ),
              pack.Scenario06( netschedule ),
              pack.Scenario07( netschedule ),
              pack.Scenario08( netschedule ),
              pack.Scenario09( netschedule ),
              pack.Scenario10( netschedule ),
              pack.Scenario11( netschedule ),

              # BUG in the latest grid_cli
              pack.Scenario12( netschedule ),
              pack.Scenario13( netschedule ),

              # pack.Scenario14( netschedule ),
              #                             The test 14 is switched off because
              #                             the DROJ command has been deleted
              #                             from the latest grid_cli.
              #                             So the old versions of NS are not
              #                             able to be tested.
              # pack.Scenario15( netschedule ),
              pack.Scenario16( netschedule ),
              pack.Scenario17( netschedule ),
              pack.Scenario18( netschedule ),
              # pack.Scenario19( netschedule ),
              # pack.Scenario20( netschedule ),
              pack.Scenario21( netschedule ),
              pack.Scenario22( netschedule ),
              pack.Scenario23( netschedule ),
              # pack.Scenario24( netschedule ),
              pack.Scenario25( netschedule ),
              pack.Scenario26( netschedule ),
              pack.Scenario27( netschedule ),
              pack.Scenario28( netschedule ),
              pack.Scenario29( netschedule ),
              pack.Scenario30( netschedule ),
              pack.Scenario31( netschedule ),
              pack.Scenario32( netschedule ),
              pack.Scenario33( netschedule ),
              # pack.Scenario34( netschedule ),
              pack.Scenario35( netschedule ),
              pack.Scenario36( netschedule ),
              pack.Scenario37( netschedule ),
              pack.Scenario38( netschedule ),
              pack.Scenario39( netschedule ),
              pack.Scenario40( netschedule ),
              pack.Scenario41( netschedule ),
              pack.Scenario42( netschedule ),
              # pack.Scenario43( netschedule ),
              pack.Scenario44( netschedule ),
              # pack.Scenario45( netschedule ),
              pack.Scenario46( netschedule ),
              pack.Scenario47( netschedule ),
              pack.Scenario48( netschedule ),
              pack.Scenario49( netschedule ),
              # pack.Scenario50( netschedule ),
              pack.Scenario51( netschedule ),
              pack.Scenario52( netschedule ),
              pack.Scenario53( netschedule ),
              pack.Scenario54( netschedule ),
              # pack.Scenario55( netschedule ),
              # pack.Scenario56( netschedule )
            ]

    # Calculate the start test index
    startIndex = 0
    while startIndex < len( tests ):
        if tests[ startIndex ].getScenarioID() >= startID:
            break
        startIndex += 1

    successCount = 0
    failureCount = 0
    skippedCount = 0
    excludeList = []
    if excludeTestsMap.has_key( options.version ):
        excludeList = excludeTestsMap[ options.version ]

    if options.header != "":
        print options.header

    netschedule.safeStop()
    try:
        for index in range( startIndex, len( tests ) ):
            aTest = tests[ index ]

            if aTest.getScenarioID() in excludeList:
                print getTimestamp() + " Test ID " + \
                      str( aTest.getScenarioID() ) + " skipped. " \
                      "It is in the exclude list for NS v." + options.version
                skippedCount += 1
                continue

            try:
                netschedule.resetPID()
                succeeded = aTest.execute()
                if succeeded:
                    successCount += 1
                    print getTimestamp() + " Test ID " + \
                          str( aTest.getScenarioID() ) + " succeeded"
                    sys.stdout.flush()
                else:
                    print >> sys.stderr, \
                          getTimestamp() + " Test ID " + \
                          str( aTest.getScenarioID() ) + " failed. " \
                          "Scenario: " + aTest.getScenario()
                    sys.stderr.flush()
                    failureCount += 1
            except Exception, exct:
                failureCount += 1
                print >> sys.stderr, \
                      getTimestamp() + " Test ID " + \
                      str( aTest.getScenarioID() ) + " failed. " \
                      "Scenario: " + aTest.getScenario() + "\n" \
                      "Exception:\n" + str( exct )
                sys.stderr.flush()
            time.sleep( 5 )
        netschedule.safeStop()
        netschedule.deleteDB()
    except:
        netschedule.safeStop()
        try:
            netschedule.deleteDB()
        except:
            pass
        raise

    print getTimestamp() + " Total succeeded: " + str( successCount )
    print getTimestamp() + " Total skipped: " + str( skippedCount )
    print getTimestamp() + " Total failed: " + str( failureCount )

    if failureCount > 0:
        return 1
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
        raise
        returnValue = 1

    sys.exit( returnValue )

