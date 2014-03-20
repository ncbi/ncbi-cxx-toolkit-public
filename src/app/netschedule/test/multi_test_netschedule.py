#!/opt/python-2.5/bin/python -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server test script
"""

import os.path, sys
import tempfile
import logging
from subprocess import Popen, PIPE
from optparse import OptionParser
from test_netschedule import latestNetscheduleVersion

grep = "/bin/grep"

GRID_CLI_PATHS = [ "$NCBI/c++.metastable/DebugMT" ]

configFiles = [ "netscheduled.ini.1", "netscheduled.ini.2",
                "netscheduled.ini.3", "netscheduled.ini.4",
                "netscheduled.ini.5", "netscheduled.ini.6",
                "netscheduled.ini.7", "netscheduled.ini.8",
                "netscheduled.ini.9",
                "netscheduled.ini.505-1", "netscheduled.ini.505-2",
                "netscheduled.ini.505-3", "netscheduled.ini.505-4",
                "netscheduled.ini.505-5",
                "netscheduled.ini.1.1000", "netscheduled.ini.1100",
                "netscheduled.ini.1200" ]
scripts = [ "make_ncbi_grid_module_tree.sh", "netschedule.py",
            "netschedule_tests_pack.py", "netschedule_tests_pack_4_10.py",
            "netschedule_tests_pack_4_11.py", "netschedule_tests_pack_4_13.py",
            "netschedule_tests_pack_4_14.py", "netschedule_tests_pack_4_16.py",
            "ns.py", "test_netschedule.py" ]



def safeRun( commandArgs ):
    " Provides the process stdout "
    stdOut, stdErr = safeRunWithStderr( commandArgs )
    return stdOut


def safeRunWithStderr( commandArgs ):
    " Runs the given command and provides both stdout and stderr "

    errTmp = tempfile.mkstemp()
    errStream = os.fdopen( errTmp[ 0 ] )
    process = Popen( commandArgs, stdin = PIPE,
                     stdout = PIPE, stderr = errStream )
    process.stdin.close()
    processStdout = process.stdout.read()
    process.stdout.close()
    errStream.seek( 0 )
    err = errStream.read()
    errStream.close()
    process.wait()
    try:
        # On WinXX the file might still be kept and unlink generates exception
        os.unlink( errTmp[ 1 ] )
    except:
        pass

    # 'grep' return codes:
    # 0 - OK, lines found
    # 1 - OK, no lines found
    # 2 - Error occured

    if process.returncode == 0 or \
       ( os.path.basename( commandArgs[ 0 ] ) == "grep" and \
         process.returncode == 1 ):
        # No problems, the ret code is 0 or the grep special case
        return processStdout, err.strip()

    # A problem has been identified
    raise Exception( "Error in '%s' invocation: %s" % \
                     (commandArgs[0], err) )


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
    %prog
    Note #1: netschedule server will be running on the same host
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "--port", dest="port",
                       default=9109,
                       help="Netschedule daemon port to be used for tests. "
                            "Default: 9109" )


    # parse the command line options
    options, args = parser.parse_args()

    if len( args ) == 0:
        return parserError( parser,
                            "At least one NetSchedule version is expected" )

    port = int( options.port )
    if port < 1024 or port > 65535:
        raise Exception( "Invalid port number " + str( port ) )

    checkPrerequisites()

    # Build a list of the netscheduled daemons.
    # Each args is a NS version from the releases storage or an exact path to
    # the directory where NS is.
    nsList = buildExecutablesList( args )
    if len( nsList ) == 0:
        print >> sys.stderr, "No netscheduled binaries are found at " + \
                             str( args ) + ". Exiting."
        return 0

    # Test all the combinations of the grid_cli components and NSs
    tempPath = createSandbox()
    trimNSLogFile( tempPath )

    if options.verbose:
        print "Found NS versions: " + str( nsList )
        print "Grid locations: " + str( GRID_CLI_PATHS )
        print "Sandbox: " + tempPath

    numberOfFailed = 0
    for oneNS in nsList:
        for oneGC in GRID_CLI_PATHS:
            numberOfFailed += testOneCombination( tempPath, oneNS, oneGC,
                                                  port, options.verbose )

    return numberOfFailed


def isVersionFormat( line ):
    " Returns True if it is a version string of the x.x.x format "
    parts = line.split( '.' )
    if len(parts) != 3:
        return False
    try:
        if str( int( parts[0] ) ) != parts[0]:
            return False
        if str( int( parts[1] ) ) != parts[1]:
            return False
        if str( int( parts[2] ) ) != parts[2]:
            return False
    except ValueError:
        return False
    return True


def guessNSVersion( elfPath, linkName, defaultVersion ):
    """ The function guesses the netschedule version.
        Denis invented the following procedure:
        - run --version and use it if not 0.0.0 is provided
        - check the link name. If it matches x.x.x then use it
        - assume that it is the latest version

        The guess version problem comes from the fact that NS does not provide
        a proper version name unless it is buit using prepare_release AND the
        bug of improperly printed version is fixed.
    """

    # Guess 1
    cmdLine = [ elfPath, "-version",
                "|", grep, "Package:" ]
    line = safeRun( cmdLine ).strip()
    if line != "":
        # e.g.: Package: netschedule 4.8.1, build Jun  7 2011 16:07:33
        line = line.split( ',' )[ 0 ].replace( 'Package: netschedule ', "" )
        if line != '0.0.0':
            if isVersionFormat( line ):
                return line

    # Guess 2
    line = os.path.basename( linkName )
    if isVersionFormat( line ):
        return line

    return defaultVersion


def testOneCombination( sandboxPath, nsPathVer, gcPath, port, verbose ):
    " Tests one combination, returns 1 if failed "

    # Expand grid CLI path
    gcPath = gcPath.replace( "$NCBI", os.environ[ "NCBI" ] )
    nsPath = nsPathVer[ 0 ]
    nsVer = nsPathVer[ 1 ]

    combination = "Testing combination:\nNS: " + nsPath + \
                  "\nGRID_CLI: " + gcPath
    logging.info( combination )

    if verbose:
        print combination

    # Copy binaries to the sandbox
    if os.system( "cp -p " + nsPath + " " + \
                  sandboxPath + os.path.sep + "netscheduled" ) != 0:
        print >> sys.stderr, combination
        print >> sys.stderr, "Error copying binary to sandbox: " + nsPath
        print >> sys.stderr, ""
        logging.error( "Error copying binary to sandbox: " + nsPath )
        return 1

    if nsVer is None:
        nsVersion = guessNSVersion( sandboxPath + os.path.sep + "netscheduled",
                                    nsPath, latestNetscheduleVersion )
        if verbose:
            print "Detected NS version: " + nsVersion
    else:
        nsVersion = nsVer


    # Now, run the script which prepares the grid_cli staff
    os.system( "rm -rf " + sandboxPath + "ncbi_grid_1_0" )
    if os.system( sandboxPath + "make_ncbi_grid_module_tree.sh " + gcPath +
                  " " + sandboxPath + "ncbi_grid_1_0" ) != 0:
        print >> sys.stderr, combination
        print >> sys.stderr, "Error creating grid_cli staff in sandbox: " + gcPath
        print >> sys.stderr, ""
        logging.error( "Error creating grid_cli staff in sandbox: " + gcPath )
        return 1

    # Binary is here as well the configuration files
    # We can run the tests now
    cmdLine = [ sandboxPath + "test_netschedule.py", str( port ),
                "--path-grid-cli=" + sandboxPath + "ncbi_grid_1_0/",
                "--path-netschedule=" + sandboxPath,
                "--db-path=/dev/shm/data",
                "--header='" + combination + "'",
                "--all-to-stderr",
                "--ns-version=" + nsVersion ]

    if verbose:
        cmdLine += [ "--verbose" ]
        print "Executing command: " + " ".join( cmdLine )

    errTmp = tempfile.mkstemp()
    errStream = os.fdopen( errTmp[ 0 ] )
    process = Popen( cmdLine, stdin = PIPE,
                     stdout = PIPE, stderr = errStream )
    process.stdin.close()
    processStdout = process.stdout.read()   # analysis:ignore
    process.stdout.close()
    errStream.seek( 0 )
    err = errStream.read()
    errStream.close()
    process.wait()
    os.unlink( errTmp[ 1 ] )

    if process.returncode != 0:
        print >> sys.stderr, err
        print >> sys.stderr, ""
        logging.error( "Test failed" )
        return 1

    logging.info( "Test succedded" )
    return 0


def createSandbox():
    " Creates a sandbox and provides the path to it "

    basePath = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
            os.path.sep
    path = basePath + "sandbox" + os.path.sep

    if not os.path.exists( path ):
        os.mkdir( path )

    retCode = 0
    for fName in configFiles:
        retCode += os.system( "cp " + basePath + fName + " " + path + fName )
    for fName in scripts:
        retCode += os.system( "cp " + basePath + fName + " " + path + fName )

    if retCode != 0:
        raise Exception( "Error copying test framework files." )

    return path


def trimNSLogFile( tempPath ):
    " Trims the NS log file if it became too long "

    limit = 200 * 1024 * 1024   # 200M
    logFileName = tempPath + "netscheduled.log"

    if not os.path.exists( logFileName ):
        return

    fileSize = os.path.getsize( logFileName )
    if fileSize > limit:
        os.system( "tail -c " + str( limit ) + " " + logFileName + \
                   " > " + logFileName + ".temp" )
        os.system( "mv -f " + logFileName + ".temp " + logFileName )
    return


def setupLogging():
    " Sets up the logging "

    fName = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
            os.path.sep + "multi_test_netschedule.log"

    logging.basicConfig( level = logging.DEBUG,
                         format = "%(levelname) -10s %(asctime)s %(message)s",
                         filename = fName)
    return


def checkPrerequisites():
    " Checks that all the required files are in place "

    basePath = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
               os.path.sep

    if not os.environ.has_key( "NCBI" ):
        raise Exception( "$NCBI variable must be set before running the script" )

    for fName in configFiles:
        if not os.path.exists( basePath + fName ):
            raise Exception( "Cannot find configuration file. Expected: " +
                             basePath + fName )

    for script in scripts:
        if not os.path.exists( basePath + script ):
            raise Exception( "Cannot find test script. Expected: " +
                             basePath + script )
    return


def buildExecutablesList( paths ):
    """ Paths is a list of paths or NS version from the releases storage """

    result = []
    for path in paths:
        path = path.replace( "$NCBI", os.environ[ "NCBI" ] ).strip()
        if not path:
            continue
        if path[ 0 ].isdigit():
            # This is a version from the release storage
            candidatePath = "/am/ncbiapdata/release/netschedule/builds/" + \
                            path + "/Linux64/"
            if not os.path.exists( candidatePath ):
                logging.warning( "Cannot find NetSchedule " + path + " at " +
                                 candidatePath + ". Skipping." )
                continue

            # Find release directory
            found = False
            for item in os.listdir( candidatePath ):
                if "ReleaseMT64" in item:
                    # Here it is
                    candidatePath += item
                    found = True
                    break
            if not found:
                logging.warning( "Cannot find NetSchedule " + path + " at " +
                                 candidatePath + ". Skipping." )
                continue

            candidatePath += "/bin/netscheduled"
            if not os.path.exists( candidatePath ):
                logging.warning( "Cannot find NetSchedule " + path + " at " +
                                 candidatePath + ". Skipping." )
                continue

            # Great, NS found
            result.append( (os.path.abspath( candidatePath ), path) )
        else:
            # This is a directory where NS is or a the binary itself
            path = os.path.abspath( path )
            if os.path.isdir( path ):
                if not path.endswith( "/" ):
                    path += "/netscheduled"

            if not os.path.exists( path ):
                logging.warning( "Cannot find NetSchedule " + path + ". Skipping." )
                continue

            if not os.access( path, os.X_OK ):
                logging.warning( "The path points to a non-executable file: " + path )
                continue    # No permissions to execute

            # It is supposed that this is the latest version
            result.append( (path, None) )
    return result

#def getCacheFileName():
#    " provides the file name where tested cache is stored "
#    return os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
#           os.path.sep + "already_tested.cache"

#def getTestedCache():
#    " Reads the tested files cache "
#    cache = {}
#    fName = getCacheFileName()
#
#    if not os.path.exists( fName ):
#        return
#    f = open( fName, "r" )
#    for line in f:
#        line = line.strip()
#        if line == "":
#            continue
#        if line.startswith( '#' ):
#            continue
#        parts = line.split()
#        if len( parts ) != 2:
#            raise Exception( "Cache file malformed" )
#        cache[ parts[ 0 ] ] = parts[ 1 ]
#    return cache

#def getMD5( path ):
#    " Provides the MD5 sum for the given file "
#    f = open( path, "rb" )
#    md5Sum = md5.new()
#    while 1:
#        block = f.read( 1024 * 1024 )
#        if not block:
#            break
#        md5Sum.update( block )
#    f.close()
#    digest = md5Sum.digest()
#    return ( "%02x"*len( digest ) ) % tuple( map( ord, digest ) )

#def updateTestedCache( fName ):
#    " Updates the cached MD5 for the given link "
#    cache = getTestedCache()
#    cache[ fName ] = getMD5( os.path.realpath( fName ) )
#    f = open( getCacheFileName(), "w" )
#    for key in cache:
#        f.write( key + " " + cache[ key ] + "\n" )
#    f.close()
#    return

#def excludeTested( files ):
#    """ Excludes those files which have already been tested.
#        Returns the number of excluded paths. """
#
#    cache = getTestedCache()
#
#    result = []
#    count = 0
#    for index in xrange( 0, len( files ) ):
#        if cache.has_key( files[ index ] ):
#            abspath = os.path.realpath( files[ index ] )
#            # Check the new MD5
#            if cache[ files[ index ] ] == getMD5( abspath ):
#                # The same, no need
#                count += 1
#                continue
#        result.append( files[ index ] )
#    return count



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
