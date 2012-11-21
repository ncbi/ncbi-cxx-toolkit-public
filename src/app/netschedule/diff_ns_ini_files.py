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


def main():
    " The utility entry point "

    parser = OptionParser(
    """
    %prog  <NS ini file>  <another NS ini file> [options]

    e.g. %prog  netscheduled.ini.old  netscheduled.ini
    NetSchedule configuration files comparing utility.
    It compares the given ini files and prints the diffwrence
    """ )
    parser.add_option( "-v", "--verbose",
                       action="store_true", dest="verbose", default=False,
                       help="be verbose (default: False)" )

    # parse the command line options
    options, args = parser.parse_args()
    verbose = options.verbose

    # Check the number of arguments
    if len( args ) != 2:
        return parserError( parser, "Incorrect number of arguments" )

    lhsIniFile = args[ 0 ]
    rhsIniFile = args[ 1 ]

    if not os.path.exists( lhsIniFile ):
        print >> sys.stderr, "Cannot find first NS ini file " + lhsIniFile
        return 1
    if not os.path.isfile( lhsIniFile ):
        print >> sys.stderr, "The first NS ini file must be a file name. " \
                             "The path " + lhsIniFile + " is not a file."
        return 1

    if not os.path.exists( rhsIniFile ):
        print >> sys.stderr, "Cannot find second NS ini file " + rhsIniFile
        return 1
    if not os.path.isfile( rhsIniFile ):
        print >> sys.stderr, "The second NS ini file must be a file name. " \
                             "The path " + rhsIniFile + " is not a file."
        return 1

    if verbose:
        print "First config file: " + lhsIniFile
        print "Second config file: " + rhsIniFile

    # Compose parsed configs
    lhsConfig = ConfigParser()
    lhsConfig.readfp( open( lhsIniFile ) )

    rhsConfig = ConfigParser()
    rhsConfig.readfp( open( rhsIniFile ) )

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
                                       qname )

    return retCode


def compareQueueValues( lConfig, rConfig, lClasses, rClasses, qname ):
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
    rSet = set( rValues )
    common = lSet & rSet

    # Compare the rest - could be different values or different variables
    lRestValues = tuplesToValues( lSet - common )
    rRestValues = tuplesToValues( rSet - common )

    missedValues = lRestValues - rRestValues
    extraValues = rRestValues - lRestValues
    commonValues = lRestValues & rRestValues

    if len( missedValues ) >= 1:
        print >> sys.stderr, "The second config file queue " + qname + \
                             " misses the following options:"
        print >> sys.stderr, "\n".join( missedValues )
        retCode += 1
    if len( extraValues ) >= 1:
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
                item[ 1 ] = another[ 1 ]
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
