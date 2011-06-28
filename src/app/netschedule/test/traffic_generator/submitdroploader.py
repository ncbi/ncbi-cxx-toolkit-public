#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
NetScheduler Submit/drop loader
"""

import sys, time, threading
from ns_traffic_settings import SubmitDropSettings


class SubmitDropLoader( threading.Thread ):
    " Submit/drop loader "

    def __init__( self, gridClient, qname ):
        threading.Thread.__init__( self )
        self.__gridClient = gridClient
        self.__qname = qname
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
                    jobKey = self.__gridClient.submitJob( self.__qname, "bla" )
                    try:
                        self.__gridClient.killJob( self.__qname, jobKey )
                    except Exception, excp:
                        print >> sys.stderr, \
                                 "Submit/Drop: Cannot kill job: " + jobKey
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

