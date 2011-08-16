#!/opt/python-2.5/bin/python -u
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Netschedule server test script
"""

import os, os.path, sys, md5
import tempfile
import logging
from subprocess import Popen, PIPE
from optparse import OptionParser
from test_netschedule import latestNetscheduleVersion

grep = "/bin/grep"



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
    parser.add_option( "--path-grid-cli", dest="pathGridCli",
                       default="",
                       help="Path to directory where grid_cli " \
                            "utility links are" )
    parser.add_option( "--path-netschedule", dest="pathNetschedule",
                       default="",
                       help="Path to directory where netschedule " \
                            "daemon links are" )
    parser.add_option( "--port", dest="port",
                       default=9109,
                       help="Netschedule daemon port to be used for tests. " \
                            "Default: 9109" )


    # parse the command line options
    options, args = parser.parse_args()

    if len( args ) != 0:
        return parserError( parser, "No arguments are expected" )

    port = int( options.port )
    if port < 1024 or port > 65535:
        raise Exception( "Invalid port number " + str( port ) )

    checkPrerequisites()

    # Build a list of grid_cli utilities
    pathGridCli = options.pathGridCli
    if pathGridCli == "":
        pathGridCli = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
                      os.path.sep + "grid_cli" + os.path.sep

    if not pathGridCli.endswith( os.path.sep ):
        pathGridCli += os.path.sep

    if not os.path.exists( pathGridCli ):
        raise Exception( "Cannot find grid_cli links directory. " \
                         "Expected here: " + pathGridCli )

    gridCliList = buildExecutablesList( pathGridCli )
    if len( gridCliList ) == 0:
        print >> sys.stderr, "No grid_cli binaries are found at " + \
                             pathGridCli + ". Exiting."
        return 0

    # Build a list of the netscheduled daemons
    pathNS = options.pathNetschedule
    if pathNS == "":
        pathNS = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
                 os.path.sep + "netscheduled" + os.path.sep

    if not pathNS.endswith( os.path.sep ):
        pathNS += os.path.sep

    if not os.path.exists( pathNS ):
        raise Exception( "Cannot find netscheduled links directory. " \
                         "Expected here: " + pathNS )

    nsList = buildExecutablesList( pathNS )
    if len( nsList ) == 0:
        print >> sys.stderr, "No netscheduled binaries are found at " + \
                             pathNS + ". Exiting."
        return 0


    # Form the lists of absolutely new items
    # May be used later
    # gridCliListNew = excludeTested( gridCliList )
    # nsListNew = excludeTested( nsList )

    #if len( nsListNew ) + len( gridCliListNew ) == 0:
    #    # Nothing to test
    #    return 0


    # Here: we have some pairs to be tested
    #for oneNS in nsListNew:
    #    for oneGC in gridCliList:
    #        testOneCombination( oneNS, oneGC )
    #    updateTestedCache( oneNS )

    #for oneGC in gridCliListNew:
    #    for oneNS in nsList:
    #        testOneCombination( oneNS, oneGC )
    #    updateTestedCache( oneGC )

    # Simple approach - test all the combinations
    tempPath = createSandbox()
    trimNSLogFile( tempPath )

    numberOfFailed = 0
    for oneNS in nsList:
        for oneGC in gridCliList:
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


def testOneCombination( sandboxPath, nsPath, gcPath, port, verbose ):
    " Tests one combination, returns 1 if failed "

    combination = "Testing combination: NS: " + nsPath + \
                  " GRID_CLI: " + gcPath
    logging.info( combination )
    basePath = os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
               os.path.sep

    # Copy binaries to the sandbox
    if os.system( "cp -p " + nsPath + " " + \
                  sandboxPath + os.path.sep + "netscheduled" ) != 0:
        print >> sys.stderr, combination
        print >> sys.stderr, "Error copying binary to sandbox: " + nsPath
        print >> sys.stderr, ""
        logging.error( "Error copying binary to sandbox: " + nsPath )
        return 1
    if os.system( "cp -p " + gcPath + " " + \
                  sandboxPath + os.path.sep + "grid_cli" ) != 0:
        print >> sys.stderr, combination
        print >> sys.stderr, "Error copying binary to sandbox: " + gcPath
        print >> sys.stderr, ""
        logging.error( "Error copying binary to sandbox: " + gcPath )
        return 1

    nsVersion = guessNSVersion( sandboxPath + os.path.sep + "netscheduled",
                                nsPath, latestNetscheduleVersion )

    # Binary is here as well the configuration files
    # We can run the tests now
    cmdLine = [ basePath + "test_netschedule.py", str( port ),
                "--path-grid-cli=" + sandboxPath,
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
    processStdout = process.stdout.read()
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

    retCode = os.system( "cp " + basePath + "netscheduled.ini.1 " + \
                         path + "netscheduled.ini.1" )
    retCode += os.system( "cp " + basePath + "netscheduled.ini.2 " + \
                          path + "netscheduled.ini.2" )
    if retCode != 0:
        raise Exception( "Error copying netschedule configuration files." )

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

    fName = basePath + "netscheduled.ini.1"
    if not os.path.exists( fName ):
        raise Exception( "Cannot find configuration file. Expected: " + fName )

    fName = basePath + "netscheduled.ini.2"
    if not os.path.exists( fName ):
        raise Exception( "Cannot find configuration file. Expected: " + fName )

    fName = basePath + "test_netschedule.py"
    if not os.path.exists( fName ):
        raise Exception( "Cannot find test script. Expected: " + fName )

    return


def buildExecutablesList( path ):
    """ Analyzes all the symbolic links in the given dir and builds a list of
        those which are not broken and point to executables """

    result = []
    for item in os.listdir( path ):
        candidate = path + item
        if not os.path.islink( candidate ):
            continue    # Not a symlink at all
        if not os.path.exists( candidate ):
            logging.warning( "Broken symlink detected: " + candidate )
            continue    # Broken link
        if not os.access( candidate, os.X_OK ):
            logging.warning( "Symlink to a non-executable file: " + candidate )
            continue    # No permissions to execute

        result.append( candidate )
    return result

def getCacheFileName():
    " provides the file name where tested cache is stored "
    return os.path.dirname( os.path.abspath( sys.argv[ 0 ] ) ) + \
           os.path.sep + "already_tested.cache"

def getTestedCache():
    " Reads the tested files cache "
    cache = {}
    fName = getCacheFileName()

    if not os.path.exists( fName ):
        return
    f = open( fName, "r" )
    for line in f:
        line = line.strip()
        if line == "":
            continue
        if line.startswith( '#' ):
            continue
        parts = line.split()
        if len( parts ) != 2:
            raise Exception( "Cache file malformed" )
        cache[ parts[ 0 ] ] = parts[ 1 ]
    return cache

def getMD5( path ):
    " Provides the MD5 sum for the given file "
    f = open( path, "rb" )
    md5Sum = md5.new()
    while 1:
        block = f.read( 1024 * 1024 )
        if not block:
            break
        md5Sum.update( block )
    f.close()
    digest = md5Sum.digest()
    return ( "%02x"*len( digest ) ) % tuple( map( ord, digest ) )

def updateTestedCache( fName ):
    " Updates the cached MD5 for the given link "
    cache = getTestedCache()
    cache[ fName ] = getMD5( os.path.realpath( fName ) )
    f = open( getCacheFileName(), "w" )
    for key in cache:
        f.write( key + " " + cache[ key ] + "\n" )
    f.close()
    return

def excludeTested( files ):
    """ Excludes those files which have already been tested.
        Returns the number of excluded paths. """

    cache = getTestedCache()

    result = []
    count = 0
    for index in xrange( 0, len( files ) ):
        if cache.has_key( files[ index ] ):
            abspath = os.path.realpath( files[ index ] )
            # Check the new MD5
            if cache[ files[ index ] ] == getMD5( abspath ):
                # The same, no need
                count += 1
                continue
        result.append( files[ index ] )
    return count



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

