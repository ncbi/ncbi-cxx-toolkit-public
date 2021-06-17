#
# Authors: Sergey Satskiy
#
# $Id$
#

"""
NetScheduler Batch submit/drop loader
"""

import sys, time, threading
from ns_traffic_settings import BatchSubmitDropSettings


class BatchSubmitDropLoader( threading.Thread ):
    " Batch submit/drop loader "

    def __init__( self, gridClient, qname ):
        threading.Thread.__init__( self )
        self.__gridClient = gridClient
        self.__qname = qname
        self.__count = 0
        return

    def getName( self ):
        " Loader identification "
        return "Batch submit/drop"

    def getCount( self ):
        " Provides haw many loops completed "
        return self.__count

    def run( self ):
        " threaded function "
        pSize = BatchSubmitDropSettings.packageSize
        if pSize <= 0:
            print >> sys.stderr, \
                     "Invalid BatchSubmitDropSettings.packageSize (" + \
                     str( pSize ) + "). Must be > 0"
            return
        pause = BatchSubmitDropSettings.pause
        if pause < 0:
            print >> sys.stderr, \
                     "Invalid BatchSubmitDropSettings.pause (" + \
                     str( pause ) + "). Must be >= 0"
            return

        pCount = BatchSubmitDropSettings.packagesCount
        if not ( pCount == -1 or pCount > 0 ):
            print >> sys.stderr, \
                     "Invalid BatchSubmitDropSettings.packagesCount (" + \
                     str( pCount ) + "). Must be > 0 or -1"
            return

        batchSize = BatchSubmitDropSettings.jobsInBatch
        if batchSize <= 1:
            print >> sys.stderr, \
                     "Invalid BatchSubmitDropSettings.jobsInBatch (" + \
                     str( batchSize ) + "). Must be >= 2"
            return

        # Prepare jobs input
        jobsInput = []
        for count in xrange( batchSize ):
            jobsInput.append( '"bla"' )


        # Settings are OK
        while True:
            pSize = BatchSubmitDropSettings.packageSize
            while pSize > 0:
                jobKeys = []
                jobKey = ""
                try:
                    jobKeys = self.__gridClient.submitBatch( self.__qname,
                                                             jobsInput )
                    try:
                        for jobKey in jobKeys:
                            self.__gridClient.killJob( self.__qname, jobKey )
                    except Exception, excp:
                        print >> sys.stderr, \
                                 "Batch submit/Drop: Cannot kill job: " + jobKey
                        print >> sys.stderr, str( excp )
                except Exception, excp:
                    print >> sys.stderr, "Batch submit/Drop: Cannot submit jobs"
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

