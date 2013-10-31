#!/usr/bin/env python
#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
Utility to compare NS config files
"""

import sys
import os
from sets import Set
from optparse import OptionParser
from ConfigParser import ConfigParser
from subprocess import Popen, PIPE
from StringIO import StringIO
from ncbi_grid_1_0.ncbi.grid import ipc, ns
from distutils.version import StrictVersion
import tempfile


def main():
    " The utility entry point "

    parser = OptionParser(
    """
    %prog  <NS ini file>  <another NS ini file> [options]

    e.g. %prog  netscheduled.ini.old  netscheduled.ini
    NetSchedule configuration files comparing utility.
    It compares the given ini files and prints the difference

    NS ini file can be given as host:port[:effective]
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )
    parser.add_option( "-s", "--no-sed",
                       action="store_true", dest="nosed", default=False,
                       help="avoid sed preprocessing (default: False)" )
    parser.add_option( "-e", "--no-extra",
                       action="store_true", dest="noextra", default=False,
                       help="don't complain about extra values for queues (default: False)" )
    parser.add_option( "-a", "--ns-admin", dest="nsadmin",
                       default="netschedule_admin",
                       help="NS Server admin name (default: netschedule_admin)" )
    parser.add_option( "-w", "--raw", dest="raw", default=False,
                       action="store_true",
                       help="Do raw comparison (empty values are not stripped) (default: False)" )


    # parse the command line options
    options, args = parser.parse_args()
    verbose = options.verbose
    nosed = options.nosed
    noextra = options.noextra
    raw = options.raw

    # Check the number of arguments
    if len( args ) != 2:
        return parserError( parser, "Incorrect number of arguments" )

    lhsIniFile = args[ 0 ]
    rhsIniFile = args[ 1 ]


    lhsContent = getIniContent( "first", lhsIniFile, options.nsadmin )
    rhsContent = getIniContent( "second", rhsIniFile, options.nsadmin )

    if verbose:
        print "First config file: " + lhsIniFile
        print "Second config file: " + rhsIniFile
        sys.stdout.flush()

    lhsConfig = parseConfigContent( lhsContent, nosed )
    rhsConfig = parseConfigContent( rhsContent, nosed )

    removeQuotas( lhsConfig, raw )
    removeQuotas( rhsConfig, raw )

    lhsSections = lhsConfig.sections()
    rhsSections = rhsConfig.sections()

    # Sort the found sections
    lClasses, lQueues, lOther = splitSections( lhsSections )
    rClasses, rQueues, rOther = splitSections( rhsSections )

    # Compare other sections
    retCode = compareOtherSections( lhsConfig, rhsConfig,
                                    lOther, rOther )

    # No need to compare classes, so check the set of queues
    lQueueSet = Set( lQueues )
    rQueueSet = Set( rQueues )

    missedQueues = lQueueSet - rQueueSet
    extraQueues = rQueueSet - lQueueSet
    commonQueues = lQueueSet & rQueueSet

    if len( missedQueues ) >= 1:
        print >> sys.stderr, "The second config file misses " \
                             "the following queues:"
        print >> sys.stderr, "\n".join( missedQueues )
        retCode += 1

    if len( extraQueues ) >= 1:
        print >> sys.stderr, "The second config file has the following " \
                             "extra queues:"
        print >> sys.stderr, "\n".join( extraQueues )
        retCode += 1

    for qname in commonQueues:
        retCode += compareQueueValues( lhsConfig, rhsConfig, lClasses, rClasses,
                                       qname, noextra )

    return retCode


def getIniContent( iniID, location, nsAdminName ):
    """
    Provides a content of the NS ini file.
    Accepts location as:
    - file name
    - host:port[:effective value]
    """

    if ":" in location:
        # It is a NetSchedule server
        parts = location.split( ":" )
        if len( parts ) not in [ 2, 3 ]:
            raise Exception( "Unexpected format of the location spec. "
                             "Supported format: host:port[:effective value]" )
        host = parts[ 0 ]
        port = int( parts[ 1 ] )
        if len( parts ) > 2:
            effective = parts[ 2 ]
            if effective not in [ "0", "1" ]:
                raise Exception( "Unsupported effective value. "
                                 "Supported: 0 or 1" )
            if effective == "0":
                effective = False
            else:
                effective = True
        else:
            effective = False

        # Now connect to NS and get the config
        return getConfigFromNS( host, port, effective, nsAdminName )

    # It is a plain file
    if not os.path.exists( location ):
        raise Exception( "Cannot find NS ini file " + location )
    if not os.path.isfile( location ):
        raise Exception( "The NS ini file must be a file name. "
                         "The path " + location + " is not a file." )

    f = open( location, "r" )
    content = f.read()
    f.close()
    return content


def getConfigFromNS( host, port, effective, nsAdminName ):
    " Connects to NS and provides the config file content "

    grid_cli = ipc.RPCServer()
    server = ns.NetScheduleServer( host + ":" + str( port ),
                                   client_name = nsAdminName,
                                   rpc_server = grid_cli )
    server.allow_xsite_connections()

    if effective:
        # Check the NS version first
        serverInfo = server.get_server_info()
        version = serverInfo[ 'server_version' ]
        if StrictVersion( version ) < StrictVersion( "4.16.10" ):
            raise Exception( "Effective == 1 can only be applied to "
                             "NetSchedule 4.16.10 and up. The " +
                             host + ":" + str( port ) + " NetSchedule "
                             " has version " + version )
        cmd = "GETCONF effective=1"
    else:
        cmd = "GETCONF"

    output = server.execute( cmd, True )
    return "\n".join( output )


def parseConfigContent( content, nosed ):
    " Parses the given config content "

    config = ConfigParser()
    if nosed:
        config.readfp( StringIO( content ) )
    else:
        # The first sed prevent having 'class =' uncommented
        # The second sed uncomments the commented values
        tempDirName = tempfile.mkdtemp()
        if not tempDirName.endswith( os.path.sep ):
            tempDirName += os.path.sep
        tempFileName = tempDirName + "ns.ini"

        temporaryStorage = open( tempFileName, "w" )
        temporaryStorage.write( content )
        temporaryStorage.close()
        
        cmd = "cat " + tempFileName + \
              " | sed 's%^[ ]*;[ ]*\\(class[ ]*=\\)%;;;\\1%'" \
              " | sed 's%^[ ]*;[ ]*\\([a-zA-Z_][a-zA-Z_]*[ ]*=\\)%\\1%'"
        try:
            afterSed = check_output( cmd, shell = True )
        except:
            os.unlink( tempFileName )
            os.rmdir( tempDirName )
            raise

        os.unlink( tempFileName )
        os.rmdir( tempDirName )
        config.readfp( StringIO( afterSed ) )

    return config


def compareQueueValues( lConfig, rConfig, lClasses, rClasses, qname, noextra ):
    " Compares values in the queues respecting classes "
    retCode = 0
    try:
        lValues = getAllValues( lConfig, lClasses, qname )
    except Exception, exc:
        print >> sys.stderr, "The first config file is invalid: " + str( exc )
        return 1

    try:
        rValues = getAllValues( rConfig, rClasses, qname )
    except Exception, exc:
        print >> sys.stderr, "The second config file is invalid: " + str( exc )
        return 1

    # Here we have two lists of tuples. Common part is not interesting
    lSet = Set( lValues )
    rSet = Set( rValues )
    common = lSet & rSet

    # Compare the rest - could be different values or different variables
    lRestValues = tuplesToValues( lSet - common )
    rRestValues = tuplesToValues( rSet - common )

    missedValues = Set( lRestValues ) - Set( rRestValues ) - Set( [ 'class' ] )
    extraValues = Set( rRestValues ) - Set( lRestValues ) - Set( [ 'class' ] )
    commonValues = Set( lRestValues ) & Set( rRestValues )

    if len( missedValues ) >= 1:
        print >> sys.stderr, "The second config file queue " + qname + \
                             " misses the following options:"
        print >> sys.stderr, "\n".join( missedValues )
        retCode += 1
    if len( extraValues ) >= 1 and noextra == False:
        print >> sys.stderr, "The second config file queue " + qname + \
                             " has the following extra options:"
        print >> sys.stderr, "\n".join( extraValues )
        retCode += 1

    for optionName in commonValues:
        lVal = getVal( lSet, optionName )
        rVal = getVal( rSet, optionName )
        print >> sys.stderr, "The [" + qname + "]/" + optionName + \
                             " differs. First config file value: '" + lVal + \
                             "'. Second config value: '" + rVal + "'"
        retCode += 1
    return retCode


def getVal( tuples, name ):
    " Provides a value "
    for item in tuples:
        if item[ 0 ] == name:
            return item[ 1 ]
    raise Exception( "Internal inconsistency" )


def getAllValues( config, classes, queue ):
    " Provides combined set of values for the queue "
    if config.has_option( queue, 'class' ):
        # this is a queue with a class
        classSection = 'qclass_' + config.get( queue, 'class' )
        if not classSection in classes:
            raise Exception( "Queue " + queue + " references to undefined " \
                             "class " + config.get( queue, 'class' ) )

        # Merge values
        values = config.items( classSection )
        queueValues = config.items( queue )

        for val in queueValues:
            mergeValue( values, val )
        return values

    # The queue without a class
    return config.items( queue )


def mergeValue( values, another ):
    " Merges another value into values "
    if another[ 0 ] in tuplesToValues( values ):
        # Replace it
        for item in values:
            if item[ 0 ] == another[ 0 ]:
                values.remove( item )
                values.append( another )
                break
    else:
        values.append( another )
    return


def compareOtherSections( lConfig, rConfig, lOther, rOther ):
    " Compares and prints the difference of the other sections "

    retCode = 0
    lSet = Set( lOther )
    rSet = Set( rOther )

    missed = lSet - rSet
    extra = rSet - lSet
    common = lSet & rSet

    if len( missed ) >= 1:
        print >> sys.stderr, "The second config file misses " \
                             "the following sections:"
        print >> sys.stderr, "\n".join( missed )
        retCode += 1

    if len( extra ) >= 1:
        print >> sys.stderr, "The second config file has " \
                             "the following extra sections:"
        print >> sys.stderr, "\n".join( extra )
        retCode += 1

    for item in common:
        retCode += compareSectionItems( lConfig, rConfig, item )

    return retCode


def compareSectionItems( lConfig, rConfig, section ):
    " Compares the section items "

    retCode = 0
    lValues = Set( tuplesToValues( lConfig.items( section ) ) )
    rValues = Set( tuplesToValues( rConfig.items( section ) ) )

    missed = lValues - rValues
    extra = rValues - lValues
    common = lValues & rValues

    if len( missed ) >= 1:
        print >> sys.stderr, "The second config file section '" + section + \
                             "' misses the following values:"
        print >> sys.stderr, "\n".join( missed )
        retCode += 1

    if len( extra ) >= 1:
        print >> sys.stderr, "The second config file section '" + section + \
                             "' has the following extra values:"
        print >> sys.stderr, "\n".join( extra )
        retCode += 1

    for value in common:
        lVal = lConfig.get( section, value )
        rVal = rConfig.get( section, value )
        if lVal != rVal:
            print >> sys.stderr, "[" + section + "]/" + value + \
                                 " differs. The first file has '" + lVal + \
                                 "' while the second file has '" + rVal + "'"
            retCode += 1

    return retCode


def splitSections( sections ):
    " Splits all the sections from an .ini file into 3 parts "

    qclasses = []
    queues = []
    other = []

    for item in sections:
        if item.startswith( "qclass_" ):
            qclasses.append( item )
        elif item.startswith( "queue_" ):
            queues.append( item )
        else:
            other.append( item )
    return qclasses, queues, other


def removeQuotas( config, raw ):
    " Removes quotas around values if so "
    for section in config.sections():
        for option, value in config.items( section ):
            if value.startswith( '"' ) and value.endswith( '"' ):
                config.set( section, option, value[ 1 : -1 ] )
            elif value.startswith( "'" ) and value.endswith( "'" ):
                config.set( section, option, value[ 1 : -1 ] )
            if not raw:
                if config.get( section, option ).strip() == "":
                    config.remove_option( section, option )
    return


def parserError( parser, message ):
    " Prints the message and help on stderr "
    sys.stdout = sys.stderr
    print message
    parser.print_help()
    return 1


def tuplesToValues( src ):
    " Converts a list of tuples to a list of first values from tuples "
    res = []
    for item in src:
        res.append( item[ 0 ] )
    return res


def check_output(*popenargs, **kwargs):
    " Copied from Python 2.7 distribution and slightly modified "
    if 'stdout' in kwargs:
        raise ValueError('stdout argument not allowed, it will be overridden.')
    process = Popen(stdout=PIPE, *popenargs, **kwargs)
    output, unused_err = process.communicate()
    retcode = process.poll()
    if retcode:
        cmd = kwargs.get("args")
        if cmd is None:
            cmd = popenargs[0]
        raise Exception( "Command '" + cmd + "' returned non-zero status " + \
                         str( retcode ) )
    return output


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
