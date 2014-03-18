#!/opt/python-2.5/bin/python -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server test script
"""

import sys, os.path, time

oldcwd = os.getcwd()
os.chdir( os.path.dirname( sys.argv[ 0 ] ) )

import datetime
from optparse import OptionParser
from netschedule import NetSchedule
import netschedule_tests_pack as pack
import netschedule_tests_pack_4_10 as pack_4_10
import netschedule_tests_pack_4_11 as pack_4_11
import netschedule_tests_pack_4_13 as pack_4_13
import netschedule_tests_pack_4_14 as pack_4_14
import netschedule_tests_pack_4_16 as pack_4_16


defaultGridCliPath = ""
defaultNetschedulePath = "netscheduled"

date = "/bin/date"
echo = "/bin/echo"
ps = "/bin/ps"
netstat = "/bin/netstat"
netcat = "/usr/bin/netcat"


latestNetscheduleVersion = "4.17.0"


# The map below describes what tests should be excluded for a certain
# netschedule version. The test ID is coming from netschedule_tests_pack.py
excludeTestsMap = \
{

    "4.16.8":   [ 214, 215, 800, 801, 900,
                  1000, 1100, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 1109 ],
    "4.16.9":   [ 214, 215,
                  1000, 1100, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 1109 ],
    "4.16.10":  [ 1108, 1109 ],
    "4.16.11":  [ 1108, 1109 ],
    "4.17.0":   [ 801 ]
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
    parser.add_option( "--count", dest="count",
                       default="0",
                       help="Number of tests to run" )
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
    test_count = int( options.count )

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
        if test_count > 0:
            print "Number of tests to run: " + options.count
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
              pack.Scenario08( netschedule ),
              pack.Scenario09( netschedule ),
              pack.Scenario11( netschedule ),
              pack.Scenario12( netschedule ),
              pack.Scenario13( netschedule ),
              pack.Scenario16( netschedule ),
              pack.Scenario17( netschedule ),
              pack.Scenario18( netschedule ),
              pack.Scenario22( netschedule ),
              pack.Scenario23( netschedule ),
              pack.Scenario25( netschedule ),
              pack.Scenario26( netschedule ),
              pack.Scenario27( netschedule ),
              pack.Scenario30( netschedule ),
              pack.Scenario31( netschedule ),
              pack.Scenario32( netschedule ),
              pack.Scenario33( netschedule ),
              pack.Scenario34( netschedule ),
              pack.Scenario36( netschedule ),
              pack.Scenario37( netschedule ),
              pack.Scenario38( netschedule ),
              pack.Scenario39( netschedule ),
              pack.Scenario40( netschedule ),
              pack.Scenario41( netschedule ),
              pack.Scenario46( netschedule ),
              pack.Scenario50( netschedule ),
              pack.Scenario52( netschedule ),
              pack.Scenario53( netschedule ),
              pack.Scenario54( netschedule ),
              pack.Scenario55( netschedule ),

              pack_4_10.Scenario100( netschedule ),
              pack_4_10.Scenario101( netschedule ),
              pack_4_10.Scenario102( netschedule ),
              pack_4_10.Scenario103( netschedule ),
              pack_4_10.Scenario104( netschedule ),
              pack_4_10.Scenario105( netschedule ),
              pack_4_10.Scenario106( netschedule ),
              pack_4_10.Scenario107( netschedule ),
              pack_4_10.Scenario108( netschedule ),
              pack_4_10.Scenario109( netschedule ),
              pack_4_10.Scenario110( netschedule ),
              pack_4_10.Scenario111( netschedule ),
              pack_4_10.Scenario112( netschedule ),
              pack_4_10.Scenario113( netschedule ),
              pack_4_10.Scenario114( netschedule ),
              pack_4_10.Scenario115( netschedule ),
              pack_4_10.Scenario116( netschedule ),
              pack_4_10.Scenario117( netschedule ),
              pack_4_10.Scenario118( netschedule ),
              pack_4_10.Scenario119( netschedule ),
              pack_4_10.Scenario120( netschedule ),
              pack_4_10.Scenario121( netschedule ),
              pack_4_10.Scenario122( netschedule ),
              pack_4_10.Scenario123( netschedule ),
              pack_4_10.Scenario124( netschedule ),
              pack_4_10.Scenario125( netschedule ),
              pack_4_10.Scenario126( netschedule ),
              pack_4_10.Scenario127( netschedule ),
              pack_4_10.Scenario128( netschedule ),
              pack_4_10.Scenario129( netschedule ),
              pack_4_10.Scenario131( netschedule ),
              pack_4_10.Scenario132( netschedule ),
              pack_4_10.Scenario133( netschedule ),
              pack_4_10.Scenario134( netschedule ),
              pack_4_10.Scenario135( netschedule ),
              pack_4_10.Scenario136( netschedule ),
              pack_4_10.Scenario137( netschedule ),
              pack_4_10.Scenario138( netschedule ),
              pack_4_10.Scenario139( netschedule ),
              pack_4_10.Scenario140( netschedule ),
              pack_4_10.Scenario141( netschedule ),
              pack_4_10.Scenario142( netschedule ),
              pack_4_10.Scenario143( netschedule ),
              pack_4_10.Scenario144( netschedule ),
              pack_4_10.Scenario145( netschedule ),
              pack_4_10.Scenario146( netschedule ),
              pack_4_10.Scenario147( netschedule ),
              pack_4_10.Scenario148( netschedule ),
              pack_4_10.Scenario149( netschedule ),
              pack_4_10.Scenario150( netschedule ),
              pack_4_10.Scenario151( netschedule ),
              pack_4_10.Scenario153( netschedule ),
              pack_4_10.Scenario154( netschedule ),
              pack_4_10.Scenario155( netschedule ),
              pack_4_10.Scenario156( netschedule ),
              pack_4_10.Scenario157( netschedule ),
              pack_4_10.Scenario158( netschedule ),
              pack_4_10.Scenario159( netschedule ),
              pack_4_10.Scenario160( netschedule ),
              pack_4_10.Scenario161( netschedule ),
              pack_4_10.Scenario162( netschedule ),
              pack_4_10.Scenario163( netschedule ),
              pack_4_10.Scenario164( netschedule ),
              pack_4_10.Scenario165( netschedule ),
              pack_4_10.Scenario166( netschedule ),
              pack_4_10.Scenario167( netschedule ),
              pack_4_10.Scenario168( netschedule ),
              pack_4_10.Scenario169( netschedule ),
              pack_4_10.Scenario170( netschedule ),
              pack_4_10.Scenario171( netschedule ),
              pack_4_10.Scenario172( netschedule ),
              pack_4_10.Scenario173( netschedule ),
              pack_4_10.Scenario174( netschedule ),
              pack_4_10.Scenario175( netschedule ),
              pack_4_10.Scenario176( netschedule ),
              pack_4_10.Scenario177( netschedule ),
              pack_4_10.Scenario178( netschedule ),
              pack_4_10.Scenario179( netschedule ),
              pack_4_10.Scenario180( netschedule ),
              pack_4_10.Scenario181( netschedule ),
              pack_4_10.Scenario182( netschedule ),
              pack_4_10.Scenario183( netschedule ),
              pack_4_10.Scenario184( netschedule ),
              pack_4_10.Scenario185( netschedule ),
              pack_4_10.Scenario186( netschedule ),
              pack_4_10.Scenario187( netschedule ),
              pack_4_10.Scenario188( netschedule ),
              pack_4_10.Scenario189( netschedule ),
              pack_4_10.Scenario190( netschedule ),
              pack_4_10.Scenario191( netschedule ),
              pack_4_10.Scenario192( netschedule ),
              pack_4_10.Scenario193( netschedule ),
              pack_4_10.Scenario194( netschedule ),
              pack_4_10.Scenario195( netschedule ),
              pack_4_10.Scenario196( netschedule ),
              pack_4_10.Scenario197( netschedule ),
              pack_4_10.Scenario198( netschedule ),
              pack_4_10.Scenario203( netschedule ),
              pack_4_10.Scenario204( netschedule ),
              pack_4_10.Scenario206( netschedule ),
              pack_4_10.Scenario207( netschedule ),
              pack_4_10.Scenario208( netschedule ),
              pack_4_10.Scenario212( netschedule ),
              pack_4_10.Scenario213( netschedule ),
              pack_4_10.Scenario214( netschedule ),
              pack_4_10.Scenario215( netschedule ),
              pack_4_10.Scenario220( netschedule ),
              pack_4_10.Scenario221( netschedule ),
              pack_4_10.Scenario222( netschedule ),
              pack_4_10.Scenario223( netschedule ),
              pack_4_10.Scenario224( netschedule ),
              pack_4_10.Scenario225( netschedule ),
              pack_4_10.Scenario226( netschedule ),
              pack_4_10.Scenario227( netschedule ),
              pack_4_10.Scenario228( netschedule ),
              pack_4_10.Scenario229( netschedule ),
              pack_4_10.Scenario230( netschedule ),
              pack_4_10.Scenario231( netschedule ),
              pack_4_10.Scenario232( netschedule ),
              pack_4_10.Scenario233( netschedule ),
              pack_4_10.Scenario234( netschedule ),
              pack_4_10.Scenario235( netschedule ),
              pack_4_10.Scenario236( netschedule ),
              pack_4_10.Scenario238( netschedule ),
              pack_4_10.Scenario239( netschedule ),
              pack_4_10.Scenario240( netschedule ),
              pack_4_10.Scenario241( netschedule ),
              pack_4_10.Scenario242( netschedule ),
              pack_4_10.Scenario243( netschedule ),
              pack_4_10.Scenario244( netschedule ),
              pack_4_10.Scenario245( netschedule ),
              pack_4_10.Scenario246( netschedule ),
              pack_4_10.Scenario247( netschedule ),
              pack_4_10.Scenario248( netschedule ),
              pack_4_10.Scenario249( netschedule ),
              pack_4_10.Scenario251( netschedule ),
              pack_4_10.Scenario252( netschedule ),
              pack_4_10.Scenario253( netschedule ),
              pack_4_10.Scenario254( netschedule ),
              pack_4_10.Scenario256( netschedule ),
              pack_4_10.Scenario257( netschedule ),
              pack_4_10.Scenario258( netschedule ),
              pack_4_10.Scenario259( netschedule ),
              pack_4_10.Scenario260( netschedule ),
              pack_4_10.Scenario261( netschedule ),
              pack_4_10.Scenario262( netschedule ),
              pack_4_10.Scenario263( netschedule ),
              pack_4_10.Scenario264( netschedule ),
              pack_4_10.Scenario265( netschedule ),
              pack_4_10.Scenario266( netschedule ),
              pack_4_10.Scenario267( netschedule ),

              pack_4_11.Scenario300( netschedule ),
              pack_4_11.Scenario301( netschedule ),
              pack_4_11.Scenario303( netschedule ),
              pack_4_11.Scenario304( netschedule ),
              pack_4_11.Scenario305( netschedule ),
              pack_4_11.Scenario306( netschedule ),
              pack_4_11.Scenario307( netschedule ),
              pack_4_11.Scenario308( netschedule ),
              pack_4_11.Scenario309( netschedule ),
              pack_4_11.Scenario310( netschedule ),
              pack_4_11.Scenario311( netschedule ),
              pack_4_11.Scenario312( netschedule ),
              pack_4_11.Scenario313( netschedule ),

              pack_4_13.Scenario400( netschedule ),
              pack_4_13.Scenario401( netschedule ),

              pack_4_14.Scenario500( netschedule ),
              pack_4_14.Scenario501( netschedule ),
              pack_4_14.Scenario502( netschedule ),
              pack_4_14.Scenario503( netschedule ),
              pack_4_14.Scenario504( netschedule ),
              pack_4_14.Scenario505( netschedule ),

              pack_4_16.Scenario600( netschedule ),
              pack_4_16.Scenario601( netschedule ),
              pack_4_16.Scenario602( netschedule ),
              pack_4_16.Scenario603( netschedule ),

              pack_4_16.Scenario700( netschedule ),
              pack_4_16.Scenario701( netschedule ),
              pack_4_16.Scenario702( netschedule ),
              pack_4_16.Scenario703( netschedule ),

              pack_4_16.Scenario800( netschedule ),
              pack_4_16.Scenario801( netschedule ),

              pack_4_16.Scenario900( netschedule ),

              pack_4_16.Scenario1000( netschedule ),
              pack_4_16.Scenario1100( netschedule ),
              pack_4_16.Scenario1101( netschedule ),
              pack_4_16.Scenario1102( netschedule ),
              pack_4_16.Scenario1103( netschedule ),
              pack_4_16.Scenario1104( netschedule ),
              pack_4_16.Scenario1105( netschedule ),
              pack_4_16.Scenario1106( netschedule ),
              pack_4_16.Scenario1107( netschedule ),
              pack_4_16.Scenario1108( netschedule ),
              pack_4_16.Scenario1109( netschedule ),
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

            testID = str( aTest.getScenarioID() )
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
#                raise
                failureCount += 1
                print >> sys.stderr, \
                      getTimestamp() + " Test ID " + \
                      str( aTest.getScenarioID() ) + " failed. " \
                      "Scenario: " + aTest.getScenario() + "\n" \
                      "Exception:\n" + str( exct )
                sys.stderr.flush()
            netschedule.safeStop()
            if os.path.exists( "valgrind.out" ):
                os.system( "mv valgrind.out valgrind.out." + testID )
            test_count -= 1
            if test_count == 0:
                break
        netschedule.safeStop()
        netschedule.deleteDB()
    except:
        netschedule.safeStop()
        try:
            netschedule.deleteDB()
        except:
            pass
        os.chdir( oldcwd )
#        raise

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
#        raise
        returnValue = 1

    os.chdir( oldcwd )
    sys.exit( returnValue )
