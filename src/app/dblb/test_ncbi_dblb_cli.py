#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Authors: Sergey Satskiy, Aaron Ucko
#
# $Id$
#

" Unit test for ncbi_dblb_cli "

import sys

# Check the python version explicitly
if sys.version_info[ 0 ] < 2 or \
   (sys.version_info[ 0 ] == 2 and sys.version_info[ 1 ] < 4):
    print >> sys.stderr, \
            "Python version 2.4 or higher is required. You have " + \
            str( sys.version_info[ 0 ] ) + "." + str( sys.version_info[ 1 ] )
    sys.exit( 1 )

import unittest, os.path, tempfile, os
from subprocess import Popen, PIPE
from optparse import OptionParser


# Default number of tries in each test
TRIES = 3

NONEXISTENT_SERVICE                  = "NCBI_DBLB_QA_DOES_NOT_EXIST"
EXISTING_SERVICE                     = "NCBI_DBLB_QA_UP"
ALTERNATE_SERVICES                   = [ "NCBI_DBLB_QA_UP_REVDNS1",
                                         "NCBI_DBLB_QA_UP_REVDNS2" ]
EXISTING_SERVICE_ALWAYS_DOWN         = "NCBI_DBLB_QA_DOWN"
EXISTING_SERVICE_WITHOUT_REVERSE_DNS = "NCBI_DBLB_QA_UP_NO_REVERSE_DNS"

# Updated in main()
NCBI_DBLB_PATH = ""


class NCBIDBLBTest( unittest.TestCase ):
    " Unit test class "

    def test_alternative_services( self ):
        " Testing against a service that should exist on multiple hosts "

        resolved = []
        count = TRIES
        while count > 0:

            args = [ NCBI_DBLB_CLI, 'whereis', EXISTING_SERVICE ]
            retCode, stdout, stderr = safeRun( args )

            errorMsg = "RetCode in case of existing " \
                       "service must be 0"
            self.failIf( count == 1 and retCode != 0, errorMsg )

            if retCode == 0:
                break

            count -= 1

        lines = stdout.rstrip().split('\n')
        for x in lines:
            x = x[0:x.index('\t')]
            errorMsg = "The output must not match the service name"
            self.failIf( x == EXISTING_SERVICE, errorMsg )

            resolved.append( x )

        # Sort the resolved list and compare with the expected
        resolved.sort()
        errorMsg = "Existing service expects to alternate between " + \
                   str( len( ALTERNATE_SERVICES ) ) + " (" + \
                   ", ".join( ALTERNATE_SERVICES ) + ") services. " \
                   "In reality, it alternates between " + \
                   str( len( resolved ) ) + ": " + \
                   ", ".join( resolved )
        self.failIf( resolved != ALTERNATE_SERVICES, errorMsg )
        return


    # No exclusion support at present
    def DISABLED_test_skipping_services1( self ):
        """ Testing against a service where two hosts are available
            and one of them is skipped """

        resolved = []
        expected = ALTERNATE_SERVICES[ 1: ]
        exclude  = [ ALTERNATE_SERVICES[ 0 ] ]
        count = TRIES
        while count > 0:

            args = [ NCBI_DBLB_PATH, '-q', EXISTING_SERVICE ] + \
                   exclude

            retCode, stdout, stderr = safeRun( args )

            errorMsg = "RetCode in case of existing " \
                       "service must be 0"
            self.failIf( retCode != 0, errorMsg )

            errorMsg = "The output must not match the service name"
            self.failIf( stdout == EXISTING_SERVICE, errorMsg )

            if not stdout in resolved:
                resolved.append( stdout )
            count -= 1

        resolved.sort()
        errorMsg = "Existing service expects to alternate between " + \
                   str( len( expected ) ) + " (" + \
                   ", ".join( expected ) + ") services. " \
                   "In reality, it alternates between " + \
                   str( len( resolved ) ) + ": " + \
                   ", ".join( resolved )
        self.failIf( resolved != expected, errorMsg )
        return


    # No exclusion support at present
    def DISABLED_test_skipping_services2( self ):
        """ Testing against a service where many hosts are available
            and ALL of them are skipped """

        count = TRIES
        while count > 0:

            args = [ NCBI_DBLB_PATH, '-q', EXISTING_SERVICE ] + \
                   ALTERNATE_SERVICES
            retCode, stdout, stderr = safeRun( args )

            errorMsg = "RetCode when all alternatives are skipped " \
                       "must not be 0."
            self.failIf( retCode == 0, errorMsg )

            errorMsg = "The output must match the service name"
            self.failIf( stdout != EXISTING_SERVICE, errorMsg )

            count -= 1
        return


    def test_nonexistent_service_whatis( self ):
        " Testing against a nonexistent service in whatis mode "

        count = TRIES
        while count > 0:
            args = [ NCBI_DBLB_CLI, 'whatis', NONEXISTENT_SERVICE ]
            retCode, stdout, stderr = safeRun( args )

            errorMsg = "RetCode in case of nonexistent " \
                       "service must be 1"
            self.failIf( retCode != 1, errorMsg )

            errorMsg = "The output must match the service name"
            self.failIf( not stdout.startswith(NONEXISTENT_SERVICE
                                               + " is unknown"),
                         errorMsg )

            count -= 1
        return


    def test_nonexistent_service_whereis( self ):
        " Testing against a nonexistent service in whereis mode "

        count = TRIES
        while count > 0:
            args = [ NCBI_DBLB_CLI, 'whereis', NONEXISTENT_SERVICE ]
            retCode, stdout, stderr = safeRun( args )

            errorMsg = "RetCode in case of nonexistent " \
                       "service must be 1"
            self.failIf( retCode != 1, errorMsg )

            errorMsg = "The output must be empty"
            self.failIf( len(stdout) > 0, errorMsg )

            count -= 1
        return


    def test_no_reverse_dns( self ):
        """ Testing against a service which does not have
            an accompanying reverse DNS record """

        count = TRIES
        while count > 0:
            args = [ NCBI_DBLB_CLI, 'whatis',
                     EXISTING_SERVICE_WITHOUT_REVERSE_DNS ]
            retCode, stdout, stderr = safeRun( args )

            errorMsg = "RetCode in case of an existing service without a " \
                       "reverse DNS record must still be 0"
            self.failIf( retCode != 0, errorMsg )

            errorMsg = "The output must report type STANDALONE"
            self.failIf( stdout.find('STANDALONE') < 0, errorMsg )

            count -= 1
        return


def safeRun( commandArgs ):
    " Runs the given command and reads the output "

    errTmp = tempfile.mkstemp()
    errStream = os.fdopen( errTmp[0] )
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
        os.unlink( errTmp[1] )
    except:
        pass

    return process.returncode, processStdout.strip(), err.strip()


def parserError( parser, message ):
    " Prints the message and help on stderr "

    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return


def checkServiceAvailability():
    " Checks that LBSMC is aware of all the required services "

    servicesToBeAlive = [ "NCBI_DBLB_QA_UP", "NCBI_DBLB_QA_UP_REVDNS1",
                          "NCBI_DBLB_QA_UP_REVDNS2",
                          "NCBI_DBLB_QA_UP_NO_REVERSE_DNS" ]
    servicesToBeDead = [ "NCBI_DBLB_QA_DOES_NOT_EXIST", "NCBI_DBLB_QA_DOWN" ]

    errors = 0
    for name in servicesToBeAlive:
        if os.system( "/opt/machine/lbsm/bin/test_mghbn -du1 " + \
                      name + " > /dev/null" ) != 0:
            print >> sys.stderr, "The service " + name + \
                                 " is not known to LBSMD. " \
                                 "Unit tests cannot be executed."
            errors += 1

    for name in servicesToBeDead:
        if os.system( "/opt/machine/lbsm/bin/test_mghbn -du1 " + \
                      name + " > /dev/null" ) == 0:
            print >> sys.stderr, "The service " + name + \
                                 " is known to LBSMD while it must not be. " \
                                 "Unit tests cannot be executed."
            errors += 1

    return errors


def main():
    " Main script entry point "

    global TRIES
    global NCBI_DBLB_CLI

    parser = OptionParser(
        """%prog [options] [path/to/ncbi_dblb_cli]
       Tests the ncbi_dblb_cli utility""" )

    parser.add_option( "--tries", type = "int",
                       dest = "tries", default = TRIES,
                       help = "Number of tries to execute each test" )
    options, args = parser.parse_args()

    if not len( args ) in [ 0, 1 ]:
        parserError( parser, "Incorrect number of arguments" )
        return 2
    TRIES = options.tries
    if TRIES <= 0:
        print >> sys.stderr, "Tries counter cannot be <= 0"
        return 2

    # Check that lbsmc is aware of all the required services
    if checkServiceAvailability() != 0:
        return 1

    if len( args ) == 1:
        NCBI_DBLB_CLI = args[ 0 ]
        # unittest also wants to analyse sys.argv, so exclude our argument
        sys.argv.remove( NCBI_DBLB_CLI )
    else:
        NCBI_DBLB_CLI = os.path.dirname( os.path.abspath( sys.argv[0] ) ) + \
                                          "/ncbi_dblb_cli"
    if not os.path.exists( NCBI_DBLB_CLI ):
        print >> sys.stderr, "Cannot find ncbi_dblb_cli. Expected here: " + \
                             NCBI_DBLB_CLI
        return 2

    unittest.main()
    return 0


if __name__ == '__main__':
    sys.exit( main() )

